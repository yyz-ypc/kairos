// 心核 P0 · SQLite 存储（系统自带 winsqlite3.dll，可写）
#include "heart.h"

#include <windows.h>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

namespace heart {

namespace {

typedef int (*FnOpen)(const char*, void**, int, const char*);
typedef int (*FnClose)(void*);
typedef int (*FnPrepare)(void*, const char*, int, void**, const char**);
typedef int (*FnStep)(void*);
typedef int (*FnFinalize)(void*);
typedef const unsigned char* (*FnColText)(void*, int);
typedef const char* (*FnErrmsg)(void*);
typedef int (*FnBindText)(void*, int, const char*, int, void (*)(void*));
typedef long long (*FnLastId)(void*);

struct Api {
    HMODULE dll = nullptr;
    FnOpen open_v2 = nullptr;
    FnClose close_v2 = nullptr;
    FnPrepare prepare_v2 = nullptr;
    FnStep step = nullptr;
    FnFinalize finalize = nullptr;
    FnColText col_text = nullptr;
    FnErrmsg errmsg = nullptr;
    FnBindText bind_text = nullptr;
    FnLastId last_id = nullptr;

    bool Load() {
        if (dll) return open_v2 != nullptr;
        const wchar_t* names[] = {L"winsqlite3.dll", L"sqlite3.dll"};
        for (auto* n : names) {
            dll = LoadLibraryW(n);
            if (dll) break;
        }
        if (!dll) return false;
        open_v2 = (FnOpen)GetProcAddress(dll, "sqlite3_open_v2");
        close_v2 = (FnClose)GetProcAddress(dll, "sqlite3_close_v2");
        prepare_v2 = (FnPrepare)GetProcAddress(dll, "sqlite3_prepare_v2");
        step = (FnStep)GetProcAddress(dll, "sqlite3_step");
        finalize = (FnFinalize)GetProcAddress(dll, "sqlite3_finalize");
        col_text = (FnColText)GetProcAddress(dll, "sqlite3_column_text");
        errmsg = (FnErrmsg)GetProcAddress(dll, "sqlite3_errmsg");
        bind_text = (FnBindText)GetProcAddress(dll, "sqlite3_bind_text");
        last_id = (FnLastId)GetProcAddress(dll, "sqlite3_last_insert_rowid");
        return open_v2 && close_v2 && prepare_v2 && step && finalize && col_text && errmsg && bind_text && last_id;
    }
};

Api& SqliteApi() {
    static Api api;
    static bool tried = false;
    if (!tried) {
        tried = true;
        api.Load();
    }
    return api;
}

std::string Narrow(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring Widen(const char* s) {
    if (!s || !*s) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (n <= 1) return {};
    std::wstring out(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), n);
    return out;
}

std::wstring Quote(const std::wstring& s) {
    std::wstring q = L"'";
    for (wchar_t c : s) {
        if (c == L'\'') q += L'\'';
        q += c;
    }
    q += L'\'';
    return q;
}

bool ExecRaw(void* db, const std::wstring& sql, std::wstring* err) {
    Api& a = SqliteApi();
    std::string utf8 = Narrow(sql);
    const char* tail = utf8.c_str();
    while (tail && *tail) {
        void* st = nullptr;
        if (a.prepare_v2(db, tail, -1, &st, &tail) != 0) {
            if (err) *err = Widen(a.errmsg(db));
            return false;
        }
        if (!st) break;
        int rc = a.step(st);
        a.finalize(st);
        if (rc != 101 /*SQLITE_DONE*/ && rc != 100 /*SQLITE_ROW*/) {
            if (err) *err = Widen(a.errmsg(db));
            return false;
        }
    }
    return true;
}

}  // namespace

struct Store::Impl {
    void* db = nullptr;
    std::wstring path;
    std::wstring last_err;
};

Store::Store() : impl_(new Impl) {}
Store::~Store() { Close(); }

bool Store::Ok() const { return impl_ && impl_->db != nullptr; }
std::wstring Store::Path() const { return impl_ ? impl_->path : L""; }

bool Store::Open(const std::wstring& file, std::wstring* err) {
    Close();
    Api& a = SqliteApi();
    if (!a.Load()) {
        if (err) *err = L"系统里没有 winsqlite3.dll";
        return false;
    }
    std::string path = Narrow(file);
    void* db = nullptr;
    int rc = a.open_v2(path.c_str(), &db, 0x00000002 | 0x00000004 | 0x00010000 /* RW|CREATE|FULLMUTEX */,
                       nullptr);
    if (rc != 0 || !db) {
        if (err) *err = db ? Widen(a.errmsg(db)) : L"打不开心核数据库";
        if (db) a.close_v2(db);
        return false;
    }
    impl_->db = db;
    impl_->path = file;
    const wchar_t* schema =
        L"PRAGMA journal_mode=WAL;"
        L"CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);"
        L"CREATE TABLE IF NOT EXISTS sessions(id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL,"
        L" created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL);"
        L"CREATE TABLE IF NOT EXISTS messages(id INTEGER PRIMARY KEY AUTOINCREMENT, session_id INTEGER NOT NULL,"
        L" role TEXT NOT NULL, content TEXT NOT NULL, created_at INTEGER NOT NULL);"
        L"CREATE INDEX IF NOT EXISTS idx_messages_session ON messages(session_id, id);";
    if (!Exec(schema, err)) {
        Close();
        return false;
    }
    return true;
}

void Store::Close() {
    if (!impl_) return;
    if (impl_->db) SqliteApi().close_v2(impl_->db);
    impl_->db = nullptr;
}

bool Store::Exec(const std::wstring& sql, std::wstring* err) {
    if (!Ok()) {
        if (err) *err = L"数据库没有打开";
        return false;
    }
    if (ExecRaw(impl_->db, sql, err)) return true;
    return false;
}

bool Store::LastError(std::wstring* err) const {
    if (!err || !impl_) return false;
    *err = impl_->last_err;
    return !err->empty();
}

bool Store::GetMeta(const std::wstring& key, std::wstring* value) {
    if (!Ok() || !value) return false;
    Api& a = SqliteApi();
    std::string sql = Narrow(L"SELECT value FROM meta WHERE key=" + Quote(key));
    void* st = nullptr;
    if (a.prepare_v2(impl_->db, sql.c_str(), -1, &st, nullptr) != 0 || !st) return false;
    bool found = false;
    if (a.step(st) == 100) {
        *value = Widen((const char*)a.col_text(st, 0));
        found = true;
    }
    a.finalize(st);
    return found;
}

bool Store::SetMeta(const std::wstring& key, const std::wstring& value, std::wstring* err) {
    std::wstring sql = L"INSERT INTO meta(key,value) VALUES(" + Quote(key) + L"," + Quote(value) +
                       L") ON CONFLICT(key) DO UPDATE SET value=excluded.value;";
    return Exec(sql, err);
}

bool Store::AddSession(const std::wstring& title, long long* id, std::wstring* err) {
    long long now = (long long)std::time(nullptr);
    std::wstring sql = L"INSERT INTO sessions(title,created_at,updated_at) VALUES(" + Quote(title) + L"," +
                       std::to_wstring(now) + L"," + std::to_wstring(now) + L");";
    if (!Exec(sql, err)) return false;
    if (id) *id = SqliteApi().last_id(impl_->db);
    return true;
}

bool Store::AddMessage(long long session, const std::wstring& role, const std::wstring& text, long long* id,
                       std::wstring* err) {
    long long now = (long long)std::time(nullptr);
    std::wstring sql = L"INSERT INTO messages(session_id,role,content,created_at) VALUES(" +
                       std::to_wstring(session) + L"," + Quote(role) + L"," + Quote(text) + L"," +
                       std::to_wstring(now) + L");";
    if (!Exec(sql, err)) return false;
    if (id) *id = SqliteApi().last_id(impl_->db);
    return true;
}

bool Store::CountRows(const std::wstring& table, long long* n) {
    if (!Ok() || !n) return false;
    if (table != L"sessions" && table != L"messages" && table != L"meta") return false;
    Api& a = SqliteApi();
    std::string sql = Narrow(L"SELECT COUNT(*) FROM " + table);
    void* st = nullptr;
    if (a.prepare_v2(impl_->db, sql.c_str(), -1, &st, nullptr) != 0 || !st) return false;
    bool ok = false;
    if (a.step(st) == 100) {
        *n = (long long)// sqlite3_column_int64 is optional; text parse keeps the loader small
            _wtoi64(Widen((const char*)a.col_text(st, 0)).c_str());
        ok = true;
    }
    a.finalize(st);
    return ok;
}

}  // namespace heart
