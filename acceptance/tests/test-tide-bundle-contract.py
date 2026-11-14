#!/usr/bin/env python3
"""潮信随包契约：清单、bootstrap、C++ 启动器、前端必须用同一套文件清单与语义。

不联网、不启动程序，只读源码；任何一处漂移都会在这里失败。
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUNDLE_FILES = [
    "start.cmd",
    "src\\main.mjs",
    "vendor\\ws\\wrapper.mjs",
    "vendor\\silk-wasm\\silk.wasm",
    "native\\build\\hermes-qq-hook.dll",
    "native\\build\\hermes-qq-launcher.exe",
]
failures = []
checked = [0]


def check(cond, name):
    checked[0] += 1
    if cond:
        print(f"PASS: {name}")
    else:
        failures.append(name)
        print(f"FAIL: {name}")


def must(cond, name):
    if not cond:
        raise AssertionError(name)


components = json.loads((ROOT / "tools" / "components.json").read_text(encoding="utf-8"))
hermes = next(c for c in components["components"] if c["id"] == "hermes")

check(hermes["tier"] == "required" and hermes.get("default_install") is True, "hermes stays a required, default component")
check(hermes["source"]["kind"] == "bundled", "source.kind is bundled")
check(hermes["source"]["distribution"].find("下载队列") >= 0, "source declares it never enters the download queue")
check(all(s.get("kind") not in ("download", "npm_install") for s in hermes["install"]["steps"]), "install has no download/npm step")
check(hermes["install"]["steps"][0]["kind"] == "verify_tree", "install starts with local bundle verification")
check(hermes["install"]["steps"][0]["from"] == "{repo}\\sidecar\\hermes", "install verifies the installed sidecar copy")
check("node_modules" in hermes["install"]["steps"][0]["exclude"] and "state" in hermes["install"]["steps"][0]["exclude"], "bundle verification excludes runtime data")
check("-DataDir" in hermes["install"]["steps"][1]["args"] and "{state}\\hermes" in hermes["install"]["steps"][1]["args"], "init writes to the writable state directory")


def probe_files(probe):
    if probe.get("kind") == "file":
        return set(probe.get("paths") or [])
    if probe.get("kind") in ("all", "any"):
        out = set()
        for p in probe.get("probes") or []:
            out |= probe_files(p)
        return out
    return set()


detect_paths = probe_files(hermes["detect"])
check(all(("{sidecar}\\hermes\\" + f) in detect_paths for f in BUNDLE_FILES), "detect accepts a complete bundled sidecar tree")
check(all(("{bots}\\Hermes\\" + f) in detect_paths for f in BUNDLE_FILES), "detect still accepts a legacy bots tree")
verify_paths = probe_files(hermes["verify"])
check("{sidecar}\\node\\node.exe" in verify_paths, "verify requires the bundled Node")
check("{state}\\hermes\\config\\onebot.json" in verify_paths, "verify requires the initialized writable data dir")

tide = (ROOT / "win32" / "src" / "her" / "her_tide.cpp").read_text(encoding="utf-8")
for f in BUNDLE_FILES:
    check('L"%s"' % f.replace(chr(92), chr(92)*2) in tide, f"launcher checks {f}")
check("TideTreeOk(side)" in tide and tide.index("TideTreeOk(side)") < tide.index("BotPath(BRAND_HERMES_DIR)"), "launcher prefers the bundled tree before legacy bots")
check("TideTreeOk(legacy)" in tide, "legacy bots remains a fallback only")
check('L"HERMES_NODE", node' in tide and 'JoinPath(SidecarDir(), L"node")' in tide, "launcher pins HERMES_NODE to the bundled Node")
check('BRAND_ENV_HERMES_HOME, JoinPath(StateDir(), BRAND_HERMES)' in tide, "launcher keeps HERMES_HOME in the writable state dir")
check("请点「修复」" in tide and "重装时序之子" in tide, "launcher tells the user to repair/reinstall")
check(not re.search(r"找不到潮信的启动脚本", tide), "launcher no longer reports a missing script with no repair path")

setup = (ROOT / "tools" / "setup-hermes.ps1").read_text(encoding="utf-8-sig")
check("KAIROS_STATE_DIR" in setup and "APPDATA" in setup, "init lives in the writable state dir")
check("sidecar\\hermes" in setup and "随包的潮信文件不存在" in setup, "init prefers the bundled tree and fails with a repair hint")
check("迁移旧配置" in setup, "init migrates legacy bots data without overwriting")

boot = (ROOT / "tools" / "bootstrap.ps1").read_text(encoding="utf-8-sig")
check("'verify_tree'" in boot and "Assert-SourceTree" in boot, "bootstrap verifies bundled trees locally")
check("BundledIds" in boot and "Initialize-SetupMirror" in boot, "bundled components skip mirror setup")
check("随包文件校验未通过，请点「修复」或重装时序之子" in boot, "bundled failures never blame the network")
check("{state}" in boot, "bootstrap expands the state token")

web = (ROOT / "win32" / "web" / "setup.js").read_text(encoding="utf-8")
check("已随包" in web and "正在初始化" in web and "已就绪" in web, "setup page shows 已随包/正在初始化/已就绪")
check("bundled?button('修复','retry')+button('看日志','log')" in web, "bundled rows only offer repair")
check("if(!bundled&&item.state==='downloading'" in web, "bundled rows never show bytes/speed/ETA")
check("随安装包分发，不下载、不换镜像" in web, "bundled tooltip has no mirror wording")

tidejs = (ROOT / "win32" / "web" / "pages" / "tide.js").read_text(encoding="utf-8")
check("setup==='missing'" in tidejs and "setup==='initializing'" in tidejs, "tide page shares the bundle setup states")
check("data-setup=\"retry\" data-setup-id=\"hermes\"" in tidejs, "tide page offers the bundled repair action")
check("不用下载" in tidejs or "不用另装" in tidejs, "tide page makes clear nothing is downloaded")

try:
    must(not failures, "contract checks failed: " + ", ".join(failures))
except AssertionError as e:
    print(str(e))
    sys.exit(1)
print(f"SUMMARY total={checked[0]} failed={len(failures)}")
print("OK")
