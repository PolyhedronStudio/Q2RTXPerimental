# M7 actor integration foundation — 2026-10-04

Disposition: **source integration implemented; native acceptance BLOCKED; M7 remains IN PROGRESS and M8 remains TODO.** Production actor scheduling defaults off (`nav_actor_worker 0`). No native M7 pass, performance improvement or general bug-free claim is made.

## Implemented source outcome

- Synchronous and worker requests use the same scalar preparation function. Exact submitted graph endpoints, indexed prior-corridor preference, hull, shape, traversal limits, tactical costs and full actor/publication identity are copied on the owner. No live entity or callback enters the worker.
- Actors expose Idle, Pending, Ready, NoRoute, Stale, Cancelled and ResourceLimited states. A matching pending request survives repeated force callers. Terminal worker failures obey retry backoff; saturation does not invoke an unlimited synchronous escape. Unknown virtual policies retain their explicit owner-only compatibility path.
- At the existing beginning-of-frame delivery point, replies are copied into actor-owned retained storage and the transport lease is released **before** native validation, revision issuance or callbacks. Allocation exceptions also release the lease before propagating. Native completion uses the same funnel, full hull proof and installation helper as synchronous computation.
- Delayed completion checks current actor/order/publication, latest sequence, native policy and tactical cost. The actor must still occupy a face in the solved corridor. Native attachment explicitly starts from current feet; the graph result retains its original submitted start and exact goal. Native validation preserves the exact requested goal and rejects unsafe attachment.
- A still-owned installed route can guide a goal-only replacement/backoff. State, crowd, incarnation, epoch, geometry and native policy remain strict. Obsolete-goal callbacks are suppressed. Pending movement without usable geometry holds XY and remains active; reaching an old endpoint while pending also holds without reporting new-goal arrival. Realized current-goal arrival cancels a pending same-goal refresh before it can reinstall a completed route.
- Explicit route reset and pool reset cancel actor transport ownership before clearing/reusing actor storage. Mode switches stop/join before replacement. Unsupported service capacity backs off for the same publication; temporary publication preparation remains retryable. Diagnostic jobs have an exclusive service mode, so their numeric keys cannot commit actor movement or be cancelled by an unrelated actor reset.
- Worker native commit and synchronous crowd guide construction have distinct owner performance phases. Crowd guides retain the corrected copied-graph synchronous contract; assignment and live order state remain on the owner. Their runtime measurement and compiled behavior acceptance remain outstanding.
- Opt-in capture adds copied request/accepted identities and query state. Authored semantic replay also owns the complete physical policy, avoiding rejection by the new movement gate. The native matrix runner accepts `--actor-worker` and `--libdir` and freezes/hashes the actual selected module. Synthetic shadow and actor activation cannot be combined in one run.

## Verified evidence

All **five standalone targets pass** on this slice:

| Target | Checks |
|---|---:|
| Corner geometry | 19 |
| Native movement contract | 28 |
| Pure query contexts | 8,093, including 4,000 simultaneous-context solves with allocation/free forbidden |
| Bounded worker service | 2,322, including 100 start/stop cycles |
| Actor ownership/policy/backoff predicates | 78 |

The new actor test calls production predicates. It varies every identity dimension, every native policy field/axis, all four tactical cost fields and terminal forced-retry behavior. It does **not** simulate engine collision, the AI state machines or native route installation. A source inventory separately checks that the policy predicate covers every declared native policy field except the two intentionally owner-only callback/entity pointers.

Seven relevant production object targets compile: monster base, compiled debug dummy, path, worker, capture, performance and crowd manager. This is object compilation, **not** a successful module link or ABI/native execution result. Fifteen reviewed navigation/test/harness inputs are hashed in `out/build/m7-slice-inputs.json`; machine-readable disposition is [m7-actor-foundation-2026-10-04.json](m7-actor-foundation-2026-10-04.json).

Commands/evidence:

- `cmd /c out\build\run-m7-objects.cmd` — targeted production object checks; logs `m7-navigation-objects-final.log`, `m7-navigation-objects-v2.log`, and the final `m7-navigation-objects-v3.log` under `out/build`.
- `cmd /c out\build\run-m7-standalone.cmd` — configure/build/check standalone targets; `m7-standalone-final.log` and copied detailed output `m7-standalone-detailed.log`.
- `python -m py_compile scripts/test_nav_corner_matrix.py` and `--help` — syntax/interface checks only; the new native options have not yet been executed against a linked M7 module.

## Build dependency and preservation

The initial full server-game build failed in the concurrently edited skeletal-animation refactor: `sg_skm.cpp` line 404 has incompatible faux-animation types, line 502 passes `model_t` to a `skm_model_t` API, and lines 1018–1027 reference removed IQM fields. The retained log is `out/build/m7-build-first.log`. The user confirmed **another task is still editing that refactor**. Its sources were not changed by this navigation work, and its ongoing edits are not claimed as a frozen ABI.

The initial attempt temporarily redirected the game output to `out/build/m7-module/baseq2rtxp` to preserve the running interactive engine. The shared developer tree's normal DLL output has been restored and verified (`m7-build-output-restoration.log`). The prepared `run-m7-build.cmd` now uses a separate `out/build/m7-Debug` tree and private module output for future linked validation; that separate build has not yet been run. No interactive process was stopped and no engine/module was installed by this slice.

M6 reports remain historical evidence for their captured binary/source bundle. The current changing engine and module cannot inherit M6 acceptance merely because the earlier tests passed. No navigation/native failure has been waived.

## Required continuation, in dependency order

1. Finish the other task's build refactor, produce a compatible engine/module and freeze the source, binaries, maps and harnesses. Establish actual loaded module identity before native testing.
2. Validate the factored synchronous reference first, including authored replay, the lowest riser of the bend staircase and all existing stair/corner/crowd controls. Then run worker mode with identical geometry and policy oracles.
3. Add and execute 1/4/20-tick delivery delay, reverse delivery, superseded goal/state/order, same-goal arrival-refresh cancellation, pool reuse, mode switch, saturation and busy clear/load/generation/shutdown controls. Source review and predicate tests do not satisfy these native gates.
4. Implement runnable assertions for every compiled behavior row: pillar pursuit, trail, sound, cover/no-cover flee, formation/serialized ingress and egress, member death/protected leader/reassignment, idle/death, airborne/step/ramp/trap recovery. Rerun the complete C1–C12 acceptance with no waived physical failure.
5. Only after M7 passes, complete M8's optimized baseline/synchronous/worker comparison, 64-agent tick/commit and request-latency distributions, allocation/memory/lifetime checks and explicit rollout decision. Two replies with a cooperative 500us delivery boundary are the current starting budget; an indivisible native completion may exceed it. No 2ms/25ms/latency target is claimed from the current source work.

The current implementation is an opt-in integration foundation pending native acceptance. Delayed transport controls, full compiled-behavior harnesses and M8 acceptance are intentionally unfinished, rather than silently marked complete.
