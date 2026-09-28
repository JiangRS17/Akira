#!/usr/bin/env python3
"""Apply per-component software hardening from fabric.yaml.

FlexOS-style hardening: stack protector + UBSan per component (uksched: stack protector only).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

_parse_config_dir = str(Path(__file__).resolve().parent.parent / "parse_config")
if _parse_config_dir not in sys.path:
    sys.path.insert(0, _parse_config_dir)

from parse_config import load_yaml, normalise_lib_name  # noqa: E402

HARDEN_MARKER = "# fabric-hardening"
# MPK + private stacks: -fstack-protector / libuksp false-positive ("Stack smashing
# detected") after netif comes up. Keep markers for CSV metadata only; do not
# inject SSP/UBSan/KASan until canaries work across fabric stacks.
HARDEN_FLAGS = ""
HARDEN_FLAGS_LIGHT = ""
LIGHT_COMPONENTS = {"uksched"}

COMPONENT_TARGETS: dict[str, list[tuple[Path, list[str]]]] = {
    "redis": [
        ("libs/redis", ["LIBREDIS", "LIBREDIS_SERVER", "LIBREDIS_COMMON",
                        "LIBREDIS_LUA", "LIBREDIS_HIREDIS"]),
    ],
    "nginx": [
        ("libs/nginx", ["LIBNGINX"]),
    ],
    "newlib": [
        ("libs/newlib", ["LIBNEWLIBC", "LIBNEWLIBM", "LIBNEWLIBGLUE"]),
    ],
    "uksched": [
        ("unikraft/lib/uksched", ["LIBUKSCHED"]),
        ("unikraft/lib/ukschedcoop", ["LIBUKSCHEDCOOP"]),
    ],
    "lwip": [
        ("libs/lwip", ["LIBLWIP"]),
    ],
}

# Do not enable UKSP under MPK private stacks.
KCONFIG_ENABLE: list[tuple[str, str]] = []

KCONFIG_DISABLE = [
    "CONFIG_LIBUBSAN",
    "CONFIG_LIBUBSAN_GLOBAL",
    "CONFIG_LIBKASAN",
    "CONFIG_LIBKASAN_GLOBAL",
    "CONFIG_LIBUKSP",
    "CONFIG_STACKPROTECTOR_STRONG",
    "CONFIG_STACKPROTECTOR_REGULAR",
    "CONFIG_STACKPROTECTOR_ALL",
    "CONFIG_LIBUKSP_VALUE_USECONSTANT",
    "CONFIG_LIBUKSP_VALUE_USERANDOM",
]


def load_hardening(fabric_yaml: Path) -> list[str]:
    cfg = load_yaml(fabric_yaml)
    raw = cfg.get("hardening", []) or []
    return [normalise_lib_name(str(x)) for x in raw]


def _patch_makefile(makefile: Path, var_prefixes: list[str], enable: bool, flags: str = HARDEN_FLAGS) -> bool:
    text = makefile.read_text(encoding="utf-8", errors="replace")
    original = text

    for prefix in var_prefixes:
        # Always strip previous fabric-hardening CFLAGS (SSP disabled for MPK).
        text = re.sub(
            rf"^{re.escape(prefix)}_CFLAGS-y \+= .*{re.escape(HARDEN_MARKER)}.*\n?",
            "",
            text,
            flags=re.M,
        )
        if enable and flags.strip():
            line = f"{prefix}_CFLAGS-y += {flags}  {HARDEN_MARKER}"
            text += f"\n{line}\n"

    if text != original:
        makefile.write_text(text, encoding="utf-8")
        return True
    return False


def apply_makefile_hardening(
    repo_root: Path,
    app_dir: Path,
    components: list[str],
) -> int:
    wanted = set(components)
    changed = 0

    for component, targets in COMPONENT_TARGETS.items():
        enable = component in wanted
        flags = HARDEN_FLAGS_LIGHT if component in LIGHT_COMPONENTS else HARDEN_FLAGS
        for rel_dir, prefixes in targets:
            makefile = repo_root / rel_dir / "Makefile.uk"
            if not makefile.exists():
                continue
            if _patch_makefile(makefile, prefixes, enable, flags):
                changed += 1
                state = "enabled" if enable else "cleared"
                print(f"  [hardening] {component}: {makefile} ({state})")

    app_makefile = app_dir / "Makefile.uk"
    if app_makefile.exists():
        if _patch_makefile(app_makefile, ["APPREDIS"], "redis" in wanted, HARDEN_FLAGS):
            changed += 1
            print(f"  [hardening] redis: {app_makefile}")
        if _patch_makefile(app_makefile, ["APPNGINX"], "nginx" in wanted, HARDEN_FLAGS):
            changed += 1
            print(f"  [hardening] nginx: {app_makefile}")

    return changed


def apply_kconfig(app_dir: Path, components: list[str]) -> None:
    config_path = app_dir / ".config"
    if not config_path.exists():
        return

    text = config_path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()

    def set_opt(name: str, value: str | None) -> None:
        nonlocal lines
        found = False
        new_lines = []
        for line in lines:
            if line.startswith(f"{name}=") or line.startswith(f"# {name} is not set"):
                found = True
                if value is None:
                    continue
                new_lines.append(f"{name}={value}")
            else:
                new_lines.append(line)
        if not found and value is not None:
            new_lines.append(f"{name}={value}")
        lines = new_lines

    if components:
        for name, value in KCONFIG_ENABLE:
            set_opt(name, value)
    else:
        for name, _ in KCONFIG_ENABLE:
            set_opt(name, None)
            lines.append(f"# {name} is not set")
    # Always clear KASan: leftover=y from prior harden builds breaks multi-domain boot.
    for name in KCONFIG_DISABLE:
        set_opt(name, None)
        lines.append(f"# {name} is not set")

    config_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    build_config = app_dir / "build" / "config"
    if build_config.exists():
        build_config.write_text(config_path.read_text(encoding="utf-8"), encoding="utf-8")


def lwip_is_isolated(fabric_yaml: Path) -> bool:
    cfg = load_yaml(fabric_yaml)
    compartments = []
    if "fabrics" in cfg:
        compartments.append(cfg)
    else:
        compartments = [v for v in cfg.values() if isinstance(v, dict) and "fabrics" in v]

    for comp in compartments:
        fabrics = comp.get("fabrics", {}) or {}
        default_name = None
        for name, fcfg in fabrics.items():
            if isinstance(fcfg, dict) and fcfg.get("default"):
                default_name = name
                break
        for name, fcfg in fabrics.items():
            if name == default_name:
                continue
            libs = fcfg.get("libs", []) if isinstance(fcfg, dict) else []
            for entry in libs:
                lib = normalise_lib_name(entry.get("name", "") if isinstance(entry, dict) else str(entry))
                if lib == "lwip":
                    return True
    return False


def sync_lwip_init_source(ext_lib_root: Path, isolated: bool) -> None:
    init_c = ext_lib_root / "lwip" / "init.c"
    if not init_c.exists():
        return
    text = init_c.read_text(encoding="utf-8", errors="replace")

    split_pat = re.compile(
        r'__attribute__\(\(section\("\.text_shared"\)\)\)\s*\n\s*int liblwip_init\(void\)\s*\{',
        re.M,
    )
    merged_pat = re.compile(
        r'int _liblwip_init\(\);\s*\n'
        r'__attribute__\(\(section\("\.text_shared"\)\)\)\s*\n'
        r'int liblwip_init\(void\)\s*\n'
        r'\{int ret;\s*\n'
        r'\tflexos_nop_gate_r\(0, 0, ret, _liblwip_init\);\s*\n'
        r'\treturn ret;\s*\n'
        r'\}\s*\n\s*'
        r'int _liblwip_init\(\)\s*\{',
        re.M,
    )
    # tcpip_thread has no MPK domain switch; isolated lwip must use sync lwip_init().
    tcpip_pat = re.compile(
        r'tcpip_init\(_lwip_init_done,\s*NULL\);\s*\n'
        r'\s*/\* Wait until stack is booted \*/\s*\n'
        r'\s*(?:flexos_nop_gate\(0,\s*0,\s*uk_semaphore_down,\s*&_lwip_init_sem\)|'
        r'fabric_gate\(\d+,\s*uk_semaphore_down,\s*&_lwip_init_sem\));',
        re.M,
    )
    sync_snip = (
        "/* fabric-isolated: sync init (tcpip thread lacks MPK domain switch) */\n"
        "\tlwip_init();"
    )
    tcpip_snip = (
        "tcpip_init(_lwip_init_done, NULL);\n"
        "\n"
        "\t/* Wait until stack is booted */\n"
        "\tflexos_nop_gate(0, 0, uk_semaphore_down, &_lwip_init_sem);"
    )

    if isolated:
        # Already split as _liblwip_init() or _liblwip_init(void) (VMEPT form).
        already_split = (
            re.search(r"\bint\s+_liblwip_init\s*\(\s*(void)?\s*\)", text)
            is not None
        )
        if not already_split:
            text, n = merged_pat.subn(
                "int _liblwip_init();\n"
                "__attribute__((section(\".text_shared\")))\n"
                "int liblwip_init(void)\n"
                "{int ret;\n"
                "\tflexos_nop_gate_r(0, 0, ret, _liblwip_init);\n"
                "\treturn ret;\n"
                "}\n\n"
                "int _liblwip_init()\n{",
                text,
                count=1,
            )
            if n == 0:
                text, n = split_pat.subn(
                    "int _liblwip_init();\n"
                    "__attribute__((section(\".text_shared\")))\n"
                    "int liblwip_init(void)\n"
                    "{int ret;\n"
                    "\tflexos_nop_gate_r(0, 0, ret, _liblwip_init);\n"
                    "\treturn ret;\n"
                    "}\n\n"
                    "int _liblwip_init()\n{",
                    text,
                    count=1,
                )
            if n == 0:
                raise RuntimeError(f"failed to split liblwip_init in {init_c}")
        text = text.replace(
            "uk_lib_initcall(liblwip_init);",
            "/* fabric-isolated: liblwip_init invoked via fabric gate */",
        )
        # Keep tcpip_init: sync lwip_init() under NO_SYS=0 breaks memp/timers.
        # Page-tagging is skipped; tcpip thread can access untagged (key0) memory.
        if "fabric-isolated: sync init" in text:
            text = text.replace(sync_snip, tcpip_snip)
            print("[hardening] lwip: removed stale sync-init rewrite")
    else:
        text, n = merged_pat.subn(
            "__attribute__((section(\".text_shared\")))\n"
            "int liblwip_init(void)\n{",
            text,
            count=1,
        )
        text = text.replace(
            "/* fabric-isolated: liblwip_init invoked via fabric gate */",
            "uk_lib_initcall(liblwip_init);",
        )
        if "fabric-isolated: sync init" in text:
            text = text.replace(sync_snip, tcpip_snip)
            print("[hardening] lwip: restored tcpip_init (monolith)")

    init_c.write_text(text, encoding="utf-8")
    print(f"[hardening] lwip init.c synced (isolated={isolated})")


def sync_lwip_kconfig(app_dir: Path, fabric_yaml: Path) -> None:
    """Keep lwIP on THREADS for Redis (Socket API requires NO_SYS=0).

    Isolated lwip still uses threaded mode; cross-fabric calls go through
    fabric gates. NOTHREADS disables Socket API and cannot run Redis.
    """
    config_path = app_dir / ".config"
    if not config_path.exists():
        return
    lines = config_path.read_text(encoding="utf-8", errors="replace").splitlines()
    out = []
    seen_nothreads = False
    seen_threads = False
    for line in lines:
        if line.startswith("CONFIG_LWIP_NOTHREADS=") or line.startswith("# CONFIG_LWIP_NOTHREADS"):
            seen_nothreads = True
            out.append("# CONFIG_LWIP_NOTHREADS is not set")
        elif line.startswith("CONFIG_LWIP_THREADS=") or line.startswith("# CONFIG_LWIP_THREADS"):
            seen_threads = True
            out.append("CONFIG_LWIP_THREADS=y")
        else:
            out.append(line)
    if not seen_nothreads:
        out.append("# CONFIG_LWIP_NOTHREADS is not set")
    if not seen_threads:
        out.append("CONFIG_LWIP_THREADS=y")
    config_path.write_text("\n".join(out) + "\n", encoding="utf-8")
    build_config = app_dir / "build" / "config"
    if build_config.exists():
        build_config.write_text(config_path.read_text(encoding="utf-8"), encoding="utf-8")
    if lwip_is_isolated(fabric_yaml):
        print("[hardening] CONFIG_LWIP_THREADS=y (Socket API; isolated via gates)")


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: apply_hardening.py <repo-root> <app-dir>", file=sys.stderr)
        return 1

    repo_root = Path(sys.argv[1]).resolve()
    app_dir = Path(sys.argv[2]).resolve()
    fabric_yaml = app_dir / "fabric.yaml"
    if not fabric_yaml.exists():
        print(f"[hardening] missing {fabric_yaml}", file=sys.stderr)
        return 1

    components = load_hardening(fabric_yaml)
    print(f"[hardening] components: {', '.join(components) if components else 'none'}")
    isolated = lwip_is_isolated(fabric_yaml)
    sync_lwip_init_source(Path(sys.argv[1]).resolve() / "libs", isolated)
    sync_lwip_kconfig(app_dir, fabric_yaml)
    apply_kconfig(app_dir, components)
    apply_makefile_hardening(repo_root, app_dir, components)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
