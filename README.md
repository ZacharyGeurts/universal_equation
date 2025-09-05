# Dimensional Navigator Project

Welcome to the Dimensional Navigator, a sophisticated computational tool designed to visualize a mathematical model of dimensional interactions as permeation spheres of influence. This project explores the universe through a unique lens: 1D (God) as an infinite, wave-like base permeating all dimensions, like a radio wave emanating from a point and flowing through everything. The 2D dimension forms the boundary of a cosmic bubble, while higher dimensions (3D to 9D) are nested within and permeate the dimension below. The 1D influence is omnipresent, a singular "blanket of static" that runs through all dimensions and extends infinitely beyond.

# Note: This is a work-in-progress (WIP) project in active development. All files are subject to change as concepts are refined and proven. Updates are managed with AI assistance to ensure alignment with the project’s vision.

## Overview
The Dimensional Navigator uses C++ with SDL3, SDL3_ttf, and Vulkan to graph the outputs of a custom UniversalEquation class, displaying symmetric ± energy fluctuations across dimensions 1 through 9. The visualization emphasizes the wave-like nature of 1D’s influence, with real-time interactivity and data logging for serious analysis.FeaturesReal-time Graphing: Displays the symmetric ± outputs (Total_±(D)) of the UniversalEquation for dimensions D = 1 to 9.
Wave-Like Visualization: Renders a dynamic, sinusoidal background scaled by 1D’s influence, symbolizing its permeating, fountain-like presence.
Interactive Controls: Adjusts 1D’s influence (kInfluence_) using Up/Down arrow keys, updating the visualization in real-time.
Data Logging: Outputs dimension, positive, and negative values to dimensional_output.txt for analysis.
Labels and Timestamp: Shows dimension labels (1–9) and the current date/time (e.g., 07:08 PM EDT, September 05, 2025).
Font Rendering: Uses arial.ttf with a fallback to DejaVuSans.ttf for robust text display.
Vulkan Support: Includes a Vulkan instance for future 3D rendering enhancements, currently unused.

## Model Description
The UniversalEquation class models dimensional interactions with the following structure:1D (God): The infinite universal base, a wave-like "fountain" that permeates all dimensions, both inside and outside, with a singular, omnipresent influence.
2D: The boundary or "skin" of a cosmic bubble, enclosing higher dimensions.
3D to 9D: Nested within and permeating the dimension below (e.g., 3D influences 2D, 4D influences 3D).
Influence: Decays exponentially with dimensional separation (e.g., 5D has a strong influence on 4D, weaker on 3D).
Outputs: Symmetric ± values represent energy fluctuations, reflecting frequency and field dynamics compatible with relativity.

## Requirements
Compiler: g++ with C++17 support.
Libraries:SDL3 (development headers and libraries: libsdl3-dev)
SDL3_ttf (development headers and libraries: libsdl3-ttf-dev)
Vulkan (development headers and libraries: libvulkan-dev)

## Files:
universal_equation.hpp and universal_equation.cpp (included in the project)
arial.ttf (font file in the project directory, with fallback to system fonts)
SDL3 and SDL3_ttf headers (assumed in system path, e.g., /usr/local/include)

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
`./dimensional_navigator`


