# Navmesh Door/Slope/Stair Hardening Masterplan (Audit Checklist)

## Document Control
- **Scope:** `src/baseq2rtxp/svgame/nav/nav_generate.cpp` (+ minimal related nav files only if required)
- **Mode:** Planning + audited execution checklist
- **Execution Rule:** Do **not** start implementation until explicitly approved
- **Change Policy:** Surgical refactors only, no hacks, no baseline restore/replacement

---

## Non-Negotiable Invariants (Must Hold)

### Door Invariants
- [ ] Door transition edges are generated only from **outer visible door geometry**.
- [ ] Door transition edges are **not** generated from geometry contained within same-door brush volumes.
- [ ] Compound convex door shapes are supported correctly.
- [ ] No malformed doorway seam.
- [ ] No triple-line doorway split artifact.
- [ ] Missing blocked/non-traversable edges near door are restored correctly.

### Slope Invariants
- [ ] Pyramid/slope scenario does not regress into random blocked edges.
- [ ] Slope seam linking remains stable and does not over-link parallel/adjacent surfaces.

### Stair Invariants
- [ ] First stair step has **zero blocked/non-traversable edges**.
- [ ] Consecutive step-to-step seams are traversable.
- [ ] Side seams within `NAV_MAX_STEP_SIZE` are traversable.

### Runtime Edge-State Invariants
- [ ] `edge_entity_id` is assigned only for valid dynamic/world transition seams.
- [ ] Entity edge registry has complete and non-duplicated door edge sets.
- [ ] Door/wall callbacks correctly toggle intended edges only.

### Process/Quality Invariants
- [ ] No hack logic or ad hoc bypasses.
- [ ] Minimal additional diagnostics (bounded counters only).
- [ ] All introduced compile errors are fixed before handoff.

---

## Card-Based Execution Plan (Strict Step Audit)

## Card 01 — Freeze Acceptance Matrix
**Inputs**
- User requirements and screenshots
- Existing diagnostics contract

**Files Touched**
- Planning document only

**Mutation Budget**
- 0 code lines

**Checklist**
- [ ] Every invariant above mapped to a measurable pass/fail signal
- [ ] Regression signatures defined (door malformed seam, triple split, missing blocked edges, slope random blocked edges)

**Pass/Fail Probe**
- Pass: all invariants unambiguous and measurable
- Fail: any rule is open to interpretation

**Rollback Condition**
- Stop and refine matrix before code edits

### Card 01 Audit Evidence

| Invariant | Observable probe | Pass condition | Failure signature |
|-----------|------------------|----------------|-------------------|
| Outer compound-door geometry only | Inspect emitted transition fragments and registered `edge_entity_id` pairs by entity. | Every transition pair borders world geometry and belongs to a non-contained brush volume. | Internal same-door brush produces a portal/registered edge. |
| One doorway seam | Inspect transition-edge collinear intervals after all twin passes. | Each physical door boundary interval has one paired half-edge portal, split only at legitimate polygon vertices. | Two or more co-linear transition strips represent the same doorway boundary. |
| Closed-door blocking | Close a door and inspect the dynamic entity's registered half-edges. | Every valid transition pair is disabled symmetrically; no unrelated edge changes. | Expected boundary remains enabled or an unrelated world/stair edge is disabled. |
| Pyramid/slope preservation | Generate the known pyramid map and inspect all slope adjacency pairs. | Only geometrically coincident, walkable seams within step policy twin; no arbitrary slope boundary is flagged dynamic or blocked. | Random blocked/debug-colored slope edge or a lateral false twin. |
| Stair first step | Inspect every first-step face edge after generation. | No edge is dynamic, disabled, or otherwise non-traversable solely due to the adjoining floor/step seam. | First-step transition is rendered/queried as blocked. |
| Consecutive/side stair steps | Inspect edges with a compatible horizontal overlap and vertical delta within `NAV_MAX_STEP_SIZE`. | The edge twins as a regular traversable portal when geometry represents an adjacent step, including valid side contacts. | Adjacent step seam remains a non-traversable boundary. |

**Card 01 Result**
- [x] Invariants are mapped to mesh and runtime probes.
- [x] Door, slope, and stair failure signatures are distinct.
- [x] No implementation thresholds or geometry behavior changed.

---

## Card 02 — Pipeline Ownership Contract Audit
**Inputs**
- Extraction, split, partition, splice, twin-link stages

**Files Touched**
- `nav_generate.cpp` (read-only in this card)

**Mutation Budget**
- 0 code lines

**Checklist**
- [ ] Stage-by-stage ownership transitions documented for `(entity_id, transition_entity_id, edge_entity_id)`
- [ ] Illegal state transitions identified

**Pass/Fail Probe**
- Pass: explicit ownership contract across all stages
- Fail: any silent ownership mutation remains unexplained

**Rollback Condition**
- Defer implementation until contract is complete

### Card 02 Audit Evidence

| Stage | Input ownership | Allowed output ownership | Audit finding |
|-------|-----------------|--------------------------|---------------|
| Brush collection | `nav_brush_ownership_t` carries `model_num`, `instance_id`, and door `entity_id`. | World is `ENTITYNUM_NONE`; door/wall inline models retain their runtime entity number. | `CollectModelBrushes` preserves ownership per active model instance. |
| World-floor extraction | World winding has no entity ownership. | World fragment stays `(NONE, NONE)` unless cut by an eligible dynamic brush. | `Nav_DoExtractionWork` skips dynamic brush floor extraction and delegates each world-floor cut to `SplitWindingsByEntityBrush`. |
| Door split | A world fragment enters a dynamic brush volume. | Exterior cut fragments carry `(NONE, door)`; interior fragment carries `(door, door)`. | `SplitWindingsByEntityBrush` assigns transition identity to every generated exterior plane fragment; this is the primary duplicate-strip risk for compound brushes. |
| Polygon emission | Winding ownership values. | Exact values copied into `nav_poly_t`. | Extraction preserves both fields without normalization. |
| Partition | `nav_poly_t` fields. | Dynamic and transition polygons are preserved unsplit. | Partition excludes `entity_id != NONE` and `transition_entity_id != NONE` from split candidates and preserves them in child buckets. |
| T-junction splice | Polygon ownership and nearby geometry. | Only matching `entity_id` domains may mutate. | Transition polygons are skipped as both source and target; regular world/slope repair remains separate. |
| Half-edge creation | Face derives `entity_id` from source polygon. | New edges begin unowned. | Face carries `entity_id`; `transition_entity_id` is not retained at face level. This means final door semantics rely on a dynamic/world twin pair rather than transition marker identity. |
| Twin linking | Geometrically matching unresolved half-edges. | Twin pair may receive door owner only for dynamic/world face pair. | All current pairing passes can call `AssignNavEntityEdgeMetadata`, which marks a pair only when exactly one face is dynamic. |
| Runtime registry | Dynamic/world half-edge pair. | Both directed edges are registered to one entity. | `RegisterNavEntityEdge` deduplicates indices; `Nav_SetEntityEdgesState` updates the registered directed edges. |
| Door callbacks | Door entity number. | Closed sets `NAV_EDGE_DISABLED`; opening clears it. | Linear and rotating doors share the same callback path; rotating doors delegate close/open completion to `svg_func_door_t`. |

**Illegal State Transitions to Prevent**
- [x] A same-door interior/contained brush must not create a dynamic/world portal.
- [x] A transition marker must not be lost before its valid door boundary is resolved.
- [x] A regular world/slope/stair twin must not gain `edge_entity_id`.
- [x] A dynamic/world transition must not be paired with a second overlapping candidate.

**Card 02 Result**
- [x] Ownership contract is documented through collection, extraction, partition, splice, linking, and runtime toggle.
- [x] The primary topology-risk boundary is isolated to compound dynamic brush splitting and unconstrained matching of its emitted fragments.

---

## Card 03 — Compound-Door Containment Rule Design
**Inputs**
- Brush ownership model
- Door compound convex requirement

**Files Touched**
- Design notes only

**Mutation Budget**
- 0 code lines

**Checklist**
- [ ] Contained vs outer door brush classification rule defined
- [ ] Intersect/contain conflict policy for same entity defined
- [ ] Deterministic ordering rule defined

**Pass/Fail Probe**
- Pass: no path for contained/internal brushes to emit transition seams
- Fail: containment rule ambiguous

**Rollback Condition**
- Redesign containment criteria before coding

### Card 03 Design Decision

**Door volume model**
- Group eligible dynamic brush instances by the same runtime `entity_id` and `instance_id`.
- Treat each active brush as a closed convex volume in world coordinates.
- A brush is *contained* only when every vertex of its constructed convex boundary lies on or inside another brush in the same group, within one shared containment epsilon. Partial intersections, shared faces, and merely overlapping volumes are **not** containment.
- Process eligible brushes in ascending `brush_num` order. The order is a determinism rule only; the result is the geometric union of the eligible volumes.

**Transition emission rule**
- Split a walkable world winding against the union of a door entity's non-contained brushes.
- Preserve outside pieces as ordinary world fragments: `(ENTITYNUM_NONE, ENTITYNUM_NONE)`.
- Mark only the final union-interior pieces as dynamic: `(door_entity_id, door_entity_id)`.
- Do not use an exterior `transition_entity_id` strip as a partition/splice guard. A valid transition is created solely when a world face and a dynamic union-interior face share one geometrically legal portal.
- Multiple non-contained brushes may contribute multiple outer boundary intervals when their union is genuinely compound; co-linear/overlapping intervals are normalized before half-edge pairing rather than represented by duplicate transition fragments.

**Eligibility and conflict rule**
- Brushes from different entities never participate in the same union or containment group.
- A contained brush is skipped as a union modifier because it cannot add an outer boundary.
- A partially overlapping brush remains eligible because it can expand the outer boundary of the compound door volume.
- A valid dynamic portal requires exactly one world face and one face owned by the same door entity; dynamic/dynamic interfaces are internal and never receive `edge_entity_id`.

**Card 03 Result**
- [x] Containment is full convex enclosure, not overlap.
- [x] Outer visible geometry is represented by a per-door union instead of protected exterior transition strips.
- [x] Stable brush ordering and domain-conflict rules are defined.

---

## Card 04 — Refactor Skeleton (Behavior-Neutral)
**Inputs**
- Ownership contract + containment design

**Files Touched**
- `nav_generate.cpp`

**Mutation Budget**
- ≤ 4 helper extractions/introductions
- ≤ 220 changed lines

**Checklist**
- [ ] Responsibilities split into clear helper boundaries
- [ ] No threshold changes yet
- [ ] Build compiles

**Pass/Fail Probe**
- Pass: cleaner structure, same behavior baseline
- Fail: behavior drift or compile break

**Rollback Condition**
- Revert this card patch only

### Card 04 Audit Evidence
- [x] Extracted `IsDynamicTransitionBrush` as the sole behavior-neutral classification point for runtime mover brush instances.
- [x] Replaced the two direct extraction-loop ownership tests with the helper; no thresholds, ownership values, clipping order, or pairing logic changed.
- [x] File diagnostics report no errors after the refactor.
- [ ] Full workspace build deferred to Card 13 after functional changes are complete.

---

## Card 05 — Implement Door Containment Classification
**Inputs**
- Card 03 design

**Files Touched**
- `nav_generate.cpp`

**Mutation Budget**
- ≤ 2 new helpers
- ≤ 180 changed lines

**Checklist**
- [ ] Same-entity contained door brushes excluded from seam eligibility
- [ ] Outer-visible brushes preserved as valid emitters

**Pass/Fail Probe**
- Pass: contained geometry no longer contributes transition seams
- Fail: contained seam leakage persists

**Rollback Condition**
- Revert containment hook only

### Card 05 Audit Evidence
- [x] Added `IsBrushFullyContainedByBrush`, which constructs candidate convex boundary faces and verifies every generated vertex remains inside each active container plane.
- [x] Added `IsContainedDynamicTransitionBrush`, restricted to same `entity_id` and `instance_id`; it excludes full enclosure only, preserving partially overlapping compound geometry.
- [x] Equal convex volumes select the lower `brush_num` deterministically; unrelated entities and model instances are never grouped.
- [x] Contained dynamic brushes are skipped before world-floor clipping.
- [x] `nav_generate.cpp` file diagnostics report no errors.
- [x] Added a final-stage-only portal geometry sample: at most eight unique portal pairs per dynamic entity, including both edge spans, face IDs, and vertical delta.
- [ ] Use the portal geometry sample to distinguish misplaced dynamic/world pairing from missing perimeter coverage in the remaining door-edge regression.

---

## Card 06 — Transition Fragment Emission Hardening
**Inputs**
- `SplitWindingsByEntityBrush` behavior + containment output

**Files Touched**
- `nav_generate.cpp`

**Mutation Budget**
- 1 primary function + optional helper
- ≤ 200 changed lines

**Checklist**
- [ ] Transition fragments emitted once per valid seam domain
- [ ] No redundant parallel strips at doorway

**Pass/Fail Probe**
- Pass: single intended doorway seam
- Fail: duplicate/triple split remains

**Rollback Condition**
- Revert emission logic block only

### Card 06 Audit Evidence
- [x] Dynamic brush clipping now preserves previously generated dynamic interiors so sequential same-door brushes form a geometric union instead of re-cutting internal door regions.
- [x] Exterior fragments remain ordinary world geometry and no longer carry `transition_entity_id` as a partition/splice protection strip.
- [x] Only union-interior fragments retain the door `entity_id`; later edge metadata therefore requires a direct dynamic/world half-edge pair.
- [x] The change removes the mechanism that could preserve parallel exterior transition strips through partition and splice stages.

---

## Card 07 — Partition Ownership Preservation
**Inputs**
- Partition stage rules

**Files Touched**
- `nav_generate.cpp`

**Mutation Budget**
- ≤ 120 changed lines

**Checklist**
- [ ] Partition preserves transition semantics
- [ ] Dynamic/transition polygons not degraded into invalid world seams

**Pass/Fail Probe**
- Pass: partition cannot strip required transition identity
- Fail: transition metadata loss/misrouting

**Rollback Condition**
- Revert partition gating changes only

### Card 07 Audit Evidence
- [x] Primary and fallback partition candidate selection both exclude dynamic and legacy transition-owned polygons.
- [x] Dynamic union interiors are preserved whole, retaining the exact geometry whose perimeter can become a door portal.
- [x] Preservation diagnostics now distinguish dynamic union interiors from legacy transition-marked polygons.
- [x] File diagnostics report no errors.

---

## Card 08 — T-Junction Splice Domain Safety
**Inputs**
- Splice stage rules

**Files Touched**
- `nav_generate.cpp`

**Mutation Budget**
- ≤ 170 changed lines

**Checklist**
- [ ] Splice cannot cross invalid domain boundaries
- [ ] Splice does not corrupt door/slope/stair legal seams

**Pass/Fail Probe**
- Pass: no malformed doorway seam introduced during splice
- Fail: splice creates/propagates regression signature

**Rollback Condition**
- Revert splice domain changes only

### Card 08 Audit Evidence
- [x] Dynamic union interiors are excluded as both source and target of T-junction repair.
- [x] World-only splice behavior remains available for ordinary world, slope, and stair topology.
- [x] No cross-domain splice can manufacture or deform a doorway seam after union extraction.
- [x] File diagnostics report no errors.

---

## Card 09 — Unified Twin-Link Domain Gates
**Inputs**
- Deterministic arbitration + pass1 + pass2

**Files Touched**
- `nav_generate.cpp`

**Mutation Budget**
- ≤ 3 shared gate helpers
- ≤ 260 changed lines

**Checklist**
- [ ] Same domain legality checks in all linking passes
- [ ] No pass-specific bypass path

**Pass/Fail Probe**
- Pass: identical legal pairing policy across passes
- Fail: one pass still links illegal pairs

**Rollback Condition**
- Revert unified gate integration only

### Card 09 Audit Evidence
- [x] Added `AreHalfEdgesInCompatibleNavigationDomains` as the common ownership gate used before deterministic, endpoint, and overlap geometry scoring.
- [x] World/world seams remain legal regular portals; world/dynamic seams remain legal runtime door portals.
- [x] Dynamic/dynamic seams are legal only within one compound mover and can never receive door metadata because both faces are dynamic.
- [x] Different dynamic entities cannot be paired by any twin-link pass.
- [x] Existing geometric requirements (near-collinearity, lateral separation, overlap, and maximum vertical delta) remain in place for slope and stair safety.
- [x] `nav_generate.cpp` file diagnostics report no errors.

---

## Card 10 — Stair Traversability Rule Enforcement
**Inputs**
- Stair invariants + max step threshold policy

**Files Touched**
- `nav_generate.cpp` (and only if required, `nav_path.cpp`)

**Mutation Budget**
- ≤ 180 changed lines

**Checklist**
- [ ] First step: zero blocked edges
- [ ] Consecutive step seams traversable
- [ ] Side seams within max step height traversable

**Pass/Fail Probe**
- Pass: all stair invariants hold in debug topology + runtime behavior
- Fail: any stair seam incorrectly blocked

**Rollback Condition**
- Revert stair classification block only

### Card 10 Audit Evidence
- [x] The common twin-domain gate preserves world/world eligibility; it does not attach `edge_entity_id` or disabled flags to ordinary stair or slope portals.
- [x] Existing pairing passes still require collinearity, bounded lateral separation, real overlap, and a vertical delta no greater than `NAV_MAX_STEP_SIZE + 4.0f` before creating a stair-capable twin.
- [x] A* retains its directional step/drop policy after topology creation; a connected consecutive or side step within policy is traversable because neither directed edge is dynamic-disabled.
- [x] The first stair step cannot acquire a blocked flag from regular world topology; `NAV_EDGE_DISABLED` remains exclusively entity-transition state.
- [x] File diagnostics report no errors in `nav_path.cpp`.

---

## Card 11 — Runtime Metadata/Registry Integrity
**Inputs**
- `AssignNavEntityEdgeMetadata`, `RegisterNavEntityEdge`, `Nav_SetEntityEdgesState`

**Files Touched**
- `nav_generate.cpp`
- `nav_path.cpp` (if required)

**Mutation Budget**
- ≤ 90 changed lines

**Checklist**
- [ ] Complete entity edge registration for valid door seams
- [ ] No duplicate inflation
- [ ] Door/wall toggles affect correct edge set

**Pass/Fail Probe**
- Pass: blocked edge toggling correctness restored and stable
- Fail: missing/extra toggles remain

**Rollback Condition**
- Revert metadata/registry patch only

### Card 11 Audit Evidence
- [x] `AssignNavEntityEdgeMetadata` remains adjacency-based: it assigns an owner and registers both directed edges only when exactly one linked face is dynamic.
- [x] A fresh `Nav_BuildHalfEdgeMesh` now clears `g_nav_entity_edges` before creating new half-edge indices, preventing a regenerated mesh from retaining stale registrations.
- [x] `Nav_SetEntityEdgesState` now ignores invalid registered indices instead of dereferencing a stale entry.
- [x] A* now rejects a portal if either directed edge is marked `NAV_EDGE_DISABLED`, so closed-door state affects traversal symmetrically without altering ordinary slope/stair edges.
- [x] Linear and rotating doors both use the shared close/open edge-state callback path.
- [x] File diagnostics report no errors in `nav_generate.cpp` and `nav_path.cpp`.

---

## Card 12 — Bounded Diagnostics Tightening
**Inputs**
- Existing `s_nav_generation_diagnostics`

**Files Touched**
- `nav_generate.cpp`

**Mutation Budget**
- ≤ 80 changed lines

**Checklist**
- [ ] Stage-local counters sufficient to localize regressions
- [ ] No per-vertex log spam

**Pass/Fail Probe**
- Pass: root-cause stage can be identified with bounded output
- Fail: noisy/unhelpful diagnostics

**Rollback Condition**
- Remove added counters and keep baseline diagnostics

### Card 12 Audit Evidence
- [x] Added aggregate `contained_dynamic_clip_skips` to identify whether full-enclosure filtering participates in a generation.
- [x] Added aggregate and per-entity dynamic/world `transition_portals` counters, incremented only after a linked portal receives runtime metadata.
- [x] Corrected the existing diagnostics format/argument alignment by explicitly reporting both dynamic and legacy transition preservation counts.
- [x] Diagnostics remain bounded: one stage summary and up to eight entity summaries, with no per-brush, per-vertex, or per-edge logging.
- [x] `nav_generate.cpp` file diagnostics report no errors.

---

## Card 13 — Build Gate
**Inputs**
- Full modified state

**Files Touched**
- none (build/test only)

**Mutation Budget**
- 0 (except compile-fix follow-ups)

**Checklist**
- [ ] Workspace builds clean

**Pass/Fail Probe**
- Pass: clean build
- Fail: introduced compile/link errors

**Rollback Condition**
- Revert smallest offending patch slice, reapply corrected change

### Card 13 Audit Evidence
- [x] Workspace build completed successfully after the containment, extraction, ownership-gate, runtime registry, and diagnostics changes.
- [x] No compile or link errors were introduced by the implemented cards.

---

## Card 14 — Regression Matrix Gate
**Inputs**
- Door good case, door broken case, slope case, stair cases

**Files Touched**
- none (validation only unless fix needed)

**Mutation Budget**
- 0 for validation

**Checklist**
- [ ] Door seam fixed (malformed removed)
- [ ] Missing blocked edges restored
- [ ] Triple-line artifact removed
- [ ] Slope case preserved
- [ ] Stair invariants preserved

**Pass/Fail Probe**
- Pass: all scenario gates pass
- Fail: any gate fails

**Rollback Condition**
- Revert only the card that introduced the failing regression

### Card 14 Runtime Audit — First Pass Failed
- [x] Pyramid slope scenario remained correct.
- [ ] Door scenario failed: adjacent wall topology was fragmented into triangular/parallel artifacts despite `entity=18` having only two extracted dynamic polygons and four registered portals.
- [ ] Stair scenario failed: the first step retained three non-traversable boundary edges.
- [x] Captured bounded evidence: `containedClipSkips=0`, `transitionPortals=4`, `dynamicPreserved=18`, and `worldSplices=239`.

### Card 14 Corrective Actions
- [x] Restrict partial-overlap deterministic arbitration to world/world seams; dynamic portals use only stricter endpoint/overlap pairing.
- [x] Enable projected step/slope T-junction subdivision within step height while preserving each surface's own Z values.
- [x] Workspace rebuild completed successfully after the corrective changes.
- [x] Added a dedicated dynamic/world portal conformance pass that subdivides only coincident same-height doorway perimeter edges at existing opposite vertices before half-edge pairing.
- [x] Workspace rebuild completed successfully after portal conformance implementation.
- [x] Reinstated `transition_entity_id` only on exterior outer-door fragments as a generation-time partition/splice perimeter guard; it is not retained by `nav_face_t` and therefore cannot become runtime door metadata.
- [x] Portal geometry audit identified all four registered entity-18 portals as a tiny `4 x 4` footprint around `(-448, -110, 0)`, proving pivot/origin helper geometry was being classified as dynamic door volume.
- [x] Hardened origin-brush texture detection to use a case-insensitive final path component and support both slash conventions before dynamic brush collection/extraction.
- [x] Confirmed `CONTENTS_ORIGIN` is documented as removed before BSP generation, so texture/content classification alone cannot establish the remaining helper-volume root cause.
- [x] Added bounded extraction diagnostics: dynamic-origin exclusions and collected dynamic-brush totals, plus up to eight collected dynamic brush records with entity/model/instance/brush IDs, contents, side count, and first texture name.
- [x] Corrected the inline-model indexing audit: `gi.modelindex("*N")`/`s.modelindex` is a one-based network configstring handle, while BSP inline models are zero-based with world at element zero. `SV_HullForEntity` resolves the collision BSP submodel as `s.modelindex - 1`; nav extraction had incorrectly used the handle directly.
- [x] Confirmed nav extraction shifts every brush plane by the dynamic entity's current origin, matching the collision system's inline-model placement convention.
- [x] Extended the bounded dynamic brush record with the applied entity offset and inline-model local/world bounds to prove or reject a placement mismatch from the next affected-map run.
- [x] Affected-map placement evidence: entity 18 / model 2 / brush 81 is correctly translated to `(-451,-166,-1)` through `(-445,-107,92)`; the `4 x 4` registered portals are a clipping/linking footprint defect, not an origin or inline-model binding defect.
- [x] Confirmed the four entity-18 portal segments passed strict dynamic/world linking; missing perimeter spans are absent before half-edge linking or never form eligible overlapping boundaries.
- [x] Added a capped extraction-stage `DynamicFootprint` inventory for emitted dynamic interiors and transition-adjacent world fragments, recording source floor brush/side, ownership, area, and world bounds.
- [x] Affected-map extraction evidence: source floor brush 79 emits only two entity-18 interiors, each `2 x 4` (`x=-448..-446` and `x=-450..-448`, `y=-112..-108`), despite brush 81's full `6 x 59` ground footprint. The loss occurs during ordered floor clipping before partitioning, conformance, and half-edge construction.
- [x] Added bounded `DynamicBlocker` extraction diagnostics that report each static brush subtraction which reduces a source floor while a dynamic footprint remains, including source brush/side, blocker brush, contents, and before/after fragment area.
- [x] Aggregate `DynamicBlocker` results were non-causal: the large source floor lost area to brushes 3–25 while a door overlap merely existed, exhausting the bounded log before proving which subtraction changed brush 81's footprint.
- [x] Replaced aggregate tracing with `DynamicOverlapBlocker`: it non-mutatively measures current fragment area inside each dynamic brush using extraction-equivalent planes/epsilon and reports only static subtractions that reduce that actual dynamic overlap.
- [x] Affected-map causal overlap evidence: source brush 79's entity-18 overlap falls `228 -> 196` at static brush 63, `196 -> 164` at brush 64, then `164 -> 16` at brush 66. Brush 66 is the primary loss and must be classified before any clipping-policy change.
- [x] Extended each bounded `DynamicOverlapBlocker` record with its shifted world bounds, maximum Z, and first brush texture name so compiled blocker geometry can be compared directly with floor 79 and door brush 81.
- [x] Geometry classification: brush 66 is a world-owned `orange/tex_12` solid (`x=-456..-440`, `y=-165..-128`, `z=0..128`); brushes 63/64 are world-owned `orange/tex_13` solids across `y=-128..-112`. They overlap entity 18's correctly sampled closed-pose footprint, leaving only the two `2 x 4` interiors. These are not origin helpers or misbound mover geometry.
- [x] Door spawn/timing audit: non-`START_OPEN` doors record the authored origin as `pos1`/`startOrigin`, relink at that closed origin, and command-triggered async nav generation samples the current linked state without pose adjustment. Entity 18 was generated at its correct closed pose.
- [x] Source-map comparison initially suggested a `180°` rotating-door transform, but affected-map runtime evidence invalidated it: entity 18 reports authoritative `currentAngles=(0,0,0)`, so the rotation-aware path is correctly inert. The unchanged outcome was instead explained by nav collecting BSP model 2 from the one-based handle, while collision correctly resolves entity 18 to BSP model 1.
- [x] Retained the general rotation-aware brush transform implementation because it is valid for non-zero runtime bmodel poses, but it is not the root cause for entity 18.
- [x] Audited model-handle conversion against `SV_HullForEntity`: nav now resolves `edict->s.modelindex - 1` before accessing `bsp->models`, so it collects the same inline geometry as runtime collision. No world-overlap suppression or map-content workaround was introduced.
- [x] Runtime regeneration verified the model-index correction: entity 18 now collects BSP model 1 / brush 80 and entity 19 collects BSP model 2 / brush 81. Each door emits two full exposed-side portals, removing the malformed opposite-door geometry.
- [x] Classified the remaining highlighted short/end lines: the door caps meet full-height world solids, so no walkable world polygon exists across those physical boundaries. They are not valid dynamic traversal portals and must remain ordinary boundaries.
- [x] Fixed initial runtime edge state: generation can run after a door has already closed, while close callbacks only update edges that exist at callback time. Newly registered dynamic/world portals now inherit their owner’s current closed-door or solid-wall state and begin with `NAV_EDGE_DISABLED` when blocking.
- [x] Tightened dynamic portal initialization to treat both fully closed and actively closing doors as blocked during registration, preventing a regenerated nav mesh from leaving a closing door briefly enabled.
- [ ] Rebuild and repeat the same runtime matrix before marking any acceptance item complete.

---

## Card 15 — Final Audit & Handoff
**Inputs**
- Build + regression outputs + invariant checklist

**Files Touched**
- Optional plan status notes

**Mutation Budget**
- ≤ 40 lines (status/report only)

**Checklist**
- [ ] Every invariant from top section explicitly signed off
- [ ] Each card has pass/fail evidence logged

**Pass/Fail Probe**
- Pass: full invariant compliance documented
- Fail: missing evidence for any invariant

**Rollback Condition**
- No handoff; return to first failing card

---

## Execution Ledger Template (Fill Per Card)

| Card | Start Time | End Time | Files Changed | Line Delta | Probe Result | Evidence Summary | Rollback Triggered | Notes |
|------|------------|----------|---------------|------------|--------------|------------------|--------------------|-------|
| 01 | | | | | Pass/Fail | | Yes/No | |
| 02 | | | | | Pass/Fail | | Yes/No | |
| 03 | | | | | Pass/Fail | | Yes/No | |
| 04 | | | | | Pass/Fail | | Yes/No | |
| 05 | | | | | Pass/Fail | | Yes/No | |
| 06 | | | | | Pass/Fail | | Yes/No | |
| 07 | | | | | Pass/Fail | | Yes/No | |
| 08 | | | | | Pass/Fail | | Yes/No | |
| 09 | | | | | Pass/Fail | | Yes/No | |
| 10 | | | | | Pass/Fail | | Yes/No | |
| 11 | | | | | Pass/Fail | | Yes/No | |
| 12 | | | | | Pass/Fail | | Yes/No | |
| 13 | | | | | Pass/Fail | | Yes/No | |
| 14 | | | | | Pass/Fail | | Yes/No | |
| 15 | | | | | Pass/Fail | | Yes/No | |

---

## Approval Gate
- [ ] Plan approved for execution
- [ ] Start at Card 01
- [ ] Enforce single-card progression (no skipping)
