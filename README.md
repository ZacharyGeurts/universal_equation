# Dimensional Navigator
Note: vulkan, glm, and SDL3 headers included for easier building.

Browsing dimensions 1D (God) through 9D (Spot ;) )

**Latest Video**: [Latest Video](https://x.com/i/status/1967808016453124126)

**Last Math Video**: [Older Math Video](https://x.com/i/status/1967685833982652583)  
**Older Video**: [9D-1D](https://x.com/i/status/1967431708258804189)  
**Older Video**: [Co-Pilot Update](https://github.com/user-attachments/assets/4a730d87-8b38-428e-bd5e-79c51921b67b)  
**Shader Update**: [Old WIP](https://github.com/user-attachments/assets/1a259212-3314-424f-b997-7792e8ac9066)  
**Old Video**: [Old WIP](https://github.com/user-attachments/assets/2980fe0d-1204-4a53-b6a4-cdf4f3eca072)  
**Old Video**: [Old WIP](https://github.com/user-attachments/assets/c1b983bf-bdd9-4ae7-b8bb-fc7a1debdbef)  
**Previous**: [Older WIP](https://github.com/ZacharyGeurts/universal_equation/raw/refs/heads/main/wip2.mov)  
**Previous Previous**: [Older Version](https://github.com/user-attachments/assets/344232f5-e7b8-4485-af40-5a302873f88c)

If you do not intend to modify the code, just watch the latest video above and you may have found God.  
Change `main.cpp` resolution if you have startup issues.  

**Platform**: Currently Linux-only  

This project uses a custom Vulkan backend for rendering, chosen from idiocy, ignorance, and patience.  

## Licensing
This is **not free (as in freedom) software**. It is provided for **educational purposes only**.  
- **Restrictions**: You may not sell this software or require any compensation from users.  
- **Distribution**: Projects using this code must be distributed freely (no cost).  
- **Source**: Obtain the software exclusively from [https://github.com/ZacharyGeurts/universal_equation](https://github.com/ZacharyGeurts/universal_equation).  
- **Cost**: Free of charge.

## Overview
The **Dimensional Navigator** is a computational visualization tool built in C++ using SDL3, SDL3_ttf, Vulkan, and GLM. It graphs the outputs of the `UniversalEquation` class, modeling dimensional interactions across dimensions 1 to 9. The visualization displays symmetric positive and negative energy fluctuations, influenced by dark matter and dark energy, with real-time interactivity.

The `UniversalEquation` class models a universe with multiple dimensions, each influencing the system’s total energy. Parameters like "influence," "weak force," "collapse," and the strengths of dark matter and dark energy can be set. The class tracks the current dimension and allows cycling through dimensions. For each dimension, it calculates energetic contributions from observable effects, potential, dark matter, and dark energy, considering interactions between nearby dimensions, affected by distance, permeation, and densities of dark matter and dark energy.

The `compute()` method aggregates influences, dark matter, dark energy, and a “collapse” effect tied to the current dimension, providing a breakdown of observable energy, potential energy, dark matter, and dark energy. Parameters can be tuned to explore alternative physical or metaphysical scenarios. Debugging output is available for detailed computation tracing.

This class transforms the philosophical idea that “as big as God is small—infinite” into a mathematical playground for exploring the interplay of scales and forces.

### Conceptual Model
The `UniversalEquation` models the universe as:  
- **1D ("God")**: An infinite, wave-like base permeating all dimensions, acting as a universal "fountain" of influence, akin to an omnipresent radio wave.  
- **2D**: The boundary or "skin" of a cosmic bubble, enclosing higher dimensions and strongly interacting with 1D and 3D.  
- **3D to 9D**: Nested dimensions, each permeating the one below (e.g., 4D influences 3D, 3D influences 2D), with interactions decaying exponentially.  
- **4D**: Time. 3D's clock, piston, heartbeat. 3D writes to it.  
- **5D**: Heaven1, Love, weak force on 3D, strong on 4D.  
- **Dark Matter**: Stabilizes interactions, amplifying pulsing effects, especially in higher dimensions.  
- **Dark Energy**: Drives dimensional expansion, increasing effective distances between dimensions.

The visualization emphasizes 1D’s omnipresent wave-like influence, with dynamic sphere animations reflecting energy fluctuations, dark matter stabilization, and dark energy expansion.

## Features
- **Real-Time Visualization**: Renders symmetric ± energy fluctuations (`observable`, `potential`, `darkMatter`, `darkEnergy`) for dimensions 1 to 9.  
- **Dynamic Rendering**: Modular rendering modes (`renderMode1` to `renderMode9`) for each dimension, implemented in `modes.hpp`.  
- **Interactive Controls**:  
  - **Key Bindings**:  
    - `UP`/`DOWN`: Increase/decrease influence (`kInfluence_`) by 0.1.  
    - `LEFT`/`RIGHT`: Decrease/increase dark matter strength (`kDarkMatter_`) by 0.05.  
    - `PAGEUP`/`PAGEDOWN`: Increase/decrease dark energy scale (`kDarkEnergy_`) by 0.05.  
    - `1` to `9`: Switch to rendering modes 1D through 9D.
    - `F` toggle Fullscreen
    - `A`/`Z`: Zoom in/out (adjusts `zoomLevel_` between 0.01 and 20.0).  
- **Data Logging**: Outputs dimensional data (dimension, observable, potential, dark matter, dark energy) to the console for debugging.

## Model Description
The `UniversalEquation` class defines dimensional interactions:  
- **1D (God)**: A singular, infinite static permeating all dimensions, modeled as a wave-like influence.  
- **2D (Truth)**: The cosmic boundary, interacting strongly with 1D and 3D, visualized as pulsating neon green spheres.  
- **3D to 9D**: Nested dimensions with exponentially decaying influence on lower dimensions, modulated by a weak interaction factor for D > 3.  
- **Dark Matter**: Increases density with dimension, stabilizing interactions and amplifying visual pulsing (e.g., stronger in 3D/4D).  
- **Dark Energy**: Drives exponential expansion of dimensional distances, affecting sphere positions and spacing.  
- **Outputs**: Symmetric ± energy fluctuations (`observable`, `potential`) reflect frequency and field dynamics, with contributions from `darkMatter` and `darkEnergy`, compatible with relativistic principles.

## Requirements
Resolution 1920x1080 or higher.  
- **OS**: Linux (Ubuntu/Debian recommended; other distros may require building SDL3 from source).  
- **Compiler**: `g++` with C++17 support.  
- **Libraries**:  
  - **SDL3**: `libsdl3-dev` (may need to build from source if not available).  
  - **Vulkan**: `libvulkan-dev`, `vulkan-tools` (for validation layers).  
  - **GLM**: `libglm-dev`.  
- **Shader Compiler**: `glslc` (from Vulkan SDK).

I do not condone violence, incorrectness, or the new Oxford comma.

## Build Instructions
```bash
git clone https://github.com/ZacharyGeurts/universal_equation
cd universal_equation
make
