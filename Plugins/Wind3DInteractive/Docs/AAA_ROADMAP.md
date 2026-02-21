# Wind3D Interactive: AAA Roadmap & Competitive Analysis

*Last Updated: 2026-02-22*

## Executive Summary
`Wind3DInteractive` aims to be the **best-in-class** open-source volumetric wind solution for Unreal Engine. By leveraging **Lattice Boltzmann Method (LBM)** and **Async Compute**, it will offer cinematic-grade physical accuracy with professional-grade optimization, surpasing the limitations of basic grid-based solvers.

---

## Factual Comparison Table

| Feature | Wind3D (Target AAA) | Unreal Built-in | FluidNinja | NVIDIA Flow |
|---------|-------------------|-----------------|------------|-------------|
| **Algorithm** | **LBM (D3Q19)** | Simple Sine Waves | Eulerian Grid | Sparse Grid (Eulerian) |
| **Volumetric** | Yes (3D) | No (2D Plane) | Yes (Pseudo-3D) | Yes (3D) |
| **Interactive** | **Full (LBM-based)** | Reactive | Full | Full |
| **Perf. Optimization**| **Async Compute** | N/A | GPU | GPU (Expensive) |
| **Scalability** | **Clipsmap + Curl Noise** | Global | Local | Local |
| **Material Integration**| **Volume Texture + MPC** | Material Function | Render Target | Custom Integration |
| **License** | Open Source | Proprietary | Proprietary | Proprietary |

---

## Core Algorithms & AAA Techniques

To reach the "Best-in-Class" status, we will integrate the following scientific techniques:

### 1. D3Q19 Lattice Boltzmann Method (LBM)
*   **The Algorithm**: We use the **D3Q19** discretization (19 velocity vectors in 3D space) for the Boltzmann Equation.
*   **BGK Collision Operator**: Implement a **Single-Relaxation-Time (SRT)** model with optimized relaxation parameters based on local Reynolds numbers.
*   **Why AAA?**: This allows the wind to flow naturally into "pockets" (like open windows or gaps in fences) where standard solvers would simply "stop".

### 2. Cascaded Voxel Grids (CVG)
*   **Technique**: Similar to Cascaded Shadow Maps.
    *   **Level 0 (Extreme Detail)**: High-resolution grid (1m cells) around the player.
    *   **Level 1 (Mid Range)**: Lower resolution (4m cells) covering the immediate landscape.
    *   **Level 2 (Horizon)**: Ultra-low resolution or procedural fallback.
*   **Benefit**: High-fidelity interaction at close range while maintaining awareness of distant storms or large-scale weather.

### 3. Spectral Turbulence Overlay
*   **Technique**: Layering a **High-Frequency Noise (Blue Noise/White Noise)** over the LBM grid.
*   **Simulation**: Since the LBM grid is finite, we use **Spectral Synthesis** in the foliage shader.
*   **Why AAA?**: This creates the "jittery" micro-movements of individual leaves that a 3D grid is too "smooth" to capture.

### 4. Pressure-Acoustic Coupling
*   **Innovation**: Sample the **Divergence and Pressure Gradient** from the LBM grid.
*   **Result**: Feed this data into a **Metasound** graph to dynamically generate "Wind Whistling" sounds that change based on building geometry and wind density.

---

## Optimization Roadmap

### Phase 1: Core Evolution (Week 1-2)
1.  **Transition to LBM**: Replace current pressure-projection solver with **D3Q19 LBM kernel**.
2.  **Async Compute Implementation**: Move solver dispatch to the Unreal Engine Async Compute queue to achieve "zero-cost" frame time.
3.  **16-bit Float Optimization**: Switch all internal textures to `PF_FloatRGBA` (half-precision) to save 50% VRAM.

### Phase 2: Interactivity & Voxelization (Week 2-3)
1.  **Dynamic Voxelizer**: Implement a GPU-based voxelization system for Skeletal Meshes (Characters) to allow wind to react to player movement.
2.  **SDF Integration**: Use Global Signed Distance Fields for static collision detection within the LBM grid.

### Phase 3: Visual Polish & Upsampling (Week 3-4)
1.  **Temporal Upsampling**: Run simulation at 30Hz with Compute-based interpolation for 60/120fps display.
2.  **Cinematic Material Functions**: Release "AAA Foliage Vertex Shader" library specifically optimized for the Wind3D volume texture.

---

## Improvement Metrics

| Metric | Current (Grid) | Target (AAA LBM) | Improvement |
|--------|----------------|------------------|-------------|
| **Frame Time (Local)** | 0.8ms (Sync) | ~0.2ms (Async) | **75% Reduction** |
| **Stability** | Occasional Divergence | Mathematically Stable | **Rock Solid** |
| **Interaction Quality** | Static Blockage | Realistic Wake/Vortex | **Cinematic Fidelity** |

---

> [!IMPORTANT]
> This roadmap focuses on **Production Readiness**. The goal is not just a "visual effect" but a robust simulation layer that can be shipped in commercial Unreal Engine titles.
