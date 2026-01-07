# water_test

This is essentially a place for me to test out implementing a few algorithms for simulating water surfaces for video games.

I think I've currently implemented a CPU-side version of the iWave algorithm presented in Jerry Tessendorf's [Interactive Water Surfaces](https://jtessen.people.clemson.edu/reports/papers_files/Interactive_Water_Surfaces.pdf) paper, and eventually I'm planning on doing the same for the eWave algorithm presented in Soumitra Goswami's thesis (INTERACTIVE WATER SURFACES USING GPU BASED eWAVE ALGORITHM IN A GAME PRODUCTION ENVIRONMENT)[https://jtessen.people.clemson.edu/students/goswami_thesis.pdf].

## Compilation
To compile, you need to have Visual Studio installed with the C++ workload abd install raylib to the `lib` subfolder (according to the README file in there). It should be possible to open the `.slnx` file as a solution and hit F5 to compile and run the program.

## Usage
While the program is running, you can left click/drag left click on the window to create sources, which will displace the surface. You can hit space to reset the simulation to the initial state.

## Implementation
Right now, I'm using (raylib)[https://github.com/raysan5/raylib] to create a window and drawing textures to the screen, though the actual program here is not doing much more than this on the GPU-side. The water surface stuff is being done CPU-side, and then being uploaded to the GPU as a texture (which is super inefficient because it spends time CPU to GPU upload time unnecessarily). In addition, many of the convolution optimization techniques mentioned in the iWave paper have not been currently implemented.

I'm thinking of moving the simulation to the GPU later through either fragment or compute shaders, so that it's more viable for in a game scenario.