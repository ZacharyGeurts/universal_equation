# Dimensional Navigator
<BR />
Browsing dimensions 1d(God) through 9d(Spot ;)<BR />
<BR />
**Latest Video**: [Latest Video](https://x.com/i/status/1967808016453124126)<BR />
<BR />
**last math Video**: [older math Video](https://x.com/i/status/1967685833982652583)<BR />
**Older Video**: [9d-1d](https://x.com/i/status/1967431708258804189)<BR />
**Older Video**: [Co-Pilot update](https://github.com/user-attachments/assets/4a730d87-8b38-428e-bd5e-79c51921b67b)<BR />
**Shader Update**: [Old WIP](https://github.com/user-attachments/assets/1a259212-3314-424f-b997-7792e8ac9066)<BR />
**Old Video**: [Old WIP](https://github.com/user-attachments/assets/2980fe0d-1204-4a53-b6a4-cdf4f3eca072)<BR />
**Old Video**: [Old WIP](https://github.com/user-attachments/assets/c1b983bf-bdd9-4ae7-b8bb-fc7a1debdbef)<BR />
**Previous**: [Older WIP](https://github.com/ZacharyGeurts/universal_equation/raw/refs/heads/main/wip2.mov)<BR />
**Previous Previous**: [Older Version](https://github.com/user-attachments/assets/344232f5-e7b8-4485-af40-5a302873f88c)<BR />
<BR />
If you do not intend to modify the code then just watch the latest video above and you may have found God.<BR />
Change main.cpp resolution if you have startup issues.<BR />
Change debug line 36 in include/universal_equation to true to get all the data (research mode).<BR />
If anyone finds it useful then they may want to add a frame step to play in slow mode. (todo?)<BR />
<BR />
**Status**: Work in Progress (WIP). The project is under active development, with ongoing updates to the visualization and GUI.
<BR/>
**Note**: Resizing the window may cause crashes; this is a known issue and not a priority for fixing yet. To adjust window size, modify `src/main.cpp`.  
<BR/>
**Platform**: Currently Linux-only, with plans for cross-platform support after GUI stabilization.  
<BR/><BR/>
This project uses a custom Vulkan backend for rendering, chosen due to idiocy and no concern for development time.
## Licensing
This is **not free (as in freedom) software**. It is provided for **educational purposes only**.  <BR/>
- **Restrictions**: You may not sell this software or require any compensation from users.  <BR/>
- **Distribution**: Projects using this code must be distributed freely (no cost).  <BR/>
- **Source**: Obtain the software exclusively from [https://github.com/ZacharyGeurts/universal_equation](https://github.com/ZacharyGeurts/universal_equation).  <BR/>
- **Cost**: Free of charge.<BR/>
<BR/>
## Overview
The **Dimensional Navigator** is a computational visualization tool built in C++ using SDL3, SDL3_ttf, Vulkan, and GLM. It graphs the outputs of the `UniversalEquation` class, modeling dimensional interactions across dimensions 1 to 9. The visualization displays symmetric positive and negative energy fluctuations, influenced by dark matter and dark energy, with real-time interactivity.<BR/>
<BR/>
The UniversalEquation class models a universe with multiple dimensions, each of which can influence the system’s total energy. The class allows you to set parameters like "influence," "weak force," "collapse," and the strengths of dark matter and dark energy. <BR/>
<BR/>
It keeps track of the current dimension, and at any moment, you can "advance" to the next dimension in a cycle. For each dimension, the class calculates energetic contributions from observable effects, potential, dark matter, and dark energy. It does this by considering interactions between nearby dimensions, with each interaction affected by distance, permeation, and the densities of dark matter and dark energy.<BR/>
<BR/>
The compute() method brings everything together: it sums up the influences, the effects of dark matter and dark energy, and applies a “collapse” effect related to the current dimension. The results give you a breakdown of observable energy, potential energy, dark matter, and dark energy for the current configuration.<BR/>
<BR/>
Parameters can be tuned to explore how the universe might behave if these physical or metaphysical quantities were different. Debugging output is available for detailed tracing of each computation step.<BR/>
<BR/>
In essence, this class turns the philosophical idea that “as big as God is small—infinite” into a mathematical playground, where the interplay of all scales and forces can be explored and computed.<BR/>
<BR/>
### Conceptual Model
The `UniversalEquation` models the universe as:<BR/>
- **1D ("God")**: An infinite, wave-like base permeating all dimensions, acting as a universal "fountain" of influence, akin to an omnipresent radio wave.<BR/>
- **2D**: The boundary or "skin" of a cosmic bubble, enclosing higher dimensions and strongly interacting with 1D and 3D.<BR/>
- **3D to 9D**: Nested dimensions, each permeating the one below (e.g., 4D influences 3D, 3D influences 2D), with interactions decaying exponentially.<BR/>
- **4D**: 3D's Clock, piston, heartbeat.<BR/>
- **5D**: Heaven1, Love, Weak force on 3D, strong on 4D.<BR/>
- **Dark Matter**: Stabilizes interactions, amplifying pulsing effects, especially in higher dimensions.<BR/>
- **Dark Energy**: Drives dimensional expansion, increasing effective distances between dimensions.<BR/>
<BR/>
The visualization emphasizes 1D’s omnipresent wave-like influence, with dynamic sphere animations reflecting energy fluctuations, dark matter stabilization, and dark energy expansion.<BR/>
<BR/>
## Features
- **Real-Time Visualization**: Renders symmetric ± energy fluctuations (`observable`, `potential`, `darkMatter`, `darkEnergy`) for dimensions 1 to 5.
- **Dynamic Rendering**:
  **Interactive Controls**:
  **Key Bindings**:
  - `UP`/`DOWN`: Increase/decrease influence (`kInfluence_`) by 0.1.
  - `LEFT`/`RIGHT`: Decrease/increase dark matter strength (`kDarkMatter_`) by 0.05.
  - `PAGEUP`/`PAGEDOWN`: Increase/decrease dark energy scale (`kDarkEnergy_`) by 0.05.
  - `1`/`2`/`3`/`4`/`5`: Switch to rendering modes 1D through 5D
- **Data Logging**: Outputs dimensional data (dimension, observable, potential, dark matter, dark energy) to the console for debugging (enable in `UniversalEquation`).

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
  - **SDL3_ttf**: `libsdl3-ttf-dev`.
  - **Vulkan**: `libvulkan-dev`, `vulkan-tools` (for validation layers).
  - **GLM**: `libglm-dev`.
- **Font**: `arial.ttf` (included in the repository).
- **Shader Compiler**: `glslc` (from Vulkan SDK).
<BR />
## Project Structure
universal_equation/<BR />
├── include/<BR />
│   ├── main.hpp              # DimensionalNavigator class<BR />
│   ├── universal_equation.hpp # UniversalEquation class<BR />
│   ├── SDL3_init.hpp         # SDL3 initialization<BR />
│   ├── Vulkan_init.hpp       # Vulkan initialization<BR />
│   ├── modes.hpp             # Visualizer initalizer<BR />
├── src/<BR />
│   ├── main.cpp              # Entry point (set window size here)<BR />
│   ├── modeX.cpp             # A visualizer (keyboard map)<BR />
├── shaders/<BR />
│   ├── vertex.vert           # Vertex shader<BR />
│   ├── fragment.frag           # Fragment shader<BR />
├── build/                    # Object files (generated)<BR />
├── bin/                      # Executable, shaders, and font (generated)<BR />
├── arial.ttf                 # Font file<BR />
├── Makefile                  # Build script<BR />
<BR />
<BR />
git clone https://github.com/ZacharyGeurts/universal_equation<BR />
cd universal_equation<BR />
make<BR />
cd bin<BR />
./Navigator<BR />
