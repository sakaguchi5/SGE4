from __future__ import annotations

from hashlib import sha256
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "SOURCE_MANIFEST.sha256"
EXCLUDED_DIRECTORIES = {"build", ".vs"}
EXCLUDED_SUFFIXES = {".user", ".suo", ".VC.db", ".VC.opendb"}


def included(path: Path) -> bool:
    relative = path.relative_to(ROOT)
    if relative.as_posix() == MANIFEST.name:
        return False
    if any(part in EXCLUDED_DIRECTORIES for part in relative.parts[:-1]):
        return False
    name = relative.name
    return not any(name.endswith(suffix) for suffix in EXCLUDED_SUFFIXES)


entries: list[str] = []
for path in sorted((path for path in ROOT.rglob("*") if path.is_file() and included(path)),
                   key=lambda value: value.relative_to(ROOT).as_posix()):
    digest = sha256(path.read_bytes()).hexdigest()
    entries.append(f"{digest}  {path.relative_to(ROOT).as_posix()}")

MANIFEST.write_text("\n".join(entries) + "\n", encoding="utf-8", newline="\n")
print(f"SOURCE_MANIFESTを生成しました。登録ファイル数：{len(entries)}")
