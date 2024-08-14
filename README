![sheep](data/image/sheep.png "sheep")

# DopBVH
This is a reference implementation of [SAH-Optimized k-DOP Hierarchies for Ray Tracing](https://dl.acm.org/doi/10.1145/3675391) by M. Káčerik and J. Bittner.

## Build
### Requirements
* Linux with X11 or Windows
* C++ compiler (tested with gcc v14.1, clang v18.1, msvc v19.40)
* CMake (v3.28 or newer)
* Vulkan headers and driver (tested with v1.3.290)
* although written with Vulkan, current version runs only on NVIDIA GPUs (tested on rtx 3060, 3080Ti, 4080)

### CMake with default build tools
```
git clone --recurse-submodules https://github.com/kachnicka/dopbvh.git
cd dopbvh
cmake -B build
cmake --build build --config Release -j 16
cmake --install build
cd install
./dopbvh [or dopbvh.exe]
```

## Usage
### Rendering
Application has three rendering modes (can be cycled by [f] hotkey):
1. path tracing on top of VK_KHR_ray_tracing_pipeline and VK_KHR_acceleration_structure
2. path tracing on top of compute shaders, implementing BVH construction and traversal algorithms from the paper
3. rasterized debug view

### Configuration
Algorithm parameters are defined in [benchmark.toml](data/benchmark.toml). Available scenes are defined in [scene.toml](data/scene.toml). Application can serialize loaded scene to a custom binary format for fast load.

### Navigation and camera views
Blender style: middle-mouse-button for rotation, mouse-wheel for zoom, shift + middle-mouse-button for panning. Camera views can be saved and restored between the application runs. 

### Path Tracing compute
For each BVH, three additional outputs are available: path tracing, BVH levels explorer, and a heat map, indicating the number of necessary ray-BVH node intersections to trace one primary and one secondary ray.

#### BVH construction algorithms:
1. [AABB BVH](data/shaders/gen_plocpp_aabb_PLOCpp.comp) by PLOC++
2. [AABB BVH -> OBB BVH](data/shaders/gen_transform_aabb_obb.comp) by parallel DiTO
3. [AABB BVH -> 14-DOP BVH](data/shaders/gen_transform_aabb_dop14.comp) by parallel bottom-up refit
4. [14-DOP BVH](data/shaders/gen_plocpp_dop14_PLOCpp.comp) by PLOC++
5. [14-DOP BVH -> OBB BVH](data/shaders/gen_transform_dop14_obb.comp) by parallel DiTO

Host side entrypoint for BVH construction is [BVHBuildPiecewise()](src/dopbvh/backend/vulkan/Task.h#L357), 14-DOP surface area computations are implemented in [bv_dop14.glsl](data/shaders/bv_dop14.glsl).

#### BVH traversal algorithms:
1. [AABB BVH](data/shaders/gen_ptrace_aabb_sep.comp)
2. [OBB BVH](data/shaders/gen_ptrace_obb_sep.comp)
3. [14-DOP BVH](data/shaders/gen_ptrace_dop14_split_sep.comp)

### Acknowledgement
Hairball and Chestnut scenes distributed in the repository come from [McGuire Computer Graphics Archive](https://casual-effects.com/g3d/data10/index.html).
