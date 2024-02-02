# Heightmap Visualizer:
## What?
- This C++ project can visualize `.png` heightmaps, in either a wireframe or a shaded view.
### Example outputs:
![1](/readme_media/heightmap-version-1.gif "Shaded View Turntable")

## Personal Motivation & Challenges
#### Motivation
- I took up this project because I wanted to work on a project that relates to computer graphics. Rendering terrain in the form of a heightmap was a challenging project idea, but it still seemed attainable with my C++ knowledge at the time. I also wanted to work on this project 'from scratch', solving challanges as they came my way, without following a tutorial or any other step-by-step guide to point me in the right direction. This turned out (in my opinion) to be a great idea, and I am now more confident in my problem-solving abilities as a result.

#### Challenges
1. Because `SDL2` can only render 2D graphics, I had to create my own data structures for 3D triangles and points.
2. I also had to solve the visible surface detection problem, which I did using Painter's Algorithm (z-buffer sorting). I chose this algorithm because it operates in a per-triangle manner, and `SDL2`'s interface for rendering triangles does not provide access at the fragment-level.

## How to compile:
- Necessary libraries: `SDL2` (I use `SDL2.28.5`), any other version of `SDL2` should work as well. These 

## Notes
Header libraries `lodepng.h` and `Eigen.h` are used.
