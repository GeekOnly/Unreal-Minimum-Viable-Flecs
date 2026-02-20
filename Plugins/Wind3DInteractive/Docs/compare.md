Wind Simulation Comparison: Unity (God of War) vs. Current Unreal Implementation
This document compares the technique described in 
UnityGoF.md
 (based on God of War 4's GDC talk) with your current Unreal Engine implementation.

1. Core Algorithm
Feature	Unity / God of War (Reference)	Current Unreal Implementation	Impact
Logic	Fluid Dynamics (Advection + Diffusion)	Accumulation & Decay	The Unity version supports "swirling" and wind flowing over time. The Unreal version is immediate: wind exists where motors are, then fades out.
Advection	Yes. Wind velocity "moves" itself.	No. Wind stays in the cell it was injected into until it decays.	Advection creates flow trails and complex interactions. Without it, wind feels static or "on/off".
Diffusion	Yes. Velocity spreads to neighbor cells (blur).	No.	Diffusion softens edges and helps wind propagate naturally.
Scroll	Yes. Grid moves with the player (Infinite scrolling).	No. Fixed World Origin.	The current Unreal grid has fixed bounds. If the player leaves the box, wind stops working.
2. Technical Architecture
Feature	Unity / God of War (Reference)	Current Unreal Implementation	Impact
Compute	GPU (Compute Shaders)	CPU (Serial C++)	GPU is vastly faster for grid operations (32x16x32 = 16k cells). CPU is easier to debug but limits grid resolution.
Storage	3D Textures (Volume Texture)	Flat Array (TArray<FVector>)	3D Textures are native to GPU sampling, making it easy for foliage shaders to read "global wind" anywhere.
Interaction	Global Shader Variables (Texture Bindings)	ECS / Manual Query	To affect foliage efficiently in Unreal, we typically need to write this data to a Volume Texture or use massive amounts of Custom Primitive Data.
3. Visual Results
Unity (GoF): Creates a "living" wind field. If you fire a vortex, it travels and swirls even after the motor moves. High-frequency turbulence is preserved.
Unreal (Current): Creates a "proximity" wind field. Wind is strong near the motor and weak far away. It doesn't "flow" or carry momentum.
Recommendations for Unreal
To match the God of War quality in Unreal Engine 5, we should eventually migrate from the simple CPU grid to a GPU-based approach:

Use 3D Render Targets: Store velocity in a UTextureRenderTargetVolume.
Compute Shaders: Port the HLSL code from 
UnityGoF.md
 (Advection/Diffusion kernels) to Unreal .usf Global Shaders.
Global Wind Sampling: Bind the Volume Texture to a global shader parameter so all foliage materials can sample Texture3DSample(WindField, WorldPos).
Current Next Steps (CPU Path)
If you want to stick with the CPU implementation for now (easier to code/debug):

Implement Scrolling: Update WindSubsystem::Tick to center the grid on the player.
Add Simple Diffusion: In 
WindGrid
, average each cell with its neighbors every tick.
Add "Pseudo-Advection": Shift the grid data in the direction of the wind (harder on CPU).