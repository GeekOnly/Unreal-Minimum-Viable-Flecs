# Wind3DInteractive Setup Guide

This README focuses on practical setup for the current Wind3DInteractive plugin state, including:
- physics-based foliage spring response
- automatic foliage registration
- foliage presets (Grass, Shrub, Tree)
- displacement debug overlays (instance and heatmap modes)

## 1) Requirements

- Unreal Engine 5.7
- Project builds with Visual Studio 2022 toolchain
- Wind3DInteractive plugin enabled

Build command used in this workspace:

```powershell
& "J:\GAMES\UE_5.7\Engine\Build\BatchFiles\Build.bat" FlecsTestEditor Win64 Development "-Project=J:\Project_2026\February\Unreal\Unreal-Minimum-Viable-Flecs\FlecsTest.uproject" -WaitMutex -FromMsBuild -NoLiveCoding
```

Use `-NoLiveCoding` when editor live coding is active.

## 2) Quick Scene Setup

1. Place `Wind Field Setup` actor in the level.
2. Configure wind grid:
	- `GridSizeX`, `GridSizeY`, `GridSizeZ`
	- `CellSize`
3. Enable world wind if needed:
	- `bEnableWorldWind = true`
	- tune `WorldWindSpeed`, rotation/noise values
4. Enable foliage auto registration:
	- `bAutoRegisterFoliage = true`
	- optional filters: source actors, tag filter, radius, max instance cap
5. Choose foliage preset in setup actor:
	- `FoliagePhysicsPreset = Grass | Shrub | Tree | Custom`
	- optional: click `ApplyFoliagePresetToDefaults` to copy preset values to editable defaults
6. Place one or more `Wind Motor Actor` in the level and tune shape/emission/strength.

## 3) Foliage Material Contract

The runtime writes to HISM custom primitive data slots:

- Displacement slot: default `0`
- Turbulence slot: default `1`

If a different slot mapping is needed, set:
- `FoliageCPDSlotDisplace`
- `FoliageCPDSlotTurbulence`

## 4) Foliage Presets

Preset behavior is applied during auto registration.

- Grass: light, fast, higher responsiveness
- Shrub: balanced default
- Tree: heavier, slower movement, stronger damping
- Custom: uses values currently set in `Foliage|Physics Defaults`

## 5) Runtime Debug - Wind Grid

```text
Wind3D.ShowDebug 1
Wind3D.ShowTrailDebug 1
Wind3D.ShowObstacleNormals 1
```

## 6) Runtime Debug - Foliage Displacement

Enable foliage debug:

```text
Wind3D.ShowFoliageDisplacementDebug 1
Wind3D.ShowFoliageDisplacementText 0
```

Render mode:

```text
Wind3D.FoliageDisplacementDebugRenderMode 0   ; instance lines
Wind3D.FoliageDisplacementDebugRenderMode 1   ; heatmap grid clusters
Wind3D.FoliageDisplacementDebugRenderMode 2   ; both
```

Color mode:

```text
Wind3D.FoliageDisplacementDebugColorMode 0    ; smooth gradient
Wind3D.FoliageDisplacementDebugColorMode 1    ; high-contrast bands
Wind3D.FoliageDisplacementDebugColorMode 2    ; binary
```

Density and camera filters:

```text
Wind3D.FoliageDisplacementDebugMaxInstances 400
Wind3D.FoliageDisplacementDebugCameraRadius 5000
```

Heatmap cluster controls:

```text
Wind3D.FoliageDisplacementHeatmapCellSize 220
Wind3D.FoliageDisplacementHeatmapMinSamples 2
Wind3D.FoliageDisplacementHeatmapMaxClusters 1000
Wind3D.FoliageDisplacementHeatmapPointSizeMin 8
Wind3D.FoliageDisplacementHeatmapPointSizeMax 28
Wind3D.FoliageDisplacementHeatmapHeight 35
```

Auto-adaptive heatmap cell size by camera distance:

```text
Wind3D.FoliageDisplacementHeatmapAdaptiveCellSize 1
Wind3D.FoliageDisplacementHeatmapAdaptiveLevels 5
Wind3D.FoliageDisplacementHeatmapAdaptiveMaxScale 4
```

Distance optimization:

```text
Wind3D.FoliageDisplacementDistanceOptimize 1
Wind3D.FoliageDisplacementDistanceNear 1200
Wind3D.FoliageDisplacementDistanceFar 8000
```

## 7) Recommended Debug Presets

Close-range tuning:

```text
Wind3D.FoliageDisplacementDebugRenderMode 0
Wind3D.FoliageDisplacementDebugColorMode 1
Wind3D.FoliageDisplacementDebugCameraRadius 2500
Wind3D.ShowFoliageDisplacementText 1
```

Far/overview tuning (drone camera):

```text
Wind3D.FoliageDisplacementDebugRenderMode 1
Wind3D.FoliageDisplacementHeatmapAdaptiveCellSize 1
Wind3D.FoliageDisplacementHeatmapAdaptiveLevels 6
Wind3D.FoliageDisplacementHeatmapAdaptiveMaxScale 5
Wind3D.FoliageDisplacementDistanceNear 1500
Wind3D.FoliageDisplacementDistanceFar 12000
Wind3D.FoliageDisplacementDebugCameraRadius 9000
```

## 8) Stability Notes

The plugin now guards custom data writes to avoid HISM array crashes:

- validates instance index before write
- ensures enough custom data floats are allocated
- uses safe registration checks for invalid foliage entries

If foliage is rebuilt dynamically while playing, invalid entries are skipped safely.

## 9) Troubleshooting

If no foliage movement appears:

1. Confirm foliage material reads custom primitive data slot 0/1 (or your configured slots).
2. Confirm `bAutoRegisterFoliage` is enabled on `Wind Field Setup`.
3. Check setup actor log output for registered count.
4. Verify at least one active `Wind Motor Actor` is in range.

If debug is too noisy:

1. Reduce `Wind3D.FoliageDisplacementDebugMaxInstances`.
2. Set `Wind3D.FoliageDisplacementDebugRenderMode 1` (heatmap only).
3. Use `Wind3D.FoliageDisplacementDebugCameraRadius` and distance optimization.
