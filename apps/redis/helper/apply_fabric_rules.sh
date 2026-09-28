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

spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_0.cocci' '/root/.unikraft_akira/apps/redis/build/liblwip/origin/lwip-2.1.2/src/api/netdb.c' --in-place
ensure_include '/root/.unikraft_akira/apps/redis/build/liblwip/origin/lwip-2.1.2/src/api/netdb.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_1.cocci' '/root/.unikraft_akira/apps/redis/build/libredis/origin/redis-5.0.6/deps/hiredis/net.c' --in-place
ensure_include '/root/.unikraft_akira/apps/redis/build/libredis/origin/redis-5.0.6/deps/hiredis/net.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_2.cocci' '/root/.unikraft_akira/apps/redis/build/libredis/origin/redis-5.0.6/src/ae_select.c' --in-place
ensure_include '/root/.unikraft_akira/apps/redis/build/libredis/origin/redis-5.0.6/src/ae_select.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_3.cocci' '/root/.unikraft_akira/apps/redis/build/libredis/origin/redis-5.0.6/src/anet.c' --in-place
ensure_include '/root/.unikraft_akira/apps/redis/build/libredis/origin/redis-5.0.6/src/anet.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_4.cocci' '/root/.unikraft_akira/apps/redis/build/libredis/origin/redis-5.0.6/src/server.c' --in-place
ensure_include '/root/.unikraft_akira/apps/redis/build/libredis/origin/redis-5.0.6/src/server.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_5.cocci' '/root/.unikraft_akira/libs/lwip/init.c' --in-place
ensure_include '/root/.unikraft_akira/libs/lwip/init.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_6.cocci' '/root/.unikraft_akira/libs/lwip/mailbox.c' --in-place
ensure_include '/root/.unikraft_akira/libs/lwip/mailbox.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_7.cocci' '/root/.unikraft_akira/libs/lwip/mutex.c' --in-place
ensure_include '/root/.unikraft_akira/libs/lwip/mutex.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_8.cocci' '/root/.unikraft_akira/libs/lwip/netbuf.c' --in-place
ensure_include '/root/.unikraft_akira/libs/lwip/netbuf.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_9.cocci' '/root/.unikraft_akira/libs/lwip/semaphore.c' --in-place
ensure_include '/root/.unikraft_akira/libs/lwip/semaphore.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_10.cocci' '/root/.unikraft_akira/libs/lwip/sockets.c' --in-place
ensure_include '/root/.unikraft_akira/libs/lwip/sockets.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_11.cocci' '/root/.unikraft_akira/libs/lwip/uknetdev.c' --in-place
ensure_include '/root/.unikraft_akira/libs/lwip/uknetdev.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_12.cocci' '/root/.unikraft_akira/libs/pthread-embedded/attributes.c' --in-place
ensure_include '/root/.unikraft_akira/libs/pthread-embedded/attributes.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_13.cocci' '/root/.unikraft_akira/libs/pthread-embedded/pte_osal.c' --in-place
ensure_include '/root/.unikraft_akira/libs/pthread-embedded/pte_osal.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_14.cocci' '/root/.unikraft_akira/unikraft/lib/uk9p/9pdev.c' --in-place
ensure_include '/root/.unikraft_akira/unikraft/lib/uk9p/9pdev.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_15.cocci' '/root/.unikraft_akira/unikraft/lib/uk9p/9preq.c' --in-place
ensure_include '/root/.unikraft_akira/unikraft/lib/uk9p/9preq.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_16.cocci' '/root/.unikraft_akira/unikraft/lib/uklock/include/uk/mutex.h' --in-place
ensure_include '/root/.unikraft_akira/unikraft/lib/uklock/include/uk/mutex.h'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_17.cocci' '/root/.unikraft_akira/unikraft/lib/uklock/include/uk/semaphore.h' --in-place
ensure_include '/root/.unikraft_akira/unikraft/lib/uklock/include/uk/semaphore.h'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_18.cocci' '/root/.unikraft_akira/unikraft/lib/uknetdev/netdev.c' --in-place
ensure_include '/root/.unikraft_akira/unikraft/lib/uknetdev/netdev.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_19.cocci' '/root/.unikraft_akira/unikraft/lib/uksched/sched.c' --in-place
ensure_include '/root/.unikraft_akira/unikraft/lib/uksched/sched.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_20.cocci' '/root/.unikraft_akira/unikraft/lib/uktime/time.c' --in-place
ensure_include '/root/.unikraft_akira/unikraft/lib/uktime/time.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_21.cocci' '/root/.unikraft_akira/unikraft/lib/vfscore/pipe.c' --in-place
ensure_include '/root/.unikraft_akira/unikraft/lib/vfscore/pipe.c'
spatch --sp-file '/root/.unikraft_akira/apps/redis/helper/cocci/rule_22.cocci' '/root/.unikraft_akira/libs/lwip/init.c' --in-place || true
