#!/usr/bin/env python3
"""Fabric topology analysis library.

This module provides pure analysis functions for the Fabric build toolchain:

- YAML parsing (fabric.yaml, kraft.yaml)
- Fabric topology extraction (compartments, fabrics, bridge edges)
- Source scanning (function definitions, flexos_nop_gate call sites, exportsyms)
- Cross-fabric call resolution
- Registration plan building

It does NOT generate any output files.  Code generation lives in
``auto_register.py``, which imports from this module.

The module is intentionally self-contained (no external YAML dependency).
"""

from __future__ import annotations

import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

# ---------------------------------------------------------------------------
# Optional YAML support
# ---------------------------------------------------------------------------
try:
    import yaml  # type: ignore
except ImportError:
    yaml = None

# ---------------------------------------------------------------------------
# Constants & regex patterns
# ---------------------------------------------------------------------------
SYMBOL_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.S)
LINE_COMMENT_RE = re.compile(r"//.*?$", re.M)

# flexos_nop_gate(key_from, key_to, func, ...)
NOP_GATE_RE = re.compile(
    r"flexos_nop_gate\s*\(\s*[^,]+\s*,\s*[^,]+\s*,\s*([A-Za-z_][A-Za-z0-9_]*)",
    re.S,
)
# flexos_nop_gate_r(key_from, key_to, ret, func, ...)
NOP_GATE_R_RE = re.compile(
    r"flexos_nop_gate_r\s*\(\s*[^,]+\s*,\s*[^,]+\s*,\s*[^,]+\s*,\s*([A-Za-z_][A-Za-z0-9_]*)",
    re.S,
)

# Existing fabric_gate / fabric_gate_r targets (to avoid double-rewriting)
FABRIC_GATE_RE = re.compile(
    r"fabric_gate\s*\(\s*[^,]+\s*,\s*([A-Za-z_][A-Za-z0-9_]*)",
    re.S,
)
FABRIC_GATE_R_RE = re.compile(
    r"fabric_gate_r\s*\(\s*[^,]+\s*,\s*[^,]+\s*,\s*([A-Za-z_][A-Za-z0-9_]*)",
    re.S,
)

MACRO_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(", re.M)
FUNC_HEAD_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\)\s*$")
ADDLIB_RE = re.compile(r"addlib\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")

# 隔离后端：fabric.yaml isolation.backend 值 → (C 枚举名, C 配置类型名)
ISO_MAP: dict[str, tuple[str, str]] = {
    "mpk":       ("FABRIC_ISO_MPK",       "MPKConfig"),
    "ept":       ("FABRIC_ISO_EPT",       "EPTConfig"),
    "trustzone": ("FABRIC_ISO_TRUSTZONE", "MPKConfig"),
    "software":  ("FABRIC_ISO_SOFTWARE",  "MPKConfig"),
}


# ===================================================================
#  简易 YAML 解析器（未安装 PyYAML 时使用）
# ===================================================================

def _yaml_indent(line: str) -> int:
    return len(line) - len(line.lstrip())


def _yaml_scalar(value: str) -> Any:
    value = value.strip()
    if value in {"true", "True", "yes", "on"}:
        return True
    if value in {"false", "False", "no", "off"}:
        return False
    if value in {"null", "~", ""}:
        return None
    if len(value) >= 2 and value[0] in {"'", '"'} and value[-1] == value[0]:
        return value[1:-1]
    try:
        return int(value)
    except ValueError:
        pass
    try:
        return float(value)
    except ValueError:
        pass
    return value


def _yaml_parse_list(lines: list[str], start: int, base_indent: int):
    result: list[Any] = []
    idx = start
    while idx < len(lines):
        line = lines[idx]
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            idx += 1
            continue
        indent = _yaml_indent(line)
        if indent < base_indent:
            break
        if not stripped.startswith("- "):
            break
        item = stripped[2:].strip()
        # Inline list: [a, b]
        if item.startswith("[") and item.endswith("]"):
            result.append(
                [part.strip().strip("'\"") for part in item[1:-1].split(",")]
            )
            idx += 1
            continue
        result.append(_yaml_scalar(item))
        idx += 1
    return result, idx


def _yaml_parse_block(lines: list[str], start: int, base_indent: int):
    result: dict[str, Any] = {}
    idx = start
    while idx < len(lines):
        line = lines[idx]
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            idx += 1
            continue
        indent = _yaml_indent(line)
        if indent < base_indent:
            break
        # List item at current level → delegate
        if stripped.startswith("- "):
            return _yaml_parse_list(lines, idx, indent)
        if ":" not in stripped:
            idx += 1
            continue
        key, _, value = stripped.partition(":")
        key = key.strip().strip("'\"")
        value = value.strip()
        if value:
            result[key] = _yaml_scalar(value)
            idx += 1
            continue
        # Child block
        child_indent = None
        peek = idx + 1
        while peek < len(lines):
            peek_line = lines[peek].strip()
            if peek_line and not peek_line.startswith("#"):
                child_indent = _yaml_indent(lines[peek])
                break
            peek += 1
        if child_indent is not None and child_indent > indent:
            child, idx = _yaml_parse_block(lines, peek, child_indent)
            result[key] = child
        else:
            result[key] = None
            idx += 1
    return result, idx


def _mini_yaml_load(text: str) -> dict[str, Any]:
    lines: list[str] = []
    for raw in text.splitlines():
        stripped = raw.strip()
        if stripped in {"---", "..."}:
            continue
        lines.append(raw)
    return _yaml_parse_block(lines, 0, 0)[0]


def load_yaml(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8")
    if yaml is not None:
        return yaml.safe_load(text) or {}
    return _mini_yaml_load(text)


# ===================================================================
#  工具函数
# ===================================================================

def strip_comments(text: str) -> str:
    return LINE_COMMENT_RE.sub("", BLOCK_COMMENT_RE.sub("", text))


def normalise_lib_name(raw: str) -> str:
    """将库名标准化为 fabric.yaml / kraft.yaml 中使用的格式。"""
    name = raw.strip().strip("/ ")
    if name.startswith("lib"):
        name = name[3:]
    name = name.replace("-", "_").replace("/", "_")
    if name in {"c", "libc"}:
        return "newlib"
    return name


def c_safe(name: str) -> str:
    """将名称转换为 C 安全标识符（横线 → 下划线）。"""
    return name.replace("-", "_")


def fabric_core_rel(out_dir: Path, lib_root: Path) -> str:
    """Compute the relative path from *out_dir* to the fabric-core directory."""
    fc = lib_root / "fabric-core"
    try:
        return fc.relative_to(out_dir).as_posix()
    except ValueError:
        return os.path.relpath(fc, out_dir).replace("\\", "/")


# ===================================================================
#  数据结构
# ===================================================================

@dataclass(frozen=True)
class FunctionInfo:
    name: str
    lib: str          # normalised library name
    kind: str         # "function" | "inline" | "macro"
    registerable: bool
    path: str


@dataclass(frozen=True)
class CallSite:
    """A single flexos_nop_gate / flexos_nop_gate_r call site."""
    caller_lib: str
    callee_lib: str
    func: str
    path: Path
    has_ret: bool
    registerable: bool


@dataclass
class AnalysisResult:
    """Aggregated result of topology + source analysis."""
    # 配置
    fabric_cfg: dict[str, Any]
    isolation_backend: str
    app_lib_name: str
    app_runtime_lib: str
    default_compartment: str
    # 拓扑
    fabric_names: list[str]
    fabric_libs: dict[str, list[str]]
    edges: list[tuple[str, str]]
    lib_fabric: dict[str, int]
    default_fabrics: dict[str, str]
    default_fabric: int
    # 索引
    function_index: dict[str, FunctionInfo]
    export_index: dict[str, str]
    # 调用点
    call_sites: list[tuple[str, str, Path, bool]]
    cross_calls: list[CallSite]
    # 注册计划
    reg_plan: dict[str, list[str]]


# ===================================================================
#  fabric.yaml → 拓扑解析
# ===================================================================

def parse_compartments(fabric_cfg: dict[str, Any]):
    """返回 (comp_name, comp_dict) 列表。支持单层和多隔间布局。"""
    if "fabrics" in fabric_cfg:
        return [("comp0", fabric_cfg)]
    result = []
    for key, value in fabric_cfg.items():
        if isinstance(value, dict) and "fabrics" in value:
            result.append((key, value))
    return result


def order_fabrics(fabrics_cfg: dict[str, Any]) -> list[str]:
    """返回 fabric 名称列表，``default: true`` 的排在最前面。"""
    default_name = None
    for name, cfg in fabrics_cfg.items():
        if isinstance(cfg, dict) and cfg.get("default"):
            default_name = name
            break
    ordered: list[str] = []
    if default_name:
        ordered.append(default_name)
    for name in fabrics_cfg:
        if name != default_name:
            ordered.append(name)
    return ordered


def collect_fabric_layout(fabric_cfg: dict[str, Any]):
    """
    Returns
    -------
    fabric_names : list[str]          – ordered global fabric names
    fabric_libs  : dict[str, list[str]] – fabric → normalised lib names
    edges        : list[(str, str)]   – bridge edges (global names)
    lib_fabric   : dict[str, int]     – lib → fabric index (into *fabric_names*)
    """
    compartments = parse_compartments(fabric_cfg)
    is_flat = len(compartments) == 1 and compartments[0][0] == "comp0"

    fabric_names: list[str] = []
    fabric_libs: dict[str, list[str]] = {}
    edges: list[tuple[str, str]] = []
    lib_fabric: dict[str, int] = {}

    idx = 0
    for comp_name, comp_def in compartments:
        fabrics_cfg = comp_def.get("fabrics", {}) or {}
        local_to_global: dict[str, str] = {}

        for fname in order_fabrics(fabrics_cfg):
            global_name = fname if is_flat else f"{comp_name}_{fname}"
            local_to_global[fname] = global_name
            fabric_names.append(global_name)

            libs: list[str] = []
            fabric_def = fabrics_cfg.get(fname, {}) or {}
            for entry in fabric_def.get("libs", []) or []:
                raw_lib = entry.get("name", "") if isinstance(entry, dict) else str(entry)
                norm_lib = normalise_lib_name(raw_lib)
                libs.append(norm_lib)
                lib_fabric[norm_lib] = idx
            fabric_libs[global_name] = libs
            idx += 1

        for edge in comp_def.get("edges", []) or []:
            if isinstance(edge, (list, tuple)) and len(edge) == 2:
                left = local_to_global.get(str(edge[0]))
                right = local_to_global.get(str(edge[1]))
                if left and right:
                    edges.append((left, right))

    return fabric_names, fabric_libs, edges, lib_fabric


def build_default_fabric_map(fabric_cfg: dict[str, Any]) -> dict[str, str]:
    """将每个隔间映射到其默认 fabric 的全局名称。"""
    compartments = parse_compartments(fabric_cfg)
    is_flat = len(compartments) == 1 and compartments[0][0] == "comp0"
    default_fabrics: dict[str, str] = {}
    for comp_name, comp_def in compartments:
        fabrics_cfg = comp_def.get("fabrics", {}) or {}
        ordered = order_fabrics(fabrics_cfg)
        if not ordered:
            continue
        local_name = ordered[0]
        global_name = local_name if is_flat else f"{comp_name}_{local_name}"
        default_fabrics[comp_name] = global_name
    return default_fabrics


# ===================================================================
#  kraft.yaml → 库与隔间映射
# ===================================================================

def load_kraft_defaults(app_dir: Path):
    """
    Returns
    -------
    app_runtime_lib : str
    default_compartment : str
    lib_compartments : dict[str, str]   – normalised lib → compartment name
    """
    kraft_path = app_dir / "kraft.yaml"
    if not kraft_path.exists():
        fallback = re.sub(r"[^A-Za-z0-9]", "", app_dir.name)
        return f"app{fallback.lower()}", "comp0", {}

    kraft_cfg = load_yaml(kraft_path)
    app_runtime_lib = normalise_lib_name(str(kraft_cfg.get("name") or app_dir.name))

    compartments = kraft_cfg.get("compartments") or []
    default_compartment = "comp0"
    if isinstance(compartments, list) and compartments:
        first_name = compartments[0].get("name") if isinstance(compartments[0], dict) else None
        if first_name:
            default_compartment = str(first_name)
        for comp in compartments:
            if isinstance(comp, dict) and comp.get("default"):
                name = comp.get("name")
                if name:
                    default_compartment = str(name)
                break

    lib_compartments: dict[str, str] = {}
    libraries = kraft_cfg.get("libraries") or {}
    if isinstance(libraries, dict):
        for raw_lib, data in libraries.items():
            compartment = default_compartment
            if isinstance(data, dict) and data.get("compartment"):
                compartment = str(data["compartment"])
            lib_compartments[normalise_lib_name(str(raw_lib))] = compartment

    return app_runtime_lib, default_compartment, lib_compartments


def load_isolation_config(fabric_cfg: dict[str, Any]) -> str:
    """从 fabric.yaml 加载隔离后端配置。

    返回后端名称（如 "mpk"、"ept"、"trustzone"、"software"）。
    未指定时默认使用 "ept"。
    """
    iso = fabric_cfg.get("isolation") or {}
    backend = iso.get("backend") if isinstance(iso, dict) else None
    return str(backend) if backend else "ept"


def load_app_lib_name(app_dir: Path) -> str:
    makefile = app_dir / "Makefile.uk"
    if makefile.exists():
        text = makefile.read_text(encoding="utf-8", errors="replace")
        match = ADDLIB_RE.search(text)
        if match:
            return match.group(1)
    fallback = re.sub(r"[^A-Za-z0-9]", "", app_dir.name)
    return f"app{fallback.lower()}"


# ===================================================================
#  源文件扫描工具
# ===================================================================

def find_source_files(root: Path | None) -> list[Path]:
    if root is None or not root.exists():
        return []
    files: list[Path] = []
    for pattern in ("*.c", "*.h"):
        files.extend(sorted(root.rglob(pattern)))
    return files


def find_app_source_files(app_dir: Path) -> list[Path]:
    files: list[Path] = []
    for pattern in ("*.c", "*.h"):
        for path in sorted(app_dir.rglob(pattern)):
            rel_parts = path.relative_to(app_dir).parts
            if rel_parts and rel_parts[0] in {"helper", "build"}:
                continue
            files.append(path)
    return files


def find_fetched_lib_sources(app_dir: Path | None) -> list[tuple[Path, str]]:
    """Return fetched upstream sources under ``build/lib*/origin``."""
    if app_dir is None:
        return []
    build_dir = app_dir / "build"
    if not build_dir.is_dir():
        return []

    files: list[tuple[Path, str]] = []
    for lib_build in sorted(build_dir.glob("lib*")):
        origin = lib_build / "origin"
        if not origin.is_dir():
            continue
        lib_name = normalise_lib_name(lib_build.name)
        for pattern in ("*.c", "*.h"):
            for path in sorted(origin.rglob(pattern)):
                files.append((path, lib_name))
    return files


def iter_function_defs(text: str):
    """遍历 *text* 中的每个函数定义，返回 (name, prefix_before_name)。"""
    buf = ""
    depth = 0
    for raw_line in text.splitlines():
        stripped = raw_line.strip()
        if not stripped:
            continue
        if depth == 0 and stripped.startswith("#"):
            continue
        open_count = stripped.count("{")
        close_count = stripped.count("}")
        if depth == 0:
            buf = f"{buf} {stripped}".strip()
            if open_count == 0:
                if stripped.endswith(";") or len(buf) > 400:
                    buf = ""
            else:
                head = buf.split("{", 1)[0].strip()
                buf = ""
                if ";" not in head and "(" in head and ")" in head:
                    # 清除 __attribute__((...)) 以避免误匹配
                    clean = re.sub(r"__attribute__\s*\(\s*\([^)]*\)\s*\)", "", head)
                    clean = re.sub(r"__attribute__\s*\(\s*[^)]*\)", "", clean)
                    match = FUNC_HEAD_RE.search(clean)
                    if match:
                        name = match.group(1)
                        prefix = head[: head.rfind(name)].strip()
                        yield name, prefix
        depth += open_count - close_count
        if depth < 0:
            depth = 0
        if depth == 0 and close_count > 0:
            buf = ""


# ===================================================================
#  构建函数索引
# ===================================================================

def _best_info(existing: FunctionInfo | None, candidate: FunctionInfo) -> FunctionInfo:
    if existing is None:
        return candidate
    score = {"function": 3, "inline": 2, "macro": 1}
    if score[candidate.kind] > score[existing.kind]:
        return candidate
    if score[candidate.kind] == score[existing.kind] and candidate.registerable and not existing.registerable:
        return candidate
    return existing


def build_function_index(
    lib_root: Path,
    ext_lib_root: Path | None,
    app_dir: Path | None = None,
    app_lib_name: str | None = None,
) -> dict[str, FunctionInfo]:
    """函数名 → FunctionInfo 映射（最佳匹配优先）。"""
    index: dict[str, FunctionInfo] = {}

    for root in [lib_root, ext_lib_root]:
        if root is None or not root.exists():
            continue
        for src in find_source_files(root):
            rel = src.relative_to(root)
            lib = normalise_lib_name(rel.parts[0])
            text = strip_comments(src.read_text(encoding="utf-8", errors="replace"))
            for match in MACRO_RE.finditer(text):
                name = match.group(1)
                info = FunctionInfo(name=name, lib=lib, kind="macro", registerable=False, path=str(src))
                index[name] = _best_info(index.get(name), info)
            for name, prefix in iter_function_defs(text):
                if name in {"if", "for", "while", "switch", "return"}:
                    continue
                is_inline = bool(re.search(r'(?<![a-zA-Z])inline(?![a-zA-Z])', prefix)) or src.suffix == ".h"
                is_static = " static" in f" {prefix} "
                kind = "inline" if is_inline else "function"
                registerable = kind == "function" and not is_static
                info = FunctionInfo(name=name, lib=lib, kind=kind, registerable=registerable, path=str(src))
                index[name] = _best_info(index.get(name), info)

    if app_dir is not None and app_lib_name is not None:
        for src in find_app_source_files(app_dir):
            text = strip_comments(src.read_text(encoding="utf-8", errors="replace"))
            for match in MACRO_RE.finditer(text):
                name = match.group(1)
                info = FunctionInfo(name=name, lib=app_lib_name, kind="macro", registerable=False, path=str(src))
                index[name] = _best_info(index.get(name), info)
            for name, prefix in iter_function_defs(text):
                if name in {"if", "for", "while", "switch", "return"}:
                    continue
                is_inline = bool(re.search(r'(?<![a-zA-Z])inline(?![a-zA-Z])', prefix)) or src.suffix == ".h"
                is_static = " static" in f" {prefix} "
                kind = "inline" if is_inline else "function"
                registerable = kind == "function" and not is_static
                info = FunctionInfo(name=name, lib=app_lib_name, kind=kind, registerable=registerable, path=str(src))
                index[name] = _best_info(index.get(name), info)

    for src, lib in find_fetched_lib_sources(app_dir):
        text = strip_comments(src.read_text(encoding="utf-8", errors="replace"))
        for match in MACRO_RE.finditer(text):
            name = match.group(1)
            info = FunctionInfo(name=name, lib=lib, kind="macro", registerable=False, path=str(src))
            index[name] = _best_info(index.get(name), info)
        for name, prefix in iter_function_defs(text):
            if name in {"if", "for", "while", "switch", "return"}:
                continue
            is_inline = bool(re.search(r'(?<![a-zA-Z])inline(?![a-zA-Z])', prefix)) or src.suffix == ".h"
            is_static = " static" in f" {prefix} "
            kind = "inline" if is_inline else "function"
            registerable = kind == "function" and not is_static
            info = FunctionInfo(name=name, lib=lib, kind=kind, registerable=registerable, path=str(src))
            index[name] = _best_info(index.get(name), info)

    return index


# ===================================================================
#  exportsym.uk 导出符号索引
# ===================================================================

def parse_exports_file(path: Path) -> list[str]:
    symbols: list[str] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].split("//", 1)[0].strip()
        if not line:
            continue
        token = line.split()[0]
        if SYMBOL_RE.match(token):
            symbols.append(token)
    return symbols


def build_export_index(lib_root: Path, ext_lib_root: Path | None) -> dict[str, str]:
    """导出符号 → 库名映射。"""
    index: dict[str, str] = {}
    for root in [lib_root, ext_lib_root]:
        if root is None or not root.exists():
            continue
        for path in sorted(root.glob("*/exportsyms.uk")):
            lib = normalise_lib_name(path.parent.name)
            for symbol in parse_exports_file(path):
                index.setdefault(symbol, lib)
    return index


# ===================================================================
#  调用点收集
# ===================================================================

def collect_call_sites(
    lib_root: Path,
    ext_lib_root: Path | None,
    app_dir: Path | None = None,
    app_lib_name: str | None = None,
) -> list[tuple[str, str, Path, bool]]:
    """返回 [(caller_lib, callee_func_name, source_path, has_ret), ...]。"""
    sites: list[tuple[str, str, Path, bool]] = []
    for root in [lib_root, ext_lib_root]:
        if root is None or not root.exists():
            continue
        for src in find_source_files(root):
            rel = src.relative_to(root)
            caller_lib = normalise_lib_name(rel.parts[0])
            text = strip_comments(src.read_text(encoding="utf-8", errors="replace"))
            for match in NOP_GATE_RE.finditer(text):
                sites.append((caller_lib, match.group(1), src, False))
            for match in NOP_GATE_R_RE.finditer(text):
                sites.append((caller_lib, match.group(1), src, True))
            # fabric_gate 形式（Coccinelle 重写后，libs/ 内也需要扫描）
            for match in FABRIC_GATE_RE.finditer(text):
                sites.append((caller_lib, match.group(1), src, False))
            for match in FABRIC_GATE_R_RE.finditer(text):
                sites.append((caller_lib, match.group(1), src, True))
    if app_dir is not None and app_lib_name is not None:
        for src in find_app_source_files(app_dir):
            text = strip_comments(src.read_text(encoding="utf-8", errors="replace"))
            for match in NOP_GATE_RE.finditer(text):
                sites.append((app_lib_name, match.group(1), src, False))
            for match in NOP_GATE_R_RE.finditer(text):
                sites.append((app_lib_name, match.group(1), src, True))
            # fabric_gate 形式（Coccinelle 重写后）
            for match in FABRIC_GATE_RE.finditer(text):
                sites.append((app_lib_name, match.group(1), src, False))
            for match in FABRIC_GATE_R_RE.finditer(text):
                sites.append((app_lib_name, match.group(1), src, True))
    for src, caller_lib in find_fetched_lib_sources(app_dir):
        text = strip_comments(src.read_text(encoding="utf-8", errors="replace"))
        for match in NOP_GATE_RE.finditer(text):
            sites.append((caller_lib, match.group(1), src, False))
        for match in NOP_GATE_R_RE.finditer(text):
            sites.append((caller_lib, match.group(1), src, True))
        for match in FABRIC_GATE_RE.finditer(text):
            sites.append((caller_lib, match.group(1), src, False))
        for match in FABRIC_GATE_R_RE.finditer(text):
            sites.append((caller_lib, match.group(1), src, True))
    return sites


def collect_app_gate_targets(app_dir: Path) -> set[str]:
    funcs: set[str] = set()
    for src in sorted(app_dir.rglob("*.c")) + sorted(app_dir.rglob("*.h")):
        rel_parts = src.relative_to(app_dir).parts
        if rel_parts and rel_parts[0] in {"helper", "build"}:
            continue
        text = strip_comments(src.read_text(encoding="utf-8", errors="replace"))
        for match in FABRIC_GATE_RE.finditer(text):
            funcs.add(match.group(1))
        for match in FABRIC_GATE_R_RE.finditer(text):
            funcs.add(match.group(1))
    return funcs


def collect_existing_fabric_gate_targets(lib_root: Path, ext_lib_root: Path | None) -> set[str]:
    funcs: set[str] = set()
    for root in [lib_root, ext_lib_root]:
        if root is None or not root.exists():
            continue
        for src in find_source_files(root):
            text = strip_comments(src.read_text(encoding="utf-8", errors="replace"))
            for match in FABRIC_GATE_RE.finditer(text):
                funcs.add(match.group(1))
            for match in FABRIC_GATE_R_RE.finditer(text):
                funcs.add(match.group(1))
    return funcs


# ===================================================================
#  跨 fabric 调用解析
# ===================================================================

def resolve_cross_calls(
    call_sites: list[tuple[str, str, Path, bool]],
    lib_fabric: dict[str, int],
    function_index: dict[str, FunctionInfo],
    export_index: dict[str, str],
    default_fabric: int,
) -> list[CallSite]:
    result: list[CallSite] = []
    seen: set[tuple[str, str, str, bool]] = set()

    for caller_lib, func, path, has_ret in call_sites:
        finfo = function_index.get(func)
        export_lib = export_index.get(func)

        callee_lib: str | None = None
        if export_lib is not None:
            callee_lib = export_lib
        elif finfo is not None:
            callee_lib = finfo.lib

        if callee_lib is None:
            continue

        caller_fabric = lib_fabric.get(caller_lib, default_fabric)
        callee_fabric = lib_fabric.get(callee_lib, default_fabric)

        if caller_fabric is None or callee_fabric is None or caller_fabric == callee_fabric:
            continue

        registerable = bool(export_lib is not None or (finfo and finfo.registerable))
        key = (str(path), caller_lib, func, has_ret)
        if key in seen:
            continue
        seen.add(key)
        result.append(CallSite(caller_lib, callee_lib, func, path, has_ret, registerable))

    return result


# ===================================================================
#  注册计划
# ===================================================================

def build_registration_plan(
    cross_calls: list[CallSite],
    function_index: dict[str, FunctionInfo],
    export_index: dict[str, str],
    lib_fabric: dict[str, int],
    default_fabric: int,
) -> dict[str, list[str]]:
    """Map callee library → list of function names that need registration."""
    by_lib: dict[str, set[str]] = {}

    for call in cross_calls:
        if not call.registerable:
            continue
        caller_fabric = lib_fabric.get(call.caller_lib, default_fabric)
        callee_fabric = lib_fabric.get(call.callee_lib, default_fabric)
        if caller_fabric == callee_fabric:
            continue
        finfo = function_index.get(call.func)
        export_lib = export_index.get(call.func)
        if finfo is None and export_lib is None:
            continue
        by_lib.setdefault(call.callee_lib, set()).add(call.func)

    return {lib: sorted(funcs) for lib, funcs in sorted(by_lib.items())}


# ===================================================================
#  隐式库 → fabric 绑定
# ===================================================================

def assign_implicit_lib_fabrics(
    app_lib_name: str,
    default_compartment: str,
    kraft_lib_compartments: dict[str, str],
    fabric_names: list[str],
    fabric_libs: dict[str, list[str]],
    lib_fabric: dict[str, int],
    default_fabrics: dict[str, str],
) -> int:
    """Bind implicit libraries (app lib, kraft-declared libs) to fabrics."""
    if not fabric_names:
        return 0

    fabric_index = {name: idx for idx, name in enumerate(fabric_names)}
    fallback_fabric = default_fabrics.get(default_compartment, fabric_names[0])
    fallback_idx = fabric_index[fallback_fabric]

    def bind(lib_name: str, fabric_name: str | None) -> None:
        if lib_name in lib_fabric:
            return
        target = fabric_name or fallback_fabric
        idx = fabric_index.get(target, fallback_idx)
        resolved_target = fabric_names[idx]
        lib_fabric[lib_name] = idx
        libs = fabric_libs.setdefault(resolved_target, [])
        if lib_name not in libs:
            libs.append(lib_name)

    bind(app_lib_name, default_fabrics.get(default_compartment, fallback_fabric))
    for lib_name, compartment in sorted(kraft_lib_compartments.items()):
        bind(lib_name, default_fabrics.get(compartment, fallback_fabric))

    return fallback_idx


# ===================================================================
#  统一分析入口
# ===================================================================

def analyze(
    app_dir: Path,
    lib_root: Path,
    ext_lib_root: Path | None = None,
) -> AnalysisResult:
    """Run the full topology + source analysis and return an ``AnalysisResult``.

    This is the single entry point that ``auto_register.py`` and the build
    script call.  It replaces the duplicated analysis code that used to live
    in both ``parse_config.main()`` and ``auto_register.main()``.
    """
    fabric_yaml = app_dir / "fabric.yaml"
    if not fabric_yaml.exists():
        raise FileNotFoundError(f"{fabric_yaml} not found")

    fabric_cfg = load_yaml(fabric_yaml)
    isolation_backend = load_isolation_config(fabric_cfg)
    app_lib_name = load_app_lib_name(app_dir)
    app_runtime_lib, default_compartment, kraft_lib_compartments = load_kraft_defaults(app_dir)

    fabric_names, fabric_libs, edges, lib_fabric = collect_fabric_layout(fabric_cfg)
    default_fabrics = build_default_fabric_map(fabric_cfg)

    default_fabric = assign_implicit_lib_fabrics(
        app_lib_name, default_compartment, kraft_lib_compartments,
        fabric_names, fabric_libs, lib_fabric, default_fabrics,
    )

    function_index = build_function_index(lib_root, ext_lib_root, app_dir, app_lib_name)
    export_index = build_export_index(lib_root, ext_lib_root)

    call_sites = collect_call_sites(lib_root, ext_lib_root, app_dir, app_lib_name)
    cross_calls = resolve_cross_calls(
        call_sites, lib_fabric, function_index, export_index, default_fabric,
    )

    reg_plan = build_registration_plan(
        cross_calls, function_index, export_index, lib_fabric, default_fabric
    )

    return AnalysisResult(
        fabric_cfg=fabric_cfg,
        isolation_backend=isolation_backend,
        app_lib_name=app_lib_name,
        app_runtime_lib=app_runtime_lib,
        default_compartment=default_compartment,
        fabric_names=fabric_names,
        fabric_libs=fabric_libs,
        edges=edges,
        lib_fabric=lib_fabric,
        default_fabrics=default_fabrics,
        default_fabric=default_fabric,
        function_index=function_index,
        export_index=export_index,
        call_sites=call_sites,
        cross_calls=cross_calls,
        reg_plan=reg_plan,
    )
