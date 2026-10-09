import shutil
import subprocess
import sys
from collections import deque
from pathlib import Path
from typing import Dict, List, Optional

DST = Path(__file__).resolve().parents[1] / "third_party" / "ffmpeg" / "win64"
ROOTS = ["avcodec-62.dll", "avutil-60.dll", "swresample-6.dll"]
SYSTEM = {
    "kernel32.dll",
    "user32.dll",
    "gdi32.dll",
    "advapi32.dll",
    "shell32.dll",
    "ole32.dll",
    "oleaut32.dll",
    "ntdll.dll",
    "ws2_32.dll",
    "secur32.dll",
    "bcrypt.dll",
    "ucrtbase.dll",
    "msvcrt.dll",
    "winmm.dll",
    "imm32.dll",
    "comdlg32.dll",
    "comctl32.dll",
    "rpcrt4.dll",
    "shlwapi.dll",
    "setupapi.dll",
    "version.dll",
    "crypt32.dll",
    "iphlpapi.dll",
    "dwmapi.dll",
    "msimg32.dll",
    "userenv.dll",
    "bcryptprimitives.dll",
    "gdiplus.dll",
    "dnsapi.dll",
    "dwrite.dll",
    "usp10.dll",
}


def resolve(src: Path, name: str) -> Optional[Path]:
    direct = src / name
    if direct.exists():
        return direct
    matches = [p for p in src.glob("*.dll") if p.name.lower() == name.lower()]
    return matches[0] if matches else None


def dependents(dll: Path) -> List[str]:
    out = subprocess.check_output(["dumpbin", "/dependents", str(dll)], text=True, errors="ignore")
    names = []
    for line in out.splitlines():
        line = line.strip()
        if line.lower().endswith(".dll") and " " not in line and "\\" not in line:
            names.append(line)
    return names


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: vendor_ffmpeg_win64.py <ffmpeg-shared-dll-dir>")
        return 2
    src = Path(sys.argv[1])
    if not src.is_dir():
        print("not a directory:", src)
        return 1

    DST.mkdir(parents=True, exist_ok=True)
    for old in DST.glob("*.dll"):
        old.unlink()

    need = {}  # type: Dict[str, Path]
    q = deque(ROOTS)
    missing = []  # type: List[str]
    while q:
        name = q.popleft()
        key = name.lower()
        if key in need:
            continue
        path = resolve(src, name)
        if path is None:
            missing.append(name)
            continue
        need[key] = path
        for dep in dependents(path):
            dkey = dep.lower()
            if dkey.startswith("api-ms-") or dkey.startswith("ext-ms-") or dkey in SYSTEM:
                continue
            if dkey not in need:
                q.append(dep)

    for path in need.values():
        shutil.copy2(path, DST / path.name)

    size = sum(p.stat().st_size for p in DST.glob("*.dll"))
    print("copied %d dlls -> %s (%.1f MB)" % (len(need), DST, size / 1024 / 1024))
    if missing:
        print("missing:", ", ".join(missing))
    return 0


if __name__ == "__main__":
    sys.exit(main())
