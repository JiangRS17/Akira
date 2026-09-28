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

spatch --sp-file '/root/.unikraft_akira/apps/base-overhead/helper/cocci/rule_0.cocci' '/root/.unikraft_akira/apps/base-overhead/cross_domain.c' --in-place
ensure_include '/root/.unikraft_akira/apps/base-overhead/cross_domain.c'
spatch --sp-file '/root/.unikraft_akira/apps/base-overhead/helper/cocci/rule_1.cocci' '/root/.unikraft_akira/apps/base-overhead/cross_domain_re.c' --in-place
ensure_include '/root/.unikraft_akira/apps/base-overhead/cross_domain_re.c'
spatch --sp-file '/root/.unikraft_akira/apps/base-overhead/helper/cocci/rule_2.cocci' '/root/.unikraft_akira/apps/base-overhead/cross_server.c' --in-place
ensure_include '/root/.unikraft_akira/apps/base-overhead/cross_server.c'
spatch --sp-file '/root/.unikraft_akira/apps/base-overhead/helper/cocci/rule_3.cocci' '/root/.unikraft_akira/apps/base-overhead/cross_server1.c' --in-place
ensure_include '/root/.unikraft_akira/apps/base-overhead/cross_server1.c'
spatch --sp-file '/root/.unikraft_akira/apps/base-overhead/helper/cocci/rule_4.cocci' '/root/.unikraft_akira/unikraft/lib/example2/example2.c' --in-place
ensure_include '/root/.unikraft_akira/unikraft/lib/example2/example2.c'
spatch --sp-file '/root/.unikraft_akira/apps/base-overhead/helper/cocci/rule_5.cocci' '/root/.unikraft_akira/unikraft/lib/example3/example3.c' --in-place
ensure_include '/root/.unikraft_akira/unikraft/lib/example3/example3.c'
spatch --sp-file '/root/.unikraft_akira/apps/base-overhead/helper/cocci/rule_6.cocci' '/root/.unikraft_akira/unikraft/lib/example4/example4.c' --in-place
ensure_include '/root/.unikraft_akira/unikraft/lib/example4/example4.c'
