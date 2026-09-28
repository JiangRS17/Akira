# Akira

Akira is an operating-system architecture for agile specialization. Modular components stay in place. How they are grouped, how a call crosses a group, and how strong that boundary is are set in a configuration file and compiled into the image, instead of being fixed in the components.

The architecture is described in *Akira: Towards Operating System Architecture for Agile Specialization* (Jiang et al., ACM Transactions on Embedded Computing Systems, 2026). It separates three jobs that existing modular systems leave tangled:

| Unit | What the paper assigns to it | Where this tree does that job |
| --- | --- | --- |
| **AEU** | Hide a component's calling convention, interface contract, and isolation *requirements*. The caller issues one common invocation. Lifetime, transition, and enforcement stay inside the selected backend. | `unikraft/fabric-support/` reads `fabric.yaml`, emits the interface registry, and uses Coccinelle to rewrite `flexos_nop_gate` sites to `fabric_gate`. |
| **CLU** | Own routing and dispatch outside the callee: a registry, a route over the communication fabric, and an arbiter for contended cross-domain traffic. Calls inside a domain stay native. | `unikraft/lib/fabric-core/controller/`, `map/`, `arbiter/`, `fabric/`, `message/`. |
| **BIU** | Group components that share an isolation need into a domain, and bind one backend. Switching MPK and EPT changes the binding, not the call sites. | `unikraft/lib/fabric-core/bridge/`, `backends/`. `fabric_bridge_set_isolation` selects the backend; `init` / `enter` / `leave` implement its lifetime, transition, and enforcement. |

A cross-domain call is `fabric_gate(source, function, ...)`. The macro builds a `FabricMsg`. The controller routes it over the fabric graph (a depth-first walk of the configured edges). The bridge backend performs the domain switch, and the callee runs in the target fabric. Registration, placement, and the backend are data, so moving a boundary does not require editing the library that exports the function.

In the paper, BIU may derive domains from per-component backend constraints and a profiled communication matrix. This snapshot takes the result of that decision as input: `fabric.yaml` names the fabrics, the libraries in each one, the edges, and a single isolation backend for the image. The Unikraft backends evaluated in the paper are Intel MPK (shared execution, key-tagged access) and EPT (separate guest-physical mappings). The tree also builds a software backend, used by the microbenchmark ablations, and registers TrustZone (`FABRIC_ISO_TRUSTZONE`) as a stub.

## What this repository is

This tree is the Unikraft prototype from the paper: a fork of [FlexOS](https://github.com/project-flexos/unikraft) (ASPLOS 2022), itself a fork of [Unikraft](https://unikraft.org) (EuroSys 2021). It drives the paper's Unikraft experiments: Redis and nginx under different domain splits, hardening, and backends, plus the cross-domain and scalability microbenchmarks. The FreeRTOS prototype in the same paper (InfiniTime, MPU, privilege switching, Embench) is not included. Paper figures and the scripts that draw them are not part of the release.

On this prototype the paper reports a Redis throughput range of 97.7–1017.7 kreq/s across MPK safety configurations (up to 10.4×), and up to 7.8× nginx throughput variation across domain count, placement, hardening, and MPK versus EPT. The harnesses under `apps/` are what produce those measurements. The numbers and plots are not stored here.

FlexOS is kept at one compartment (`comp1`) on purpose. Fabric domains, not FlexOS protection keys, own isolation inside that image. That avoids two MPK users fighting over the same keys.

## Repository layout

```
unikraft/                  Unikraft / FlexOS kernel
  lib/fabric-core/         Soft bus, controller, bridge, isolation backends
  fabric-support/          Topology parser, registration, Coccinelle rewrite, build
  lib/example1 … example8  Small libraries used by the microbenchmarks
  lib/flexos-core/         FlexOS compartment runtime this prototype builds on
libs/                      External libraries (newlib, lwIP, nginx, Redis, TLSF, …)
apps/
  fabric-example/          Smallest Fabric image
  base-overhead/           Cross-domain call and path ablation
  extension-overhead/      Cost as one component set is split across more domains
  scalability/             Fabric count, depth, and libraries per fabric
  nginx/  redis/           Workload matrices (domain split × hardening × backend)
  iperf/                   Throughput workload
  flexos-example/          Upstream FlexOS hello world, kept for comparison
archs/  plats/             Architecture and platform link configuration
```

Each benchmark application has two inputs:

- `kraft.yaml` — Unikraft target, libraries, and the single FlexOS compartment.
- `fabric.yaml` — Fabric domains, which libraries sit in each domain, the edges between domains, and the isolation backend.

`apps/<name>/helper/` is generated. `unikraft/fabric-support/build_fabric.sh` rewrites it on every build.

## Requirements

- Linux x86_64 with KVM.
- A CPU with user-mode memory protection keys (PKU/MPK) for MPK configurations. Guest QEMU must be started with `-cpu host`.
- The FlexOS host toolchain: GCC, Make, Python 3, PyYAML, [kraft](https://github.com/project-flexos/kraft) from the FlexOS fork, and [Coccinelle](https://github.com/coccinelle/coccinelle). Package names are listed in `unikraft/docker/flexos.dockerfile` (Debian 10).
- EPT configurations need the patched QEMU built at the bottom of that same Dockerfile (`qemu-system-ept`). Stock QEMU is enough for MPK and the software backend.

The Dockerfile clones upstream FlexOS into `/root/.unikraft` and is only a dependency list. This snapshot already vendors `unikraft/` and `libs/`; build it in place, and do not replace it with `kraft list pull`.

The TrustZone backend is registered (`FABRIC_ISO_TRUSTZONE`) and is a stub. The evaluated backends are MPK, EPT, and software. MPU and privilege switching appear in the paper's FreeRTOS prototype and are not built here.

## Build and run

From a configured application directory:

```sh
cd apps/fabric-example
kraft configure
sh ../../unikraft/fabric-support/build_fabric.sh .
kraft run
```

`build_fabric.sh <app-dir>` is the supported build. It restores previously rewritten sources, reads `fabric.yaml`, generates registration and per-library linker scripts, builds once so Coccinelle can see instantiated gates, rewrites cross-fabric `flexos_nop_gate` sites to `fabric_gate`, and rebuilds. A plain `make` skips that rewrite.

`apps/fabric-example` is the short path: three fabrics in one compartment, EPT in `fabric.yaml`. Swap the `isolation.backend` field in that file to try another backend, then rerun `build_fabric.sh`.

## Evaluation

Microbenchmarks print cycles on the guest serial console. Workload drivers also write CSV files under `apps/<name>/benchmark/`. Nothing in this tree turns those numbers into figures.

| Directory | What it measures | Paper | How to run |
| --- | --- | --- | --- |
| `apps/base-overhead` | Direct call vs. `fabric_gate`, and ablations of the gate path | Cross-domain round trip (Sec. 6.2) | `kraft configure && sh ../../unikraft/fabric-support/build_fabric.sh .` |
| `apps/extension-overhead` | Same call as the component set is partitioned into 1, 2, 4, or 8 domains (`G0`–`G3`) | Domain splits and multi-hop chains (Sec. 6.1–6.2) | See `apps/extension-overhead/README.md`. `./run_all.sh` |
| `apps/scalability` | Many fabrics, deep topologies, and libraries per fabric | Fabric depth, width, and MPK domain count (Sec. 6.4) | `./run.sh` after a build. `./run_domains.sh` and `./run_grid.sh` select the other harnesses |
| `apps/nginx` | nginx request rate under the N01–N07 domain and hardening presets | nginx throughput under MPK and EPT (Sec. 6.1) | `./configure.sh n01-1d-mpk-none --build`, then `./benchmark/run_one.sh n01-1d-mpk-none` |
| `apps/redis` | Redis GET/SET under the R01–R07 presets, plus the earlier C01–C07 isolation layouts | Redis GET/SET under MPK safety configurations (Sec. 6.1) | `./benchmark/run_matrix.sh` or `./benchmark/run_matrix.sh r01-1d-mpk-none`. Notes in `apps/redis/RUN.md` |
| `apps/iperf` | TCP throughput on the Fabric image | — | FlexOS iperf app; configure with `kraft`, then `build_fabric.sh` |

Nginx presets (same layout for MPK and EPT):

| Id | Domains | Split | Hardening |
| --- | --- | --- | --- |
| `n01-1d-mpk-none` | 1 | nginx, newlib, scheduler, lwIP together | none |
| `n02-2d-all-harden` | 2 | nginx+newlib \| scheduler+lwIP | all four |
| `n03-3d-none` | 3 | nginx+newlib \| scheduler \| lwIP | none |
| `n04-3d-nginx-harden` | 3 | same split as n03 | nginx |
| `n05-2d-nginx-harden` | 2 | same split as n02 | nginx |
| `n06-2d-3harden` | 2 | same split as n02 | nginx, scheduler, lwIP |
| `n07-2d-all-harden` | 2 | same split as n02 | all four |

Redis R01–R07 follow the same pattern with Redis in place of nginx. Preset sources are `apps/nginx/configs/` and `apps/redis/configs/`. The paper's nginx and Redis sweeps vary domain count (1–3), which libraries share a domain (application, libc, scheduler, TCP/IP), and which of those libraries are hardened.

## Relation to upstream

Unikraft builds a single-address-space library OS. FlexOS compiles that graph into compartments and selects a protection mechanism per compartment. That selection stays inside Unikraft's component organization: changing the mechanism still means annotating compartments and gates in that graph.

Akira does not replace either system. On this prototype it sits inside one FlexOS compartment and specializes a second, finer graph. Libraries are grouped into fabrics, edges become the soft bus, and the bridge backend supplies the protection that FlexOS would otherwise fix for the whole compartment. The same AEU / CLU / BIU split is what the paper applies to FreeRTOS; only the Unikraft side is in this tree.

Bugs in `unikraft/lib/` outside `fabric-core/`, and in the original FlexOS gate machinery, belong upstream. Issues with `fabric-core`, `fabric-support`, or the Akira `apps/` harnesses belong here.

## License

Unikraft sources are under the BSD 3-clause license in [`unikraft/COPYING.md`](unikraft/COPYING.md), unless a file or directory says otherwise. Libraries under `libs/` keep their own licenses (see each `COPYING` or `README`).

Akira-specific code in this snapshot — `unikraft/lib/fabric-core`, `unikraft/fabric-support`, and the Fabric benchmark applications — is released under that same BSD 3-clause license.

## Citation

If you use this prototype, please cite:

```bibtex
@article{jiang2026akira,
  author  = {Jiang, Renshuang and Dong, Pan and Wang, Yichong and Li, Lei and
             Dong, Yuchen and Fang, Xiaoxiang and Yu, Qirui and Zhang, Jianfeng and
             Jiang, Zhe},
  title   = {Akira: Towards Operating System Architecture for Agile Specialization},
  journal = {ACM Transactions on Embedded Computing Systems},
  year    = {2026}
}
```
