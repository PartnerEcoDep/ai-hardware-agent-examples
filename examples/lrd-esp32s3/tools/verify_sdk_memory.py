#!/usr/bin/env python3
"""ConvAI SDK 内存占用验证脚本（空载 vs 带SDK 差值法）

用法（在 PowerShell / bash 中，需已激活 ESP-IDF 环境）：
    python tools/verify_sdk_memory.py

原理（设计文档 delta-design.md §9）：
    ConvAI SDK 内存 = 带SDK固件内存 - 空载固件内存

流程：
    1. CONFIG_CONVAI_ENABLE=n  → 构建空载固件，解析 map 文件 .bss 段
    2. CONFIG_CONVAI_ENABLE=y  → 构建带SDK固件，解析 map 文件 .bss 段
    3. 输出 BSS 差值（静态内存增量）与判定

注意：
    - 动态堆差值（esp_get_free_heap_size）需要真机串口读数，
      本脚本只做 map 文件静态分析 + 构建自动化；
    - 真机 heap 差值请按 delta-design §9.2 手动记录。
"""

import re
import subprocess
import sys
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_DIR / "build"
MAP_FILE = BUILD_DIR / "ai_hardware_agent_esp32.map"
LIMIT_BYTES = 102400  # 100 KB

# idf.py is a Python script (no .exe on Windows); invoke via the interpreter
# that the project was configured with.  Reading it from CMakeCache.txt keeps
# the build-dir python identical to the one ESP-IDF recorded, avoiding the
# "currently active in the environment while the project was configured with"
# mismatch (which can trigger when the venv is reached via a junction/symlink
# under a different path).
import json
import os
import re as _re
import sys

CACHE_FILE = BUILD_DIR / "CMakeCache.txt"


def _resolve_idf_python() -> str:
    # 1) Prefer the python recorded in CMakeCache.txt (matches the build dir).
    if CACHE_FILE.exists():
        m = _re.search(r"^PYTHON:UNINITIALIZED=(.+)$", CACHE_FILE.read_text(encoding="utf-8"), _re.MULTILINE)
        if m:
            return m.group(1).strip()
    # 2) Fall back to the interpreter running this script.
    return sys.executable


def _resolve_idf_py() -> str:
    """Locate the ESP-IDF idf.py script without hard-coding a path.

    Detection order:
      1. $IDF_PATH/tools/idf.py        — set by ESP-IDF's export.ps1/export.sh
      2. A globally-installed idf.py    — 'idf.py' on PATH (venv Scripts shim)
      3. Common install roots           — D:\\esp\\<ver>\\esp-idf, ~/esp/<ver>/esp-idf
    """
    # 1) IDF_PATH environment variable (authoritative after export.ps1).
    idf_path = os.environ.get("IDF_PATH")
    if idf_path:
        cand = Path(idf_path) / "tools" / "idf.py"
        if cand.exists():
            return str(cand)

    # 2) idf.py already on PATH (the ESP-IDF venv provides a shim).
    which = __import__("shutil").which("idf.py")
    if which:
        return which

    # 3) Fall back to scanning common install roots for <root>/esp-idf/tools/idf.py.
    roots = []
    for env in ("IDF_TOOLS_PATH", "ESP_IDF_PATH"):
        if os.environ.get(env):
            roots.append(Path(os.environ[env]))
    roots += [Path("D:/esp"), Path.home() / "esp"]
    for root in roots:
        if root.is_dir():
            for sub in sorted(root.iterdir()):
                cand = sub / "esp-idf" / "tools" / "idf.py"
                if cand.exists():
                    return str(cand)

    raise SystemExit(
        "Cannot locate ESP-IDF idf.py. Activate the ESP-IDF environment first "
        "(run export.ps1) or set IDF_PATH."
    )


IDF_PY = [_resolve_idf_python(), _resolve_idf_py()]


def set_convai_enable(enabled: bool) -> None:
    """直接改 sdkconfig 的 CONFIG_CONVAI_ENABLE（idf.py 无 set-defaults 命令）。"""
    sdkconfig = PROJECT_DIR / "sdkconfig"
    if not sdkconfig.exists():
        raise SystemExit(f"sdkconfig not found: {sdkconfig}")
    text = sdkconfig.read_text(encoding="utf-8")
    if enabled:
        # 启用：确保行是 CONFIG_CONVAI_ENABLE=y
        if re.search(r"^#?\s*CONFIG_CONVAI_ENABLE", text, re.MULTILINE):
            text = re.sub(
                r"^#?\s*CONFIG_CONVAI_ENABLE.*$",
                "CONFIG_CONVAI_ENABLE=y",
                text,
                flags=re.MULTILINE,
            )
        elif "CONFIG_CONVAI_ENABLE=y" not in text:
            text += "\nCONFIG_CONVAI_ENABLE=y\n"
    else:
        # 禁用：替换为注释形式
        if re.search(r"^CONFIG_CONVAI_ENABLE=y$", text, re.MULTILINE):
            text = re.sub(
                r"^CONFIG_CONVAI_ENABLE=y$",
                "# CONFIG_CONVAI_ENABLE is not set",
                text,
                flags=re.MULTILINE,
            )
    sdkconfig.write_text(text, encoding="utf-8")


def _run_idf(args: list) -> str:
    """Run an idf.py subcommand inside a PowerShell child with the ESP-IDF
    environment activated.

    Using PowerShell + export.ps1 (instead of invoking IDF_PY directly) keeps
    the environment identical to a normal build and avoids the junction/symlink
    python-path mismatch that breaks direct subprocess launches from a venv
    shell.  Returns combined stdout+stderr.
    """
    # Prepend `idf.py` to the subcommand list.
    cmd = " ".join(f'"{a}"' if " " in a else a for a in ["idf.py"] + args)
    pwsh = (
        # Align python with what the build dir was configured with: the
        # ESP-IDF venv is reached via a junction (python_env -> tools\python),
        # so export.ps1 records the junction path in CMakeCache.  Force
        # IDF_PYTHON_ENV_PATH to that exact junction path and drop the real
        # venv dir from PATH, otherwise idf.py sees two different pythons
        # and aborts with the "currently active in the environment while the
        # project was configured with" error.
        r'$env:IDF_TOOLS_PATH="D:\Espressif"; '
        # Use the REAL venv path (not the python_env junction): the junction was
        # created after some sessions started, and Windows path-cache can make
        # it invisible (Test-Path=False) in those old sessions even though the
        # file exists for new processes.  activate.py with the real path passes
        # all checks (verified 2026-09-14).
        r'$env:IDF_PYTHON_ENV_PATH="D:\Espressif\tools\python\v6.0.2\venv"; '
        r'$p_venv=$env:VIRTUAL_ENV; $p_home=$env:PYTHONHOME; $p_pypath=$env:PYTHONPATH; $p_idf=$env:IDF_PATH; $p_conda=$env:CONDA_PREFIX; '
        r'Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue; '
        r'Remove-Item Env:MSYSTEM_CHERE -ErrorAction SilentlyContinue; '
        r'Remove-Item Env:VIRTUAL_ENV -ErrorAction SilentlyContinue; '
        r'Remove-Item Env:PYTHONHOME -ErrorAction SilentlyContinue; '
        r'Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue; '
        r'Write-Output ("[dbg] parent VIRTUAL_ENV=[" + $p_venv + "] PYTHONHOME=[" + $p_home + "] PYTHONPATH=[" + $p_pypath + "] IDF_PATH=[" + $p_idf + "] CONDA_PREFIX=[" + $p_conda + "]"); '
        # Drop MSYS/MinGW dirs (their GNU size.exe would shadow xtensa size)
        # and the real venv dir (avoid python path mismatch above).  Keep the
        # rest so ninja/git/cmake remain visible; export.ps1 prepends IDF tools.
        # NOTE: use -notlike (wildcards), NOT -notmatch (regex): backslashes
        # in a regex would be mis-parsed by PowerShell (e.g. \t -> \p{X}).
        r'$env:PATH=($env:PATH -split ";" | Where-Object { '
        r'  $_ -notlike "*msys64*" -and $_ -notlike "*mingw64*" -and '
        r'  $_ -notlike "*Espressif\tools\python*" -and '
        r'  $_ -notlike "*Espressif\python_env*" '
        r'}) -join ";"; '
        r'Write-Output ("[dbg] child python=" + (Get-Command python).Source); '
        r'Write-Output ("[dbg] venv py exists=" + (Test-Path "D:\Espressif\python_env\idf6.0_py3.11_env\Scripts\python.exe")); '
        r'& "D:\esp\v6.0.2\esp-idf\export.ps1" | Out-Null; '
        # Disable ccache: concurrent runs / stale locks cause ninja
        # "failed recompaction: Permission denied".
        r'$env:CCACHE_DISABLE="1"; '
        + cmd
    )
    # Windows PowerShell emits GBK-codepage text for Chinese output; decode
    # leniently (errors=replace) instead of failing on non-UTF-8 bytes.
    last_err = ""
    import time as _time
    for attempt in range(5):  # retry: antivirus/Defender transiently locks files
        proc = subprocess.run(
            ["powershell", "-NoProfile", "-Command", pwsh],
            cwd=PROJECT_DIR, capture_output=True,
            encoding="utf-8", errors="replace",
        )
        if proc.returncode == 0:
            return proc.stdout
        last_err = (proc.stderr or "")[-2000:] + (proc.stdout or "")[-2000:]
        if "Permission denied" in last_err or "recompaction" in last_err:
            print(f"  !! Permission denied (attempt {attempt+1}/5), waiting 3s ...")
            _time.sleep(3)
            continue
        break
    raise SystemExit(
        f"idf.py {' '.join(args)} failed (rc={proc.returncode}):\n{last_err}"
    )


def build_firmware(convai_enabled: bool) -> None:
    """切 CONFIG_CONVAI_ENABLE 并构建。

    切换 CONVAI_ENABLE 会改变 CMake 的源文件集合（sdk/*.c 条件编译），
    ninja 的增量构建图基于旧配置生成，直接 build 可能用错源列表。
    因此每次切换后显式 reconfigure（快）再 build。
    """
    value = "y" if convai_enabled else "n"
    print(f"[1/2] Building firmware with CONFIG_CONVAI_ENABLE={value} ...")
    set_convai_enable(convai_enabled)
    _run_idf(["reconfigure"])
    _run_idf(["build"])


def get_mem_layout() -> dict:
    """Parse the firmware memory layout from the link map.

    Uses the ESP-IDF `esp_idf_size` Python module directly on the map file —
    no idf.py / PATH / PowerShell dependency (the module ships in the ESP-IDF
    venv).  Returns a dict keyed by segment name (Flash Code / Flash Data /
    DIRAM / IRAM / RTC FAST / RTC SLOW / ...) with the `used` byte count.

    This distinguishes **internal RAM** (DIRAM + IRAM) from flash (code+data)
    and PSRAM-backed runtime allocations, which is what the 100KB subsystem
    budget really targets.
    """
    if not MAP_FILE.exists():
        print(f"  !! map not found: {MAP_FILE}")
        return {}
    proc = subprocess.run(
        [IDF_PY[0], "-m", "esp_idf_size", "--format", "json2", str(MAP_FILE)],
        cwd=PROJECT_DIR, capture_output=True, text=True, errors="replace",
    )
    if proc.returncode != 0:
        raise SystemExit(f"esp_idf_size failed:\n{proc.stderr[-2000:]}")
    data = json.loads(proc.stdout)
    layout = {}
    for seg in data.get("layout", []):
        name = seg.get("name", "?")
        layout[name] = {
            "used": seg.get("used", 0),
            "total": seg.get("total", 0),
        }
    return layout


def _fmt_layout(layout: dict) -> str:
    """一行格式化布局摘要: DIRAM/IRAM/FlashCode/FlashData used。"""
    def _seg(name):
        return layout.get(name, {}).get("used", 0)
    return (f"DIRAM={_seg('DIRAM')/1024:.1f}K "
            f"IRAM={_seg('IRAM')/1024:.1f}K "
            f"FlashCode={_seg('Flash Code')/1024:.1f}K "
            f"FlashData={_seg('Flash Data')/1024:.1f}K")


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(
        description="ConvAI SDK 内存差值验证（空载 vs 带SDK）"
    )
    parser.add_argument(
        "--fullclean",
        action="store_true",
        help="构建前先 idf.py fullclean（彻底清理，慢；默认只 reconfigure）",
    )
    args = parser.parse_args()

    print("=== ConvAI SDK 内存验证（空载 vs 带SDK 差值法） ===\n")

    if args.fullclean:
        print("[0/2] Running idf.py fullclean ...")
        subprocess.run(IDF_PY + ["fullclean"], cwd=PROJECT_DIR, check=True)

    # 1. 空载基线
    build_firmware(convai_enabled=False)
    empty_sz = get_mem_layout()
    print(f"  空载固件: " + _fmt_layout(empty_sz))

    # 2. 带SDK
    build_firmware(convai_enabled=True)
    sdk_sz = get_mem_layout()
    print(f"  带SDK固件: " + _fmt_layout(sdk_sz))

    # 3. 差值（SDK 真实贡献），按内存类型分类
    print(f"\n=== 结果（SDK 贡献 = 带SDK - 空载） ===")

    # 内部 RAM = DIRAM + IRAM（ESP32-S3 真正宝贵的快速内存）
    def _seg_sum(sz, names):
        return sum(sz.get(n, {}).get("used", 0) for n in names)

    internal_ram_empty = _seg_sum(empty_sz, ("DIRAM", "IRAM"))
    internal_ram_sdk = _seg_sum(sdk_sz, ("DIRAM", "IRAM"))
    flash_code_empty = _seg_sum(empty_sz, ("Flash Code",))
    flash_code_sdk = _seg_sum(sdk_sz, ("Flash Code",))
    flash_data_empty = _seg_sum(empty_sz, ("Flash Data",))
    flash_data_sdk = _seg_sum(sdk_sz, ("Flash Data",))

    d_ram = _seg_sum(sdk_sz, ("DIRAM",)) - _seg_sum(empty_sz, ("DIRAM",))
    i_ram = _seg_sum(sdk_sz, ("IRAM",)) - _seg_sum(empty_sz, ("IRAM",))
    d_code = flash_code_sdk - flash_code_empty
    d_rodata = flash_data_sdk - flash_data_empty

    internal_ram_delta = internal_ram_sdk - internal_ram_empty

    print(f"  DIRAM     内部数据RAM   增量 = {d_ram:+d} bytes ({d_ram/1024:+.1f} KB)")
    print(f"  IRAM      内部代码RAM   增量 = {i_ram:+d} bytes ({i_ram/1024:+.1f} KB)")
    print(f"  FlashCode 代码段(flash) 增量 = {d_code:+d} bytes ({d_code/1024:+.1f} KB)")
    print(f"  FlashData 只读数据(flash)增量 = {d_rodata:+d} bytes ({d_rodata/1024:+.1f} KB)")
    print(f"  {'TOTAL':8s} 内部RAM(DIRAM+IRAM) 增量 = {internal_ram_delta:+d} bytes ({internal_ram_delta/1024:+.1f} KB)")
    print(f"  判定阈值 = {LIMIT_BYTES} bytes (100 KB)")
    print(f"  注意：运行期 heap 动态分配（引擎/TLS/编解码实例/播放环）需真机读取 "
          f"esp_get_free_heap_size 差值；此处为编译期静态布局\n")

    # 判定：设计文档 100KB 目标是"子系统内存"（RAM 中的静态+动态）。
    # 内部 RAM（DIRAM+IRAM）静态增量是编译期最接近预算的部分。
    if internal_ram_delta <= LIMIT_BYTES:
        print(f"  [PASS] 内部RAM静态增量 {internal_ram_delta}B <= {LIMIT_BYTES}B")
        return 0
    else:
        print(f"  [WARN] 内部RAM静态增量 {internal_ram_delta}B > {LIMIT_BYTES}B")
        print(f"         注意：heap 动态分配（播放环等）可能在 PSRAM，需真机确认。")
        return 1


if __name__ == "__main__":
    sys.exit(main())
