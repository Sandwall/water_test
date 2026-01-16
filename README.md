# water_test

This is essentially a place for me to test out implementing a few algorithms for simulating water surfaces for video games.

---
- I've attempted to implement the iWave algorithm presented in Jerry Tessendorf's [Interactive Water Surfaces](https://jtessen.people.clemson.edu/reports/papers_files/Interactive_Water_Surfaces.pdf) paper.
  - Currently both a CPU-based version (`iwave.cpp`) that renders to a float array, and a GPU-based version (`iwave_gpu.cpp`) that uses fragment shaders to render to F32 textures are implemented. They can be switched by uncommenting and commenting the corresponding headers in `main.cpp`. The CPU implementation is single-threaded and takes about 40 ms to render at a 160x90 resolution on an Intel i7-10700. The GPU implementation takes about 4-6 ms to render at a 1280x720 resolution on an NVIDIA RTX 2060 SUPER.
- Eventually I'll try to do the same for the eWave algorithm presented in Soumitra Goswami's thesis [INTERACTIVE WATER SURFACES USING GPU BASED eWAVE ALGORITHM IN A GAME PRODUCTION ENVIRONMENT](https://jtessen.people.clemson.edu/students/goswami_thesis.pdf).

## Compilation
To compile, you need to have Visual Studio installed with the C++ workload. Make sure to download the latest release of [GLFW](https://github.com/glfw/glfw/releases/), copy the included headers to the `include` folder (under a `GLFW` folder), as well as copy the required `*.dll` and `*.lib` files to the `lib` folder (under an `x64` folder, if you're compiling under `x64`). At that point, it should be possible to just open up the Visual Studio solution and run the program.

## Usage
While the program is running, you can left click/drag left click on the window to create sources, which will displace the surface, and you can do the same for right click to create obstructions. You can hit space to reset the simulation to the initial state.

## Screenshot
Here's a screenshot of the single-threaded CPU simulation running on a 160x90 grid:

![CPU Simulation on 160x90 grid](doc/cpu_160x90.png)