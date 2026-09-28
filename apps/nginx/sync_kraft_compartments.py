#!/usr/bin/env python3
"""Sync kraft.yaml VMEPT compartments to match fabric.yaml domain layout.

n01 monolith (1 fabric) → COMP_COUNT=1
n02/n05/n06/n07 (2 fabrics) → COMP_COUNT=2 (nginx|lwip)
n03/n04 (3 fabrics) → COMP_COUNT=3 (nginx|uksched|lwip)

Middle fabric often only lists uksched (unikraft-internal). Kraft drops
unused non-default compartments, so we inject uksched/ukschedcoop as
is_core kraft libraries into that compartment (localdir → unikraft/lib/).
"""
from __future__ import annotations

import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    print("PyYAML required", file=sys.stderr)
    sys.exit(1)


def order_fabrics(fabrics: dict) -> list[str]:
    ordered: list[str] = []
    default = None
    for name, cfg in (fabrics or {}).items():
        if isinstance(cfg, dict) and cfg.get("default"):
            default = name
            break
    if default:
        ordered.append(default)
    for name in fabrics or {}:
        if name != default:
            ordered.append(name)
    return ordered


def fabric_libs(fabrics: dict, name: str) -> set[str]:
    cfg = (fabrics or {}).get(name) or {}
    out: set[str] = set()
    for entry in cfg.get("libs") or []:
        if isinstance(entry, dict):
            out.add(str(entry.get("name") or ""))
        else:
            out.add(str(entry))
    return {x for x in out if x}


# Unikraft-core sched libs: kraft needs is_core so localdir resolves under UK_ROOT.
CORE_PLACEHOLDER_LIBS = ("uksched", "ukschedcoop")


def _is_core_placeholder(data: dict) -> bool:
    """True if entry looks like our injected core placeholder (no version)."""
    if not isinstance(data, dict):
        return False
    keys = set(data.keys())
    return keys <= {"compartment", "is_core"} and data.get("is_core") is True


def sync(app_dir: Path) -> int:
    fabric_path = app_dir / "fabric.yaml"
    kraft_path = app_dir / "kraft.yaml"
    fabric = yaml.safe_load(fabric_path.read_text()) or {}
    kraft = yaml.safe_load(kraft_path.read_text()) or {}

    fabrics_cfg = None
    if "fabrics" in fabric:
        fabrics_cfg = fabric["fabrics"]
    else:
        for v in fabric.values():
            if isinstance(v, dict) and "fabrics" in v:
                fabrics_cfg = v["fabrics"]
                break
    if not fabrics_cfg:
        print("[sync-kraft] no fabrics in fabric.yaml", file=sys.stderr)
        return 1

    names = order_fabrics(fabrics_cfg)
    n = len(names)
    if n < 1:
        print("[sync-kraft] empty fabrics", file=sys.stderr)
        return 1

    comps = []
    for i in range(n):
        comps.append(
            {
                "name": f"comp{i + 1}",
                "mechanism": {
                    "driver": "vmept",
                    "noisolstack": i != 0,
                },
                **({"default": True} if i == 0 else {}),
            }
        )
    kraft["compartments"] = comps

    lib_to_comp: dict[str, str] = {}
    for i, fname in enumerate(names):
        cname = f"comp{i + 1}"
        for lib in fabric_libs(fabrics_cfg, fname):
            lib_to_comp[lib] = cname

    libraries = kraft.setdefault("libraries", {}) or {}
    kraft["libraries"] = libraries

    # Drop stale core placeholders / bare uksched entries before reassignment
    for core in CORE_PLACEHOLDER_LIBS:
        data = libraries.get(core)
        if isinstance(data, dict) and set(data.keys()) <= {"compartment", "is_core"}:
            libraries.pop(core, None)

    for lib_name, data in list(libraries.items()):
        if not isinstance(data, dict):
            continue
        cname = lib_to_comp.get(lib_name, "comp1")
        data["compartment"] = cname

    # Ensure every non-default compartment has ≥1 kraft library.
    for i, fname in enumerate(names):
        cname = f"comp{i + 1}"
        if i == 0:
            continue
        occupied = any(
            isinstance(d, dict) and d.get("compartment") == cname
            for d in libraries.values()
        )
        if occupied:
            continue
        flibs = fabric_libs(fabrics_cfg, fname)
        injected = False
        for core in CORE_PLACEHOLDER_LIBS:
            if core in flibs or not flibs:
                libraries[core] = {"compartment": cname, "is_core": True}
                injected = True
        if not injected:
            libraries["uksched"] = {"compartment": cname, "is_core": True}
            libraries["ukschedcoop"] = {"compartment": cname, "is_core": True}
        print(f"[sync-kraft] injected uksched* (is_core) into empty {cname} ({fname})")

    # Drop uksched placeholders when not needed (monolith / 2d)
    if n < 3:
        for core in CORE_PLACEHOLDER_LIBS:
            data = libraries.get(core)
            if _is_core_placeholder(data) or (
                isinstance(data, dict) and set(data.keys()) <= {"compartment", "is_core"}
            ):
                libraries.pop(core, None)

    kraft_path.write_text(
        yaml.safe_dump(kraft, sort_keys=False, default_flow_style=False)
    )

    # Expose lwip VMEPT id for CFLAGS consumers (comp index of fabric with lwip)
    lwip_comp_idx = 1
    for i, fname in enumerate(names):
        if "lwip" in fabric_libs(fabrics_cfg, fname):
            lwip_comp_idx = i
            break
    marker = app_dir / ".vmept_lwip_comp"
    marker.write_text(str(lwip_comp_idx) + "\n")

    print(f"[sync-kraft] fabrics={n} -> kraft compartments={n} lwip_comp={lwip_comp_idx}")
    for lib_name, data in (kraft.get("libraries") or {}).items():
        if isinstance(data, dict):
            print(f"  {lib_name}: {data.get('compartment')}"
                  + (" is_core" if data.get("is_core") else ""))
    return 0


if __name__ == "__main__":
    app = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    sys.exit(sync(app))
