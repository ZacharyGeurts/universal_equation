# Dimensional Navigator

**Latest Video**: [Latest](https://github.com/user-attachments/assets/c1b983bf-bdd9-4ae7-b8bb-fc7a1debdbef)<BR />
**Previous**: [Older WIP](https://github.com/ZacharyGeurts/universal_equation/raw/refs/heads/main/wip2.mov)<BR />
**Previous Previous**: [Older Version](https://github.com/user-attachments/assets/344232f5-e7b8-4485-af40-5a302873f88c)<BR />
<BR />
If you do not intend to modify the code then just watch the latest video above and you may have found God.<BR />
<BR />
**Status**: Work in Progress (WIP). The project is under active development, with ongoing updates to the visualization and GUI.
**Note**: Resizing the window may cause crashes; this is a known issue and not a priority for fixing yet. To adjust window size, modify `src/main.cpp`.  
**Platform**: Currently Linux-only, with plans for cross-platform support after GUI stabilization.  

This project uses a custom Vulkan backend for rendering, chosen due to idiocy and no concern for development time.
## Licensing
This is **not free (as in freedom) software**. It is provided for **educational purposes only**.  
- **Restrictions**: You may not sell this software or require any compensation from users.  
- **Distribution**: Projects using this code must be distributed freely (no cost).  
- **Source**: Obtain the software exclusively from [https://github.com/ZacharyGeurts/universal_equation](https://github.com/ZacharyGeurts/universal_equation).  
- **Cost**: Free of charge.

## Overview
The **Dimensional Navigator** is a computational visualization tool built in C++ using SDL3, SDL3_ttf, Vulkan, and GLM. It graphs the outputs of the `UniversalEquation` class, modeling dimensional interactions across dimensions 1 to 9. The visualization displays symmetric positive and negative energy fluctuations, influenced by dark matter and dark energy, with real-time interactivity.

### Conceptual Model
The `UniversalEquation` models the universe as:
- **1D ("God")**: An infinite, wave-like base permeating all dimensions, acting as a universal "fountain" of influence, akin to an omnipresent radio wave.
- **2D**: The boundary or "skin" of a cosmic bubble, enclosing higher dimensions and strongly interacting with 1D and 3D.
- **3D to 9D**: Nested dimensions, each permeating the one below (e.g., 4D influences 3D, 3D influences 2D), with interactions decaying exponentially.
- **Dark Matter**: Stabilizes interactions, amplifying pulsing effects, especially in higher dimensions.
- **Dark Energy**: Drives dimensional expansion, increasing effective distances between dimensions.

The visualization emphasizes 1D’s omnipresent wave-like influence, with dynamic sphere animations reflecting energy fluctuations, dark matter stabilization, and dark energy expansion.

## Features
- **Real-Time Visualization**: Renders symmetric ± energy fluctuations (`observable`, `potential`, `darkMatter`, `darkEnergy`) for dimensions 1 to 4.
- **Dynamic Rendering**:
  - **Mode 1 (1D)**: Displays a blue quad with wave-like pulsing, representing the infinite base.
  - **Mode 2 (2D)**: Renders neon green spheres, emphasized for dimension 2, with pulsating effects driven by dark matter and dark energy.
  - **Mode 3 (3D)**: Shows red spheres with orbiting interactions, reflecting 3D’s influence on adjacent dimensions.
  - **Mode 4 (4D)**: Displays yellow spheres with complex scaling, modulated by observable/potential ratios.
- **Interactive Controls**:
  - Adjust influence, dark matter, and dark energy in real-time.
  - Zoom in/out using 'a'/'z' keys for enhanced exploration.
- **Key Bindings**:
  - `UP`/`DOWN`: Increase/decrease influence (`kInfluence_`) by 0.1.
  - `LEFT`/`RIGHT`: Decrease/increase dark matter strength (`kDarkMatter_`) by 0.05.
  - `PAGEUP`/`PAGEDOWN`: Increase/decrease dark energy scale (`kDarkEnergy_`) by 0.05.
  - `1`/`2`/`3`/`4`: Switch to rendering modes 1 (1D), 2 (2D), 3 (3D), or 4 (4D).
  - `a`/`z`: Zoom in/out.
- **Data Logging**: Outputs dimensional data (dimension, observable, potential, dark matter, dark energy) to the console for debugging (enable in `UniversalEquation`).
- **Zoom Support**: Adjusts visualization scale dynamically, enhancing user control over perspective.

## Model Description
The `UniversalEquation` class defines dimensional interactions:
- **1D ("God")**: A singular, infinite static permeating all dimensions, modeled as a wave-like influence.
- **2D**: The cosmic boundary, interacting strongly with 1D and 3D, visualized as pulsating neon green spheres.
- **3D to 9D**: Nested dimensions with exponentially decaying influence on lower dimensions, modulated by a weak interaction factor for D > 3.
- **Dark Matter**: Increases density with dimension, stabilizing interactions and amplifying visual pulsing (e.g., stronger in 3D/4D).
- **Dark Energy**: Drives exponential expansion of dimensional distances, affecting sphere positions and spacing.
- **Outputs**: Symmetric ± energy fluctuations (`observable`, `potential`) reflect frequency and field dynamics, with contributions from `darkMatter` and `darkEnergy`, compatible with relativistic principles.

## Requirements
- **OS**: Linux (Ubuntu/Debian recommended; other distros may require building SDL3 from source).
- **Compiler**: `g++` with C++17 support.
- **Libraries**:
  - **SDL3**: `libsdl3-dev` (may need to build from source if not available).
  - **SDL3_ttf**: `libsdl3-ttf-dev`.
  - **Vulkan**: `libvulkan-dev`, `vulkan-tools` (for validation layers).
  - **GLM**: `libglm-dev`.
- **Font**: `arial.ttf` (included in the repository).
- **Shader Compiler**: `glslc` (from Vulkan SDK).

## Project Structure
universal_equation/
├── include/
│   ├── main.hpp              # DimensionalNavigator class
│   ├── universal_equation.hpp # UniversalEquation class
│   ├── SDL3_init.hpp         # SDL3 initialization
│   ├── Vulkan_init.hpp       # Vulkan initialization
├── src/
│   ├── main.cpp              # Entry point (set window size here)
├── shaders/
│   ├── shader.vert           # Vertex shader
│   ├── shader.frag           # Fragment shader
├── build/                    # Object files (generated)
├── bin/                      # Executable, shaders, and font (generated)
├── arial.ttf                 # Font file
├── Makefile                  # Build script


## Installation
`git clone https://github.com/ZacharyGeurts/universal_equation`<BR />
`cd universal_equation`<BR />
`make`<BR />
`cd bin`<BR />
`./Navigator`<BR />
