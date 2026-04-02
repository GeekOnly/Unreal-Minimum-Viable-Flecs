# Wind3DInteractive Foliage Physics Plan (GoW-Style Target)

Last updated: 2026-04-03

## Goal

Reach a God-of-War-style foliage response quality for:
- ambient wind motion
- gameplay impact response (hit, projectile, explosion, melee)
- stable performance with high foliage counts

## Current Baseline

Already implemented:
- volumetric wind grid simulation with motors
- mass-spring-damper foliage response
- foliage auto registration + presets (Grass, Shrub, Tree)
- per-instance and heatmap displacement debug
- radial gameplay impulse API: ApplyFoliageInteractionImpulse
- one-click setup actor interaction trigger
- one-click foliage spring material creation command:
  - Wind3D.CreateFoliageSpringMaterial /Game/Wind/M_WindFoliageSpring_Auto

## Main Gaps vs GoW-Style Quality

1. Scalar-only bend output
- Current output is mainly a scalar displacement value.
- Target is directional bend (at least 2D vector), then optional multi-layer bending.

2. Limited authored response profiles
- Current presets are broad categories.
- Target is species-level response profiles with curves.

3. Gameplay event wiring depth
- Current impulse API exists but does not yet auto-bridge from weapon systems.
- Target is direct hooks for hitscan, projectile, and explosions.

4. Multi-layer visual behavior
- Current material supports spring + turbulence blend.
- Target is primary/secondary sway layering with controlled trunk-to-tip distribution.

## Phase Plan

## Phase 1 - Interaction Foundation (Completed)

Scope:
- radial impulse API in WindSubsystem
- setup actor one-click interaction trigger
- docs update for setup and usage

Exit criteria:
- build passes with NoLiveCoding
- impulse can be triggered in editor and affects registered foliage

## Phase 2 - Directional Spring Response

Scope:
- upgrade receiver state from scalar bend to directional bend (2D or 3D)
- store directional output in CPD slots
- update material builder to consume directional bend

Implementation tasks:
1. Extend receiver component fields for vector displacement/velocity.
2. Update spring solver to integrate directional force.
3. Add directional CPD slot mapping (for example: X/Y bend + turbulence).
4. Update CreateFoliageSpringPhysicsMaterial graph for directional WPO.
5. Keep backward compatibility path for scalar mode.

Exit criteria:
- foliage reacts in the direction of impact and wind flow
- no regression in stability (no visible jitter spikes)
- scalar fallback still works for old assets

## Phase 3 - Gameplay Event Bridge

Scope:
- connect gameplay impacts directly to foliage impulse API
- provide unified bridge callable from combat systems

Implementation tasks:
1. Create a lightweight bridge function library or subsystem facade.
2. Add helper entry points:
   - ApplyFoliageImpulseFromHitResult
   - ApplyFoliageImpulseFromExplosion
3. Add filtering by actor tags/channels where needed.
4. Add debug traces/logs for event-to-impulse verification.

Exit criteria:
- projectile, hitscan, melee, and explosion can trigger foliage response without manual setup actor calls
- event response can be tuned from exposed parameters

## Phase 4 - Profile-Driven Authoring

Scope:
- move from fixed preset values to data-driven profiles
- support profile assignment per foliage type/component

Implementation tasks:
1. Create a profile asset type (mass, spring, damping, impulse multipliers, recovery).
2. Support profile lookup by mesh/component tag.
3. Add optional curve assets for attack/release shaping.
4. Keep existing Grass/Shrub/Tree presets as fallback defaults.

Exit criteria:
- at least 3 profiles authored and validated in a sample map
- designers can tune behavior without C++ edits

## Phase 5 - Visual Layering and Polish

Scope:
- improve shader-side motion layering and trunk-to-tip behavior

Implementation tasks:
1. Add trunk/branch/tip weighting controls.
2. Add secondary motion layer driven by turbulence/noise.
3. Add optional recovery overshoot control.
4. Add quick debug view modes for layer contribution.

Exit criteria:
- visual motion has clear trunk rigidity and tip flexibility
- impact response feels richer than single-layer bend

## Phase 6 - Performance and Ship Readiness

Scope:
- ensure scalability and frame stability on target maps

Implementation tasks:
1. Add budget cvars for max affected entities per frame.
2. Add distance-based throttling for interaction updates.
3. Add stress test map and benchmark checklist.
4. Add regression checks for CPD bounds safety.

Exit criteria:
- stable frame time under defined stress scenario
- no crashes/asserts from CPD writes during dynamic foliage changes

## Suggested Sprint Order (Next 2 Weeks)

Week 1:
1. Directional receiver data model
2. Directional solver integration
3. Directional CPD write path

Week 2:
1. Material directional WPO update
2. Compatibility path + migration notes
3. Validation pass on test map and docs

## Validation Checklist

- Build succeeds in Development Editor with NoLiveCoding.
- Auto-registration still works with existing maps.
- Interaction impulse affects only registered foliage in radius.
- Directional bending matches force direction.
- Debug overlays remain readable and performant.

## Risks and Mitigations

Risk: Added CPD slots break old materials.
- Mitigation: keep scalar compatibility mode and document slot mapping clearly.

Risk: High-frequency impulses cause unstable oscillation.
- Mitigation: clamp acceleration/velocity and add cooldown or per-frame budget.

Risk: Large foliage maps increase cost.
- Mitigation: distance culling, max-affected limits, and adaptive update budgets.

## Definition of Done (Plan Level)

- Phase 2 to Phase 4 implemented and documented.
- Usable sample demonstrating wind + combat foliage response.
- No critical stability regressions in current plugin workflow.
