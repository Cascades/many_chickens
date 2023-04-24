## Description
This repository is for me to try and improve my Vulkan development and graphics programming.

## Building
**Requires Vulkan SDK installed**

```
git submodule update --init --recursive
mkdir ../build_dir
cd ../build_dir
cmake ../many_chickens .
cmake --build .
cmake --install . --prefix=<your_install_dir>
<your_install_dir>/bin/app
```

## Notes
### Two-Pass GPU Occlusion Culling & Frustum Culling

This repoditory implements occlusion culling completely on the GPU using a two-pass method. The method is well documented by various blogs and playlists<sup>[1] [2] [3] [4]</sup>, but I'll run through my exact implementation here as I've tried to not look at too many references.

First, a diagram of the occlusion culling flow:

![Diagram showing the flow of this repo's two pass GPU occlusion culling pipeline](./media/occlusion_culling_pipeline_diagram.jpg)

* Early Pass
  * Adds every mesh which was drawn in the previous frame, as long as it remains potentially in the view frustum, to the indirect draw command buffer.
* Depth Pyramid Generation
  * Takes as input a Z-buffer (in this case, from the early pass), and produces a hierarchical Z-buffer (HiZ) as out.
  * Each mip level of the HiZ contains the previous level's information, but at half the size.
* Late Pass
  * Checks **_every_** mesh in the scene for visibility.
  * Visibility is checked against the frustum.
  * Visibility is checked by comparing the distance between the closest point on a given mesh and the camera to the HiZ generated from the early pass.
  * Marks **_every_** mesh as either potentially contributing to this frame's final pixels, or not.
  * **_Only_** draws meshes which are deemed to be potentially contributing to the frame's final pixels and haven't been drawn in the early pass already.
* The Draw Passes
  * The draw passes, in this repo's case, are a deffered rendering pipeline, with potentially some debugging output options.
  * `vkCmdDrawIndexedIndirectCount` is used to make the draw calls. The early and late passes write `VkDrawIndexedIndirectCommand` structures in to the indirect `commandBuffer`, as well as the `uint32_t` in to the `countBuffer`.
  * The G-buffers being used are:
    * Color (rgb: color, a: specularity)
    * Normal (rgb: normal)

#### Pipeline flow tl;dr
   * Frame 1:
     1. All potentially visible meshes are drawn in the late pass.
   * Frame 2:
     1. All potentially visible meshes from the last frame are drawn in the early pass.
     2. A useful HiZ is built.
     3. Any remaining meshes are drawn in the late pass.
   * Frame 3:
     1. Algorithm is now in full swing
     2. The early pass will draw contents of previous frame.
     3. The late pass will draw the contents which was missing from the early pass.

#### [Pipeline shown with no debugging](https://youtu.be/wqrAQBYW0mQ)
https://user-images.githubusercontent.com/5692370/234128074-9ed89579-1202-4845-a384-bdffe6f65b12.mp4

#### [Pipeline shown with debugging, showcasing occlusion culling](https://youtu.be/M1ZRBybtrEA)
https://user-images.githubusercontent.com/5692370/234128056-c3b241e1-3482-4abe-9519-1cb91aa5c4b9.mp4

#### [Pipeline shown with debugging, showcasing frustum culling](https://youtu.be/-1SxEx7pKro)
https://user-images.githubusercontent.com/5692370/234128094-99048d08-1e8c-48e9-bc75-0b303826b1c5.mp4

### Discrete LOD

This is a relatively simple algorithm. At the moment I use [meshoptimizer](https://github.com/zeux/meshoptimizer)'s `meshopt_simplify` to generate a single vertex buffer, and multiple index buffers for each of my LODs. Ina compute pass I then select which lod to draw based on the distance from the screen that the mesh exists at. 

This would be better improvved by basing my LOD selection on something like size of mesh triangle in screen space, but that's for the future!

[1]: https://medium.com/@mil_kru/two-pass-occlusion-culling-4100edcad501
[2]: https://interplayoflight.wordpress.com/2017/11/15/experiments-in-gpu-based-occlusion-culling/
[3]: https://blog.selfshadow.com/publications/practical-visibility/
[4]: https://www.youtube.com/playlist?list=PL0JVLUVCkk-l7CWCn3-cdftR0oajugYvd
