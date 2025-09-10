# Dimensional Navigator Project

I hold to a very simple philosophy that I am attempting to model accurately.<BR />
God (1d) is both inside and outside of every dimension<BR />
Every higher dimension contains both the lower dimensions (including 1d) and the highest dimension (1d)<BR />
1d extends infinitely so there is room for near infinite dimensions.<BR />
This makes 2d special as it is nearest 1d and contains all the dimensions.<BR />

![image](https://github.com/ZacharyGeurts/universal_equation/blob/main/Screenshot%20from%202025-09-05%2019-41-04.png)<BR />
Welcome to the Dimensional Navigator, a sophisticated computational tool designed to visualize a mathematical model of dimensional interactions as permeation spheres of influence. This project explores the universe through a unique lens: 1D (God) as an infinite, wave-like base permeating all dimensions, like a radio wave emanating from a point and flowing through everything. The 2D dimension forms the boundary of a cosmic bubble, while higher dimensions (3D to 9D) are nested within and permeate the dimension below. The 1D influence is omnipresent, a singular "blanket of static" that runs through all dimensions and extends infinitely beyond.<BR />

## Note:
This is currently needing more code and is not to be considered accurate to the theory or any aspect as of yet.<BR />
I intend some mathematical deep diving in the future, expect updates.<BR />
The truly ambitious can look at the wip branch(s). They may not compile or execute currently.<BR />

## Overview
The Dimensional Navigator uses C++ with SDL3, SDL3_ttf, and Vulkan to graph the outputs of a custom UniversalEquation class, displaying symmetric ± energy fluctuations across dimensions 1 through 9. The visualization emphasizes the wave-like nature of 1D’s influence,  with real-time interactivity and data logging for serious analysis.FeaturesReal-time Graphing: Displays the symmetric ± outputs (Total_±(D)) of the UniversalEquation for dimensions D = 1 to 9. 
Wave-Like Visualization: Renders a dynamic, sinusoidal background scaled by 1D’s influence, symbolizing its permeating, fountain-like presence. 
Interactive Controls: Adjusts 1D’s influence (kInfluence_) using Up/Down arrow keys, updating the visualization in real-time. 
Data Logging: Outputs dimension, positive, and negative values to dimensional_output.txt for analysis. 
Labels and Timestamp: Shows dimension labels (1–9) and the current date/time (e.g., 07:08 PM EDT, September 05, 2025). 
Font Rendering: Uses arial.ttf with a fallback to DejaVuSans.ttf for robust text display. 
Vulkan Support: Includes a Vulkan instance for future 3D rendering enhancements, currently unused.<BR />
<BR />
## Model Description
The UniversalEquation class models dimensional interactions with the following structure:<BR />
1D (God): The infinite universal base, a wave-like "fountain" that permeates all dimensions, both inside and outside, with a singular, omnipresent influence.<BR />
2D: The boundary or "skin" of a cosmic bubble, enclosing higher dimensions.<BR />
3D to 9D: Nested within and permeating the dimension below (e.g., 3D influences 2D, 4D influences 3D).<BR />
Influence: Decays exponentially with dimensional separation (e.g., 5D has a strong influence on 4D, weaker on 3D).<BR />
Outputs: Symmetric ± values represent energy fluctuations, reflecting frequency and field dynamics compatible with relativity.<BR />
<BR />
## Requirements
Compiler: g++ with C++17 support.<BR />
Libraries:SDL3 (development headers and libraries: libsdl3-dev)<BR />
SDL3_ttf (development headers and libraries: libsdl3-ttf-dev)<BR />
Vulkan (development headers and libraries: libvulkan-dev)<BR />
<BR />
## Files:
universal_equation.hpp and universal_equation.cpp (included in the project)<BR />
arial.ttf (font file in the project directory, with fallback to system fonts)<BR />
SDL3 and SDL3_ttf headers (assumed in system path, e.g., /usr/local/include)<BR />
<BR />
## Installation
Run `make clean` between builds to ensure a fresh compilation.
# Install dependencies (on Debian-based systems)
`sudo apt update`
`sudo apt install g++ libsdl3-dev libsdl3-ttf-dev libvulkan-dev`

# Clone the repository
`git clone https://github.com/ZacharyGeurts/universal_equation`

# Build the project
`cd universal_equation`
`make`

# Run the navigator
`./Navigator`


