# Dimensional Navigator Project
<BR />
WIP - Current designing and updating the program.<BR />
Do not resize or it will crash. Not critical for me to fix yet<BR />
Like an idiot I made a custom Vulkan backend instead of flat SDL3, so it has become more complicated than necessary.<BR />
I enjoy my idiocy however 😊, or I would be on SDL2 and OpenGL<BR />
<BR />
This is not free (like freedom) software.<BR />
This software is for educational purposes.<BR />
You may not sell, or require any compensation by the user.<BR />
Attainment is to be done at https://github.com/ZacharyGeurts/universal_equation<BR />
Projects using any of this code are subject to free distribution.<BR />
<BR />
**Key Bindings**:<BR />
`UP`: Increase influence (`kInfluence_`) by 0.1<BR />
`DOWN`: Decrease influence (`kInfluence_`) by 0.1<BR />
`LEFT`: Decrease dark matter strength (`kDarkMatter_`) by 0.05<BR />
`RIGHT`: Increase dark matter strength (`kDarkMatter_`) by 0.05<BR />
`PAGEUP`: Increase dark energy scale (`kDarkEnergy_`) by 0.05<BR />
`PAGEDOWN`: Decrease dark energy scale (`kDarkEnergy_`) by 0.05<BR />
`1`: Mode 1 Dimension 1? God?<BR />
`2`: Mode 2 Dimension 2<BR />
`3`: Mode 3 etc.<BR />
`4`: Mode 4 etc.<BR />
<BR />
**Video Clip**:<BR />
https://github.com/user-attachments/assets/344232f5-e7b8-4485-af40-5a302873f88c<BR />
<BR />
## Overview
The **Dimensional Navigator** is a computational tool that visualizes a mathematical model of dimensional interactions using C++ with SDL3, SDL3_ttf, and Vulkan.<BR />
It graphs the outputs of the `UniversalEquation` class, displaying symmetric ± energy fluctuations across dimensions 1 through 9, with dark matter and dark energy influencing the dynamics.<BR />
<BR />
The model conceptualizes the universe as:<BR />
- **1D (God)**: An infinite, wave-like base permeating all dimensions, acting as a "fountain" of influence, like a radio wave flowing through everything.<BR />
- **2D**: The boundary or "skin" of a cosmic bubble, enclosing higher dimensions.<BR />
- **3D to 9D**: Nested dimensions that permeate the dimension below, with interactions decaying exponentially.<BR />
- **Dark Matter**: Stabilizes interactions, amplifying pulsing effects in higher dimensions.<BR />
- **Dark Energy**: Drives expansion, increasing effective distances between dimensions.<BR />
<BR />
The visualization emphasizes the wave-like nature of 1D’s omnipresent influence, with real-time interactivity and dynamic rendering of cosmological effects.

## Features
- **Real-time Graphing**: Displays symmetric ± energy fluctuations (`Total_±(D)`) for dimensions D = 1 to 9.
- **Wave-Like Visualization**: Renders a dynamic, sinusoidal background scaled by 1D’s influence, symbolizing its permeating presence.
- **Dark Matter Visualization**: Amplifies pulsing effects in sphere animations, reflecting its stabilizing role (e.g., stronger pulses in higher dimensions).
- **Dark Energy Visualization**: Adjusts sphere spacing or positions, representing dimensional expansion (e.g., wider orbits in Mode 2).
- **Interactive Controls**: Adjust influence (`UP`/`DOWN`), dark matter (`LEFT`/`RIGHT`), and dark energy (`PAGEUP`/`PAGEDOWN`) in real-time.
- **Rendering Modes**: Four distinct modes to visualize dimensional interactions, with unique arrangements and effects.
- **Data Logging**: Outputs dimension, positive, negative, dark matter, and dark energy values to `dimensional_output.txt` for analysis.

## Model Description
The `UniversalEquation` class models dimensional interactions with the following structure:
- **1D (God)**: The infinite universal base, a singular "blanket of static" that permeates all dimensions, both inside and outside, with a wave-like influence.
- **2D**: The boundary of a cosmic bubble, enclosing higher dimensions and interacting strongly with 1D and 3D.
- **3D to 9D**: Nested dimensions, each permeating the dimension below (e.g., 4D influences 3D, 3D influences 2D).
- **Influence**: Decays exponentially with dimensional separation, modulated by a weak interaction factor for dimensions above 3.
- **Dark Matter**: Acts as a stabilizing force, increasing in density with dimension number, reducing fluctuations and enhancing visual pulsing.
- **Dark Energy**: Drives exponential expansion of dimensional distances, affecting sphere positions and spacing in visualizations.
- **Outputs**: Symmetric ± energy fluctuations reflect frequency and field dynamics, compatible with relativity, with contributions from dark matter and dark energy.

## Requirements
- **Compiler**: g++ with C++17 support.
- **Libraries**:
  - **SDL3**: Development headers and libraries (`libsdl3-dev`).
  - **SDL3_ttf**: Development headers and libraries (`libsdl3-ttf-dev`).
  - **Vulkan**: Development headers and libraries (`libvulkan-dev`, `vulkan-tools` for validation).
  - **GLM**: OpenGL Mathematics library (`libglm-dev`).
- **Font**: `arial.ttf` (included).

## Files
- `universal_equation.hpp`: Header for the `UniversalEquation` class, defining dimensional interactions.
- `main.hpp`: Header for the `DimensionalNavigator` class, handling visualization and input.
- `SDL3_init.hpp` and `Vulkan_init.hpp`: Utility headers for SDL3 and Vulkan initialization.
- `assets/arial.ttf`: Font file for text rendering.
- `assets/demo.mp4`: Video demo of the project.
- SDL3, SDL3_ttf, Vulkan, and GLM headers are assumed to be in the system path (e.g., `/usr/include` or `/usr/local/include`).

## Installation
Note: SDL3 is not available on all distros yet and you may need to build and install from source.
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt update
sudo apt install g++ libsdl3-dev libsdl3-ttf-dev libvulkan-dev vulkan-tools libglm-dev

# Clone the repository
git clone https://github.com/ZacharyGeurts/universal_equation
cd universal_equation

# Build the project
make

# Run the application
cd bin
./Navigator
