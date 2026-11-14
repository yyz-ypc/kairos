// 心核 P0 · 进程内事件总线
#include "heart.h"

#include <map>
#include <mutex>
#include <vector>

namespace heart {

namespace {

std::mutex g_mu;
int g_next = 1;
std::map<int, std::pair<std::wstring, std::function<void(const her::J&)>>> g_subs;

}  // namespace

int Subscribe(const std::wstring& topic, std::function<void(const her::J&)> handler) {
    if (!handler) return 0;
    std::lock_guard<std::mutex> lk(g_mu);
    int id = g_next++;
    g_subs[id] = {topic, std::move(handler)};
    return id;
}

void Unsubscribe(int id) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_subs.erase(id);
}

int Publish(const std::wstring& topic, const her::J& payload) {
    std::vector<std::function<void(const her::J&)>> hits;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        for (auto& kv : g_subs) {
            if (kv.second.first == topic) hits.push_back(kv.second.second);
        }
    }
    for (auto& fn : hits) fn(payload);
    return (int)hits.size();
}

}  // namespace heart
