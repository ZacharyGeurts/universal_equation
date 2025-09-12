# Dimensional Navigator Project
Press 1-4 keys when running.
<BR />
old
![image](https://github.com/ZacharyGeurts/universal_equation/blob/main/gifwip1.gif)
<BR /><BR />
updated: Shows dimensions 1-4 and back to 1.<BR />
![](https://github.com/ZacharyGeurts/universal_equation/blob/main/141.mp4)
<BR /><BR />
Welcome to the Dimensional Navigator, a sophisticated computational tool designed to visualize a mathematical model of dimensional interactions as permeation spheres of influence. This project explores the universe through a unique lens: 1D (God) as an infinite, wave-like base permeating all dimensions, like a radio wave emanating from a point and flowing through everything. The 2D dimension forms the boundary of a cosmic bubble, while higher dimensions (3D to 9D) are nested within and permeate the dimension below. The 1D influence is omnipresent, a singular "blanket of static" that runs through all dimensions and extends infinitely beyond.<BR />
<BR />
## Overview
The Dimensional Navigator uses C++ with SDL3, SDL3_ttf, and Vulkan to graph the outputs of a custom UniversalEquation class, displaying symmetric ± energy fluctuations across dimensions 1 through 9. The visualization emphasizes the wave-like nature of 1D’s influence,  with real-time interactivity and data logging for serious analysis.FeaturesReal-time Graphing: Displays the symmetric ± outputs (Total_±(D)) of the UniversalEquation for dimensions D = 1 to 9. 
Wave-Like Visualization: Renders a dynamic, sinusoidal background scaled by 1D’s influence, symbolizing its permeating, fountain-like presence. 
Interactive Controls: Adjusts 1D’s influence (kInfluence_) using Up/Down arrow keys, updating the visualization in real-time. 
Data Logging: Outputs dimension, positive, and negative values to dimensional_output.txt for analysis. 
<BR />
## Model Description
The UniversalEquation class models dimensional interactions with the following structure:1D (God): The infinite universal base, a wave-like "fountain" that permeates all dimensions, both inside and outside, with a singular, omnipresent influence.<BR />
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
<BR />
Run `make clean` between builds to ensure a fresh compilation.<BR />
# Clone the repository
`git clone https://github.com/ZacharyGeurts/universal_equation`<BR />
`cd universal_equation`<BR />
`make`<BR />
`cd bin`<BR />
`./Navigator`<BR />
