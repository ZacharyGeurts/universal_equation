# Dimension Dance Project

Welcome to the **Dimension Dance Project**! This is a fun and educational tool that visualizes a mathematical model of dimensions as **permeation spheres of influence**. Let's explore the universe with 1D as an infinite base, 2D as the edge of a bubble, and higher dimensions (3D to 9D) nested within each other! Everywhere you look there is 1d (God), but there is only One 1d. The blanket of static that runs through all dimensions and infinitly beyond.

wip. I am less concerned about functional code and more toward the hpp file being accurate. eg. expect to modify to get working.

## Overview

- **Purpose**: This project uses C++ with **SDL3**, **SDL3_ttf**, and **Vulkan** to graph the outputs of a custom `UniversalEquation` class. It displays the symmetric ± outputs (like energy waves) for dimensions 1 through 9.
- **Features**:
  - Real-time graph of \(\text{Total}_{\pm}(D)\) for \(D = 1\) to \(9\).
  - Labels for each dimension (1-9) and the current date/time (e.g., **07:41 PM EDT, September 04, 2025**).
  - Uses `arial.ttf` for text rendering.
  - Includes a Vulkan instance (not yet used for rendering, future enhancement).

## Model Description

- **1D**: God. The infinite universal base, like a fountain, inside and outside of everything.
- **2D**: The edge or skin of the bubble, defining its boundary.
- **3D to 9D**: Nested within and permeating the dimension below (e.g., 3D throughout 2D, 4D throughout 3D).
- **Influence**: Decays exponentially with dimensional separation (e.g., 5D strong on 4D, weak on 3D).
- **Outputs**: Symmetric ± values reflecting frequency and fields, compatible with relativity.

## Requirements

- **Compiler**: g++ (with C++17 support)
- **Libraries**:
  - SDL3 (development headers and libraries)
  - SDL3_ttf (development headers and libraries)
  - Vulkan (development headers and libraries, e.g., `libvulkan-dev`)
- **Files**:
  - `universal_equation.hpp` (included in the project)
  - `arial.ttf` (font file in the project directory)
  - `SDL3/` and `SDL3_ttf/` (assumed in system path or project directory)

## Installation
Run `make clean` between builds

**Install Dependencies** (on Debian-based systems):
   ```bash
   sudo apt update
   sudo apt install g++ libsdl3-dev libsdl3-ttf-dev libvulkan-dev

   git clone https://github.com/ZacharyGeurts/universal_equation

   cd universal_equation
   make
