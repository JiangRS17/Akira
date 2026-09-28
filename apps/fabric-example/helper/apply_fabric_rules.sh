#!/bin/sh
set -eu

ensure_include() {
    file=$1
    if grep -q 'fabric/isolation.h' "$file" 2>/dev/null; then
        return 0
    fi
    python3 - "$file" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text(encoding='utf-8')
needle = '#include <flexos/isolation.h>\n'
insert = '#include <flexos/isolation.h>\n#include <fabric/isolation.h>\n'
if 'fabric/isolation.h' in text:
    raise SystemExit(0)
if needle in text:
    text = text.replace(needle, insert, 1)
else:
    text = '#include <fabric/isolation.h>\n' + text
path.write_text(text, encoding='utf-8')
PY
}

spatch --sp-file '/root/.unikraft_akira/apps/fabric-example/helper/cocci/rule_0.cocci' '/root/.unikraft_akira/apps/fabric-example/main.c' --in-place
ensure_include '/root/.unikraft_akira/apps/fabric-example/main.c'
