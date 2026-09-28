#!/bin/sh
# build_fabric.sh — Fabric 框架完整构建脚本
#
# 用法: build_fabric.sh <app-dir> [out-dir]
#
# 构建顺序:
#   Step 0: 恢复上一轮被重写的源文件
#   Step 1: 分析拓扑，生成注册代码（不含页面标记）
#   Step 2: 注册 per-library linker scripts 到各库 Makefile.uk
#   Step 3: 注册 helper 文件到 app Makefile.uk
#   Step 4: kraft build（linker scripts 生效 → binary 包含 fabric sections）
#   Step 5: 提取 section 地址 → 生成 --defsym → 注册到 Makefile.uk
#   Step 6: 重新生成 fabric_init.c（含页面标记代码）
#   Step 7: 应用 Coccinelle 重写（flexos_nop_gate → fabric_gate）
#   Step 8: make 最终编译

set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 <app-dir> [out-dir]" >&2
    echo "  app-dir : 应用目录（包含 fabric.yaml 和 kraft.yaml）" >&2
    echo "  out-dir : 输出目录（默认: <app-dir>/helper）" >&2
    exit 1
fi

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
APP_DIR=$1
OUT_DIR=${2:-"$APP_DIR/helper"}

# 相对路径 → 绝对路径
case "$APP_DIR" in
    /*) ;;
    *) APP_DIR="$REPO_ROOT/$APP_DIR" ;;
esac
case "$OUT_DIR" in
    /*) ;;
    *) OUT_DIR="$REPO_ROOT/$OUT_DIR" ;;
esac

if [ ! -d "$APP_DIR" ]; then
    echo "[fabric-build] 错误: 应用目录不存在: $APP_DIR" >&2
    exit 1
fi

if [ ! -f "$APP_DIR/fabric.yaml" ]; then
    echo "[fabric-build] 错误: $APP_DIR/fabric.yaml 不存在" >&2
    exit 1
fi

LIB_ROOT="$REPO_ROOT/unikraft/lib"
EXT_LIB_ROOT="$REPO_ROOT/libs"
MAKEFILE_UK="$APP_DIR/Makefile.uk"

# ==================================================================
#  解析 kraft.yaml 中的外部库路径（用于最终 make）
# ==================================================================
LIB_PATHS=$(python3 - "$APP_DIR/kraft.yaml" "$EXT_LIB_ROOT" <<'PY'
from pathlib import Path
import sys

try:
    import yaml
except ImportError:
    yaml = None

kraft_yaml = Path(sys.argv[1])
libs_root = Path(sys.argv[2])
text = kraft_yaml.read_text(encoding="utf-8")

if yaml is not None:
    data = yaml.safe_load(text) or {}
    names = list((data.get("libraries") or {}).keys())
else:
    names = []
    in_libraries = False
    for raw in text.splitlines():
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        if raw.startswith("libraries:"):
            in_libraries = True
            continue
        if in_libraries and not raw.startswith("  "):
            break
        if in_libraries and raw.startswith("  ") and raw.endswith(":"):
            names.append(raw.strip()[:-1])

paths = [str((libs_root / name).resolve()) for name in names if (libs_root / name).exists()]
print(":".join(paths))
PY
)

# 计算 HELPER_REL 和 APP_NAME（后续步骤共用）
HELPER_REL=$(python3 -c "
import os, sys
app = os.path.abspath(sys.argv[1])
out = os.path.abspath(sys.argv[2])
rel = os.path.relpath(out, app)
print(rel.replace(os.sep, '/'))
" "$APP_DIR" "$OUT_DIR")

APP_NAME=$(python3 -c "
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
from parse_config import load_app_lib_name
app_dir = Path(sys.argv[2])
print(load_app_lib_name(app_dir).upper())
" "$SCRIPT_DIR/parse_config" "$APP_DIR")

# ==================================================================
#  Step 0: 恢复上一轮被重写的源文件
# ==================================================================
echo ""
echo "========================================"
echo "  Step 0: 恢复源文件 + Makefile.uk"
echo "========================================"
echo ""

python3 "$SCRIPT_DIR/restore/restore_fabric_sources.py" \
    "$APP_DIR" "$LIB_ROOT" "$EXT_LIB_ROOT"

# ==================================================================
#  Step 1: 分析拓扑，生成注册代码（不含页面标记）
# ==================================================================
echo ""
echo "========================================"
echo "  Step 1: 分析拓扑并生成代码"
echo "========================================"
echo ""

python3 "$SCRIPT_DIR/hardening/apply_hardening.py" "$REPO_ROOT" "$APP_DIR"

python3 "$SCRIPT_DIR/auto_register/auto_register.py" \
    --app-dir "$APP_DIR" \
    --out-dir "$OUT_DIR" \
    --lib-root "$LIB_ROOT" \
    --ext-lib-root "$EXT_LIB_ROOT" \
    --skip-page-tagging

# ==================================================================
#  Step 2: 注册 per-library linker scripts
# ==================================================================
echo ""
echo "========================================"
echo "  Step 2: 注册 per-library linker scripts"
echo "========================================"
echo ""

# SKIP_FABRIC_SECTIONS=1 (default): keep libs in shared .data/.text like the
# known-good Jul12 redis images. Per-lib .data_comp1_fabric_* placement currently
# breaks lwip tcpip/memp under MPK gates. Gate-based cross-calls still apply.
SKIP_FABRIC_SECTIONS="${SKIP_FABRIC_SECTIONS:-1}"
if [ "$SKIP_FABRIC_SECTIONS" = "1" ]; then
    echo "[fabric-build] SKIP_FABRIC_SECTIONS=1 — 不注册 per-library .ld（与可运行镜像一致）"
    python3 - "$LIB_ROOT" "$EXT_LIB_ROOT" <<'PY'
import re, sys
from pathlib import Path
roots = [Path(sys.argv[1])]
if sys.argv[2]:
    roots.append(Path(sys.argv[2]))
pat = re.compile(r"^.*_LDFLAGS-y \+= -Wl,-T,.*lib_.*_fabric\.ld.*\n", re.M)
flexos = re.compile(r"^#\s*(.*_LDFLAGS-y \+= -Wl,-T,.*flexos_extra\.ld.*)$", re.M)
for root in roots:
    for mk in root.glob("*/Makefile.uk"):
        text = mk.read_text(encoding="utf-8", errors="replace")
        new = pat.sub("", text)
        # re-enable flexos_extra.ld if it was commented out by a prior fabric build
        new = flexos.sub(r"\1", new)
        if new != text:
            mk.write_text(new, encoding="utf-8")
            print(f"  [ldflags] cleared fabric.ld in {mk}")
PY
else
python3 - "$SCRIPT_DIR" "$APP_DIR" "$LIB_ROOT" "$EXT_LIB_ROOT" "$OUT_DIR" <<'PY'
import sys, os, re
sys.path.insert(0, os.path.join(sys.argv[1], "parse_config"))
from pathlib import Path
from parse_config import analyze, c_safe

app_dir = Path(sys.argv[2])
lib_root = Path(sys.argv[3])
ext_lib_root = Path(sys.argv[4]) if sys.argv[4] else None
out_dir = Path(sys.argv[5])

result = analyze(app_dir, lib_root, ext_lib_root)
default_fabric_names = set(result.default_fabrics.values())

search_roots = [lib_root]
if ext_lib_root:
    search_roots.append(ext_lib_root)

for fname, libs in result.fabric_libs.items():
    if fname in default_fabric_names or not libs:
        continue
    for lib in libs:
        lib_ld_name = f"lib_{lib}_fabric.ld"
        lib_ld_path = out_dir / lib_ld_name
        if not lib_ld_path.exists():
            continue

        lib_makefile = None
        for root in search_roots:
            candidate = root / lib / "Makefile.uk"
            if candidate.exists():
                lib_makefile = candidate
                break
        if not lib_makefile:
            continue

        if lib_ld_name in lib_makefile.read_text():
            continue

        try:
            ld_rel = os.path.relpath(lib_ld_path, lib_makefile.parent).replace(os.sep, '/')
        except ValueError:
            continue

        var_name = "LIB" + lib.upper().replace("-", "").replace("_", "")

        makefile_text = lib_makefile.read_text()
        modified = False

        # 注释掉 flexos_extra.ld（它会把所有 .data 捕获到 .data_comp1，覆盖我们的脚本）
        flexos_line = f"{var_name}_LDFLAGS-y += -Wl,-T,$({var_name}_BASE)/flexos_extra.ld"
        if flexos_line in makefile_text and not f"# {flexos_line}" in makefile_text:
            makefile_text = makefile_text.replace(
                flexos_line,
                f"# {flexos_line}  # 已禁用：由 fabric linker script 替代"
            )
            modified = True

        # 添加 fabric linker script（支持含多个 sublib 的外部库）
        ld_flags_line = f"{var_name}_LDFLAGS-y += -Wl,-T,$({var_name}_BASE)/{ld_rel}"
        sublib_pattern = re.compile(
            rf"^({re.escape(var_name)}(?:_[A-Z0-9]+)?_LDFLAGS-y \+= -Wl,-T,.*{re.escape(lib_ld_name)})",
            re.M,
        )
        if lib_ld_name not in makefile_text:
            targets = []
            for match in re.finditer(rf"^({re.escape(var_name)}(?:_[A-Z0-9]+)?)_LDFLAGS-y \+=", makefile_text, re.M):
                targets.append(match.group(1))
            if not targets:
                targets = [var_name]
            for target in dict.fromkeys(targets):
                makefile_text += f"\n# Fabric section rename for {fname}\n"
                makefile_text += f"{target}_LDFLAGS-y += -Wl,-T,$({var_name}_BASE)/{ld_rel}\n"
            modified = True
        elif ld_flags_line not in makefile_text and not sublib_pattern.search(makefile_text):
            makefile_text += f"\n# Fabric section rename for {fname}\n"
            makefile_text += ld_flags_line + "\n"
            modified = True

        if modified:
            lib_makefile.write_text(makefile_text)
            print(f"  [ldflags] {lib} -> {lib_ld_name} (flexos_extra.ld 已禁用)")

print("[fabric-build] per-library linker scripts 已注册")
PY
fi

# ==================================================================
#  Step 3: 注册 helper 文件到 app Makefile.uk
# ==================================================================
echo ""
echo "========================================"
echo "  Step 3: 注册到 Makefile.uk"
echo "========================================"
echo ""

if ! grep -q 'helper/fabric_init.c' "$MAKEFILE_UK" 2>/dev/null; then
    {
        echo "${APP_NAME}_SRCS-y += \$(${APP_NAME}_BASE)/${HELPER_REL}/fabric_init.c"
        echo "${APP_NAME}_SRCS-y += \$(${APP_NAME}_BASE)/${HELPER_REL}/register_all.c"
        echo "${APP_NAME}_CINCLUDES-y += -I\$(${APP_NAME}_BASE)/${HELPER_REL}"
    } >> "$MAKEFILE_UK"
    echo "[fabric-build] 已将 helper 文件注册到 $MAKEFILE_UK"
else
    echo "[fabric-build] Makefile.uk 中已存在 helper 引用，跳过"
fi

# ==================================================================
#  Step 4: kraft build（linker scripts 生效 → binary 包含 fabric sections）
# ==================================================================
echo ""
echo "========================================"
echo "  Step 4: make prepare + kraft build"
echo "========================================"
echo ""

cd "$APP_DIR"
# Non-interactive: avoid kconfig menus when .config was patched (e.g. uksp).
if [ -f .config ]; then
    yes "" | make olddefconfig >/dev/null 2>&1 || true
fi

# Detect FlexOS VM/EPT: multi-image kraft build; skip MPK-only post steps.
IS_VMEPT=0
if grep -q '^CONFIG_LIBFLEXOS_VMEPT=y' .config 2>/dev/null; then
    IS_VMEPT=1
fi
ISO_BACKEND=$(grep -E '^\s+backend:' "$APP_DIR/fabric.yaml" | awk '{print $2}' | head -1)
ISO_BACKEND=${ISO_BACKEND:-ept}

if [ "$IS_VMEPT" -eq 1 ]; then
    echo "[fabric-build] VM/EPT mode (backend=$ISO_BACKEND)"
    echo "[fabric-build] Apply fabric_gate rewrites before multi-image kraft build"
    # Ensure fabric_init uses EPTConfig (step 1 used --skip-page-tagging; fine for EPT)
    python3 "$SCRIPT_DIR/auto_register/auto_register.py" \
        --app-dir "$APP_DIR" \
        --out-dir "$OUT_DIR" \
        --lib-root "$LIB_ROOT" \
        --ext-lib-root "$EXT_LIB_ROOT" \
        --skip-page-tagging
    if [ -f "$OUT_DIR/apply_fabric_rules.sh" ] && [ "${SKIP_FABRIC_SECTIONS:-1}" != "1" ]; then
        sh "$OUT_DIR/apply_fabric_rules.sh" || true
    else
        echo "[fabric-build] SKIP_FABRIC_SECTIONS=${SKIP_FABRIC_SECTIONS:-1} — cocci skipped"
    fi
    make prepare </dev/null
    set +e
    kraft -v build --no-progress --fast --compartmentalize
    kraft_rc=$?
    set -e
    if [ "$kraft_rc" -ne 0 ]; then
        echo "[fabric-build] kraft build failed (rc=$kraft_rc)" >&2
        exit "$kraft_rc"
    fi

    echo ""
    echo "========================================"
    echo "  VM/EPT build complete"
    echo "========================================"
    echo ""
    ls -la "$APP_DIR/build"/*comp* 2>/dev/null || ls -la "$APP_DIR/build"/*kvm* 2>/dev/null || true

    # Drop stale peer images from a previous multi-domain build (e.g. n07 → n01).
    NCOMP=$(python3 - "$APP_DIR/kraft.yaml" <<'PY'
import sys, yaml
k = yaml.safe_load(open(sys.argv[1])) or {}
print(len(k.get("compartments") or []))
PY
)
    if [ "$NCOMP" -lt 2 ]; then
        echo "[fabric-build] monolith: removing stale .comp1+ images"
        rm -f "$APP_DIR/build"/nginx_kvm-x86_64.comp[1-9]* \
              "$APP_DIR/build"/nginx_kvm-x86_64.dbg.comp[1-9]* 2>/dev/null || true
        # Multiboot load_end must cover all PROGBITS (e.g. .data_comp*);
        # otherwise memp_pools land as zeros and nginx faults in memp_malloc.
        python3 - "$APP_DIR/build" <<'PY'
import struct, sys, re, subprocess
from pathlib import Path
build = Path(sys.argv[1])
for img in sorted(build.glob("nginx_kvm-x86_64.comp0")):
    dbg = build / "nginx_kvm-x86_64.dbg.comp0"
    if not dbg.exists():
        dbg = img
    out = subprocess.check_output(
        ["readelf", "-SW", str(dbg)], text=True, stderr=subprocess.DEVNULL
    )
    # Extend load_end only for .data_comp* PROGBITS past the current end.
    data_comp_end = 0
    for line in out.splitlines():
        if "PROGBITS" not in line or ".data_comp" not in line:
            continue
        m = re.search(
            r"\.data_comp\S*\s+PROGBITS\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)",
            line,
        )
        if not m:
            continue
        addr = int(m.group(1), 16)
        size = int(m.group(2), 16)
        data_comp_end = max(data_comp_end, addr + size)
    if data_comp_end <= 0:
        continue
    new_load_end = (data_comp_end + 0xFFF) & ~0xFFF
    b = bytearray(img.read_bytes())
    off = b.find(struct.pack("<I", 0x1BADB002))
    if off < 0:
        continue
    old_load_end = struct.unpack_from("<I", b, off + 20)[0]
    bss_end = struct.unpack_from("<I", b, off + 24)[0]
    # Always clamp load_end to cover .data_comp* and nothing beyond
    # (a prior bad patch may have left an oversized load_end).
    if new_load_end == old_load_end and bss_end >= new_load_end:
        continue
    if new_load_end > bss_end:
        new_bss = (new_load_end + 0x10000 + 0xFFF) & ~0xFFF
        struct.pack_into("<I", b, off + 24, new_bss)
        print(
            f"[fabric-build] patched {img.name} bss_end "
            f"{hex(bss_end)} -> {hex(new_bss)}"
        )
        bss_end = new_bss
    struct.pack_into("<I", b, off + 20, new_load_end)
    img.write_bytes(b)
    print(
        f"[fabric-build] patched {img.name} multiboot load_end "
        f"{hex(old_load_end)} -> {hex(new_load_end)}"
    )
PY
    fi

    echo ""
    echo "Run with: $APP_DIR/run_ept.sh start"
    exit 0
fi

make prepare </dev/null
kraft -v build --no-progress --fast --compartmentalize

# ==================================================================
#  Step 5: 提取 section 地址 → 生成 --defsym → 注册到 Makefile.uk
# ==================================================================
echo ""
echo "========================================"
echo "  Step 5: 提取 section 地址并生成 --defsym"
echo "========================================"
echo ""

python3 - "$SCRIPT_DIR" "$APP_DIR" "$LIB_ROOT" "$EXT_LIB_ROOT" "$OUT_DIR" "$APP_DIR/build" <<'PY'
import sys, os, subprocess, re
sys.path.insert(0, os.path.join(sys.argv[1], "parse_config"))
from pathlib import Path
from parse_config import analyze, c_safe

app_dir = Path(sys.argv[2])
lib_root = Path(sys.argv[3])
ext_lib_root = Path(sys.argv[4]) if sys.argv[4] else None
out_dir = Path(sys.argv[5])
build_dir = Path(sys.argv[6])

result = analyze(app_dir, lib_root, ext_lib_root)
default_fabric_names = set(result.default_fabrics.values())

# 找到 kraft build 生成的二进制
dbg_image = None
for f in build_dir.rglob("*.dbg"):
    dbg_image = f
    break

if not dbg_image:
    print("[fabric-build] 警告: 未找到 .dbg 二进制，跳过 section 地址提取")
    sys.exit(0)

# 用 readelf -W -S 提取 section 地址（-W 获取完整 section 名）
readelf_output = subprocess.check_output(
    ["readelf", "-W", "-S", str(dbg_image)], text=True, stderr=subprocess.DEVNULL
)

# 解析 section 地址
section_addrs = {}
for line in readelf_output.splitlines():
    m = re.search(
        r"\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-fA-F]+)\s+\S+\s+([0-9a-fA-F]+)",
        line,
    )
    if not m:
        continue
    name = m.group(1)
    addr = int(m.group(2), 16)
    size = int(m.group(3), 16)
    section_addrs[name] = (addr, size)

# 生成 --defsym 参数（所有 fabric sections，即使为空也生成，地址为 0）
defsym_lines = []
for fname, libs in result.fabric_libs.items():
    if fname in default_fabric_names or not libs:
        continue
    safe = c_safe(fname)
    for suffix in ["data", "bss", "text"]:
        section_name = f".{suffix}_{safe}"
        if section_name in section_addrs:
            addr, size = section_addrs[section_name]
            defsym_lines.append(f"-Wl,--defsym=_fabric_{safe}_{suffix}_start=0x{addr:x}")
            defsym_lines.append(f"-Wl,--defsym=_fabric_{safe}_{suffix}_end=0x{addr + size:x}")
        else:
            # section 为空或被链接器丢弃，定义为地址 0
            defsym_lines.append(f"-Wl,--defsym=_fabric_{safe}_{suffix}_start=0x0")
            defsym_lines.append(f"-Wl,--defsym=_fabric_{safe}_{suffix}_end=0x0")

if defsym_lines:
    defsym_file = out_dir / "fabric_defsyms.txt"
    defsym_file.write_text("\n".join(defsym_lines) + "\n", encoding="utf-8")
    print(f"[fabric-build] 已生成 {defsym_file.name}（{len(defsym_lines)//2} 个 section 符号）")
else:
    print("[fabric-build] 未找到 fabric sections，跳过符号生成")
PY

# 注册 --defsym 参数到 Makefile.uk（替换旧条目，避免重复/过期地址）
if [ -f "$OUT_DIR/fabric_defsyms.txt" ]; then
    DEFSYM_FLAGS=$(cat "$OUT_DIR/fabric_defsyms.txt" | tr '\n' ' ')
    python3 - "$MAKEFILE_UK" "$DEFSYM_FLAGS" <<'PY'
import re, sys
makefile = sys.argv[1]
defsym_flags = sys.argv[2].strip()
text = open(makefile, encoding="utf-8").read()
text = re.sub(r"\nLDFLAGS-y \+= .*_fabric_comp1_fabric_[0-9]+.*\n", "\n", text)
text = re.sub(r"\nLDFLAGS-y \+= -Wl,--defsym=_fabric_[^\n]*\n", "\n", text)
text = text.rstrip() + f"\nLDFLAGS-y += {defsym_flags}\n"
open(makefile, "w", encoding="utf-8").write(text)
print(f"[fabric-build] 已更新 --defsym 参数到 {makefile}")
PY
fi

# ==================================================================
#  Step 6: 重新生成 fabric_init.c（含页面标记代码）
# ==================================================================
echo ""
echo "========================================"
echo "  Step 6: 重新生成 fabric_init.c"
echo "========================================"
echo ""

# Skip MPK section page-tagging: tagging uksched/lwip .bss then calling
# stdio (or other ungated readers) causes PF_PK. Gate-based isolation still
# applies; this matches the previously working redis multi-domain images.
python3 "$SCRIPT_DIR/auto_register/auto_register.py" \
    --app-dir "$APP_DIR" \
    --out-dir "$OUT_DIR" \
    --lib-root "$LIB_ROOT" \
    --ext-lib-root "$EXT_LIB_ROOT" \
    --skip-page-tagging

# Without per-lib sections, cross-fabric gates deadlock; call isolated inits directly.
if [ "${SKIP_FABRIC_SECTIONS:-1}" = "1" ] && [ -f "$OUT_DIR/fabric_init.c" ]; then
    python3 - "$OUT_DIR/fabric_init.c" <<'PY'
from pathlib import Path
import re, sys
p = Path(sys.argv[1])
t = p.read_text(encoding="utf-8")
t2, n = re.subn(
    r"/\* Route \w+ through fabric gate \(lib in non-default fabric\)\. \*/\n"
    r"static int (__fabric_isolated_\w+_init)\(void\)\n"
    r"\{\n"
    r"    int rc = 0;\n"
    r"    fabric_gate_r\(\d+, rc, (_\w+)\);\n"
    r"    return rc;\n"
    r"\}",
    "/* Direct call: SKIP_FABRIC_SECTIONS (gate needs per-lib sections). */\n"
    "static int \\1(void)\n"
    "{\n"
    "    return \\2();\n"
    "}",
    t,
)
if n:
    p.write_text(t2, encoding="utf-8")
    print(f"[fabric-build] rewrote {n} isolated initcall(s) to direct calls")
PY
fi

# ==================================================================
#  Step 7: 应用 Coccinelle 重写（flexos_nop_gate → fabric_gate）
# ==================================================================
echo ""
echo "========================================"
echo "  Step 7: 应用 flexos_nop_gate → fabric_gate 重写"
echo "========================================"
echo ""

if [ -f "$OUT_DIR/apply_fabric_rules.sh" ]; then
    if [ "${SKIP_FABRIC_SECTIONS:-1}" = "1" ]; then
        echo "[fabric-build] SKIP_FABRIC_SECTIONS=1 — 跳过 cocci fabric_gate 重写（避免无 section 时 gate 死锁）"
    else
        sh "$OUT_DIR/apply_fabric_rules.sh" || true
    fi
else
    echo "[fabric-build] 警告: apply_fabric_rules.sh 不存在，跳过重写"
fi

# ==================================================================
#  Step 8: 最终编译
# ==================================================================
echo ""
echo "========================================"
echo "  Step 8: 最终编译"
echo "========================================"
echo ""

# 删除旧的 partial link 产物，强制重新链接
find "$APP_DIR/build" -name "*.ld.o" -delete 2>/dev/null || true

# Step 6 会重新生成 fabric_init.c（含 MPK 页面标记），必须强制重编译
touch "$OUT_DIR/fabric_init.c"
find "$APP_DIR/build" -name "fabric_init.o" -delete 2>/dev/null || true

# Re-apply hardening/kconfig after intermediate makes may have rewritten .config
# (esp. CONFIG_LWIP_NOTHREADS for isolated lwip).
python3 "$SCRIPT_DIR/hardening/apply_hardening.py" "$REPO_ROOT" "$APP_DIR"

cd "$REPO_ROOT/unikraft"
if [ -n "$LIB_PATHS" ]; then
    make A="$APP_DIR" L="$LIB_PATHS" -j4
else
    make A="$APP_DIR" -j4
fi

# ==================================================================
#  Step 9: Coccinelle 重写后 section 地址可能变化，用最终二进制刷新 defsym + MPK 标记
# ==================================================================
echo ""
echo "========================================"
echo "  Step 9: 刷新 defsym 并重新生成 MPK 页面标记"
echo "========================================"
echo ""

_refresh=0
python3 - "$SCRIPT_DIR" "$APP_DIR" "$LIB_ROOT" "$EXT_LIB_ROOT" "$OUT_DIR" "$APP_DIR/build" <<'PY' || _refresh=$?
import sys, os, subprocess, re
sys.path.insert(0, os.path.join(sys.argv[1], "parse_config"))
from pathlib import Path
from parse_config import analyze, c_safe

app_dir = Path(sys.argv[2])
lib_root = Path(sys.argv[3])
ext_lib_root = Path(sys.argv[4]) if sys.argv[4] else None
out_dir = Path(sys.argv[5])
build_dir = Path(sys.argv[6])

result = analyze(app_dir, lib_root, ext_lib_root)
default_fabric_names = set(result.default_fabrics.values())

dbg_image = None
for f in build_dir.rglob("*.dbg"):
    dbg_image = f
    break

if not dbg_image:
    print("[fabric-build] 警告: 未找到 .dbg 二进制，跳过 defsym 刷新")
    sys.exit(0)

readelf_output = subprocess.check_output(
    ["readelf", "-W", "-S", str(dbg_image)], text=True, stderr=subprocess.DEVNULL
)

section_addrs = {}
for line in readelf_output.splitlines():
    m = re.search(
        r"\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-fA-F]+)\s+\S+\s+([0-9a-fA-F]+)",
        line,
    )
    if not m:
        continue
    name = m.group(1)
    addr = int(m.group(2), 16)
    size = int(m.group(3), 16)
    section_addrs[name] = (addr, size)

defsym_lines = []
for fname, libs in result.fabric_libs.items():
    if fname in default_fabric_names or not libs:
        continue
    safe = c_safe(fname)
    for suffix in ["data", "bss", "text"]:
        section_name = f".{suffix}_{safe}"
        if section_name in section_addrs:
            addr, size = section_addrs[section_name]
            defsym_lines.append(f"-Wl,--defsym=_fabric_{safe}_{suffix}_start=0x{addr:x}")
            defsym_lines.append(f"-Wl,--defsym=_fabric_{safe}_{suffix}_end=0x{addr + size:x}")
        else:
            defsym_lines.append(f"-Wl,--defsym=_fabric_{safe}_{suffix}_start=0x0")
            defsym_lines.append(f"-Wl,--defsym=_fabric_{safe}_{suffix}_end=0x0")

if defsym_lines:
    defsym_file = out_dir / "fabric_defsyms.txt"
    old = defsym_file.read_text(encoding="utf-8") if defsym_file.exists() else ""
    new = "\n".join(defsym_lines) + "\n"
    defsym_file.write_text(new, encoding="utf-8")
    if old.strip() == new.strip():
        print("[fabric-build] defsym 地址未变化，跳过二次编译")
        sys.exit(0)
    print(f"[fabric-build] defsym 已更新（{len(defsym_lines)//2} 个 section），需要二次编译")
    sys.exit(42)
else:
    print("[fabric-build] 未找到 fabric sections，跳过 defsym 刷新")
    sys.exit(0)
PY

_refresh=${_refresh:-0}
if [ "$_refresh" -eq 42 ]; then
    DEFSYM_FLAGS=$(cat "$OUT_DIR/fabric_defsyms.txt" | tr '\n' ' ')
    python3 - "$MAKEFILE_UK" "$DEFSYM_FLAGS" <<'PY'
import re, sys
makefile = sys.argv[1]
defsym_flags = sys.argv[2].strip()
text = open(makefile, encoding="utf-8").read()
text = re.sub(r"\nLDFLAGS-y \+= .*_fabric_comp1_fabric_[0-9]+.*\n", "\n", text)
text = re.sub(r"\nLDFLAGS-y \+= -Wl,--defsym=_fabric_[^\n]*\n", "\n", text)
text = text.rstrip() + f"\nLDFLAGS-y += {defsym_flags}\n"
open(makefile, "w", encoding="utf-8").write(text)
print(f"[fabric-build] 已更新 --defsym 参数到 {makefile}")
PY

    python3 "$SCRIPT_DIR/auto_register/auto_register.py" \
        --app-dir "$APP_DIR" \
        --out-dir "$OUT_DIR" \
        --lib-root "$LIB_ROOT" \
        --ext-lib-root "$EXT_LIB_ROOT" \
        --skip-page-tagging

    touch "$OUT_DIR/fabric_init.c"
    find "$APP_DIR/build" -name "fabric_init.o" -delete 2>/dev/null || true
    find "$APP_DIR/build" -name "*.ld.o" -delete 2>/dev/null || true

    cd "$REPO_ROOT/unikraft"
    if [ -n "$LIB_PATHS" ]; then
        make A="$APP_DIR" L="$LIB_PATHS" -j4
    else
        make A="$APP_DIR" -j4
    fi
fi

echo ""
echo "[fabric-build] 构建完成。"
