#!/usr/bin/env python3
"""恢复被 Coccinelle 重写的源文件。

将 fabric_gate / fabric_gate_r 调用还原为 flexos_nop_gate / flexos_nop_gate_r，
使得下一轮 kraft build --compartmentalize 能正常实例化。

用法: python3 restore_fabric_sources.py <app-dir> [lib-root] [ext-lib-root]

扫描范围:
  - <app-dir> 下的 .c/.h 文件（排除 helper/ 和 build/）
  - <lib-root> 下的 .c/.h 文件（unikraft/lib/）
  - <ext-lib-root> 下的 .c/.h 文件（libs/）
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

_parse_config_dir = str(Path(__file__).resolve().parent.parent / "parse_config")
if _parse_config_dir not in sys.path:
    sys.path.insert(0, _parse_config_dir)

from parse_config import find_fetched_lib_sources

# fabric_gate(src_fabric, func, ...)
GATE_RE = re.compile(
    r"fabric_gate\s*\(\s*([0-9]+)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*(?:,\s*(.*))?\)",
    re.S,
)
# fabric_gate_r(src_fabric, ret, func, ...)
GATE_R_RE = re.compile(
    r"fabric_gate_r\s*\(\s*([0-9]+)\s*,\s*([^,]+)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*(?:,\s*(.*))?\)",
    re.S,
)


def restore_file(path: Path) -> bool:
    """恢复单个文件中的 fabric_gate 调用。返回是否修改。"""
    text = path.read_text(encoding="utf-8", errors="replace")
    original = text

    def replace_gate_r(m: re.Match) -> str:
        # fabric_gate_r(src, ret, func, args...) → flexos_nop_gate_r(0, 0, ret, func, args...)
        ret = m.group(2).strip()
        func = m.group(3)
        args = m.group(4)
        if args:
            return f"flexos_nop_gate_r(0, 0, {ret}, {func}, {args})"
        return f"flexos_nop_gate_r(0, 0, {ret}, {func})"

    def replace_gate(m: re.Match) -> str:
        # fabric_gate(src, func, args...) → flexos_nop_gate(0, 0, func, args...)
        func = m.group(2)
        args = m.group(3)
        if args:
            return f"flexos_nop_gate(0, 0, {func}, {args})"
        return f"flexos_nop_gate(0, 0, {func})"

    # 先替换 fabric_gate_r（更长的模式优先）
    text = re.sub(r"fabric_gate_r\s*\(\s*\d+\s*,", "flexos_nop_gate_r(0, 0,", text)
    # 再替换 fabric_gate
    text = re.sub(r"fabric_gate\s*\(\s*\d+\s*,", "flexos_nop_gate(0, 0,", text)

    if text != original:
        path.write_text(text, encoding="utf-8")
        return True
    return False


def restore_makefile_uk(lib_root: Path, app_dir: Path | None = None) -> int:
    """恢复 Makefile.uk：重新启用 flexos_extra.ld，移除 fabric 条目。"""
    restored = 0
    # 恢复库的 Makefile.uk
    for makefile in sorted(lib_root.rglob("Makefile.uk")):
        text = makefile.read_text(encoding="utf-8", errors="replace")
        original = text
        # 重新启用被注释掉的 flexos_extra.ld
        text = text.replace(
            "#  # 已禁用：由 fabric linker script 替代",
            ""
        )
        # 移除 fabric linker script 条目
        text = re.sub(
            r"\n# Fabric section rename for.*\nLIB.*_LDFLAGS-y \+= -Wl,-T,.*lib_.*_fabric\.ld\n",
            "\n",
            text,
        )
        if text != original:
            makefile.write_text(text, encoding="utf-8")
            restored += 1
    # 恢复 app 的 Makefile.uk
    if app_dir:
        app_makefile = app_dir / "Makefile.uk"
        if app_makefile.exists():
            text = app_makefile.read_text(encoding="utf-8", errors="replace")
            original = text
            # 移除 helper 引用
            text = re.sub(r"\n.*SRCS-y \+= .*/helper/fabric_init\.c\n", "\n", text)
            text = re.sub(r"\n.*SRCS-y \+= .*/helper/register_all\.c\n", "\n", text)
            text = re.sub(r"\n.*CINCLUDES-y \+= -I.*/helper\n", "\n", text)
            # 移除 --defsym 条目
            text = re.sub(
                r"\nLDFLAGS-y \+= .*_fabric_comp1_fabric_[0-9]+.*\n",
                "\n",
                text,
            )
            text = re.sub(
                r"\nLDFLAGS-y \+= -Wl,--defsym=_fabric_[^\n]*\n",
                "\n",
                text,
            )
            if text != original:
                app_makefile.write_text(text, encoding="utf-8")
                restored += 1
    return restored


def find_files(roots: list[Path], app_dir: Path | None = None) -> list[Path]:
    """Collect .c/.h files, including fetched upstream sources."""
    files: list[Path] = []
    for root in roots:
        if root is None or not root.exists():
            continue
        for pattern in ("*.c", "*.h"):
            for path in sorted(root.rglob(pattern)):
                if app_dir is not None:
                    try:
                        parts = path.relative_to(app_dir).parts
                        if parts and parts[0] in {"helper", "build"}:
                            continue
                    except ValueError:
                        pass
                files.append(path)
    if app_dir is not None:
        for path, _lib in find_fetched_lib_sources(app_dir):
            files.append(path)
    return files


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: restore_fabric_sources.py <app-dir> [lib-root] [ext-lib-root]",
              file=sys.stderr)
        return 1

    app_dir = Path(sys.argv[1]).resolve()
    lib_root = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
    ext_lib_root = Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else None

    # 收集要扫描的目录
    roots: list[Path] = []
    if app_dir.exists():
        roots.append(app_dir)
    if lib_root and lib_root.exists():
        roots.append(lib_root)
    if ext_lib_root and ext_lib_root.exists():
        roots.append(ext_lib_root)

    files = find_files(roots, app_dir)
    app_root = app_dir if app_dir.exists() else None
    restored = 0
    for f in files:
        if restore_file(f):
            print(f"  [restore] {f}")
            restored += 1

    # 恢复 Makefile.uk（重新启用 flexos_extra.ld，移除 fabric 条目）
    roots_to_restore: list[Path] = []
    if lib_root and lib_root.exists():
        roots_to_restore.append(lib_root)
    if ext_lib_root and ext_lib_root.exists():
        roots_to_restore.append(ext_lib_root)
    for root in roots_to_restore:
        mk_restored = restore_makefile_uk(root, app_dir if root == lib_root else None)
        if mk_restored:
            print(f"[restore] {mk_restored} Makefile.uk file(s) restored under {root}")

    # 清除 per-component 软件加固 CFLAG 补丁
    hardening_root = Path(__file__).resolve().parent.parent / "hardening"
    if hardening_root.exists():
        sys.path.insert(0, str(hardening_root))
        try:
            from apply_hardening import apply_makefile_hardening, apply_kconfig
            repo_root = app_dir.parent.parent if app_dir.name == "redis" else app_dir.parent
            if not (repo_root / "unikraft").exists():
                repo_root = app_dir.parent.parent
            apply_kconfig(app_dir, [])
            apply_makefile_hardening(repo_root, app_dir, [])
        except Exception as exc:
            print(f"[restore] hardening cleanup skipped: {exc}", file=sys.stderr)

    print(f"[restore] {restored} source file(s) restored")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
