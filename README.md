# Mesh2Splat
<div align="center">
    <img src="./res/mesh2splatPipeline.jpg" width="750px">
</div>

**Mesh2Splat** is a fast surface splatting approach used to convert convert 3D meshes into 3DGS [(3D Gaussian Splatting)](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/) models by exploiting the rasterizer's interpolator the comes with a 3DGS renderer to view the results.<br>
What if we wanted to represent a synthetic object (3D model) in 3DGS format?<br>
Currently, the only way to do so is to generate a synthetic dataset (camera poses, image renders and initial sparse point cloud) of the 3D model, and then feed this into the 3DGS pipeline. This process can take several minutes, depending on the specific 3DGS pipeline and model used.<br>

**Mesh2Splat** instead, by directly using the geometry, materials and texture information from the 3D model, rather than going through the classical 3DGS pipeline, is able to obtain a 3DGS representation of the input 3D models in milliseconds.<br>

## Method
The (current) core concept behind **Mesh2Splat** is rather simple:
- Compute 3D model bounding box
- Initialize a 2D covariance matrix for our 2D Gaussians as: <br>
$`{\Sigma_{2D}} = \begin{bmatrix} \sigma^{2}_x & 0 \\\ 0 & \sigma^{2}_y \end{bmatrix}`$ <br><br> where: $`{\sigma_{x}}\sim {\sigma_{y}}\sim 0.65`$ <br>and $`{\rho} = 0`$
- Then, for each triangle primitive in the Geometry Shader stage, we do the following:-
    - Compute Jacobian matrix from *normalized UV space* to *3D space* for each triangle:  $`J = V \cdot (UV)^{-1} `$.
    - Apply triplanar orthogonal projection onto X,Y or Z face based on normal similarity and normalize in [-1, 1].
    - Derive the 3D directions corresponding to texture axes $`u`$ and $`v`$, and calculate the magnitudes of the 3D derivative vectors.
    - Multiply the found lengths for by the 2D Gaussian´s standard deviation and we found our scaling factors along the directions aligned with the surface in 3D space.
    - The packed scale values will be: 
        - $`packedScale_x = log(length(Ju) * sigma_x)`$
        - $`packedScale_y = log(length(Jv) * sigma_y)`$
        - $`packedScale_z = log(1e-7)`$
    

- Now that we have the **Scale** and **Rotation** for a *3D Gaussian* part of a specific triangle, emit one 3D Gaussian for each vertex of this triangle, setting their respective 3D position to the 3D position of the vertex, and in order to exploit while setting ```gl_Position = vec4(gs_in[i].normalizedUv * 2.0 - 1.0, 0.0, 1.0);```. This means that the rasterizer will interpolate these values and generate one 3D Gaussian per fragment in the orthogonal space.
- Do texture fetches and set this data per gaussian in Fragment Shader. 
- The `.ply` file format was modified in order to account for roughness and metallic properties

## Features
### Converter

- **Direct 3D Model Processing**: Directly obtain a 3DGS model from a 3D mesh.
- **Sampling density**: you can easily tweak the sampling density (conversion quality) in the settings via a slider.
- **Texture map support**: For now, Mesh2Splat supports the following texture maps:
    - Diffuse
    - MetallicRoughness
    - Normal
- **Enhanced Performance**: Significantly reduce the time needed to transform a 3D mesh into a 3DGS.
- **Relightability**: Can easily relight the gaussians given a renderer that supports it.
<div align="center">
    <img src="./res/conversion.gif" width="850px">
</div>

### 3DGS Renderer

- **Visualization options**: albedo, normals, depth, geometry, overdraw and pbr properties.
- **Gaussian shading**: supports PBR based shading.
- **Lighting and shadows**: simple point light and omnidirectional shadow mapping.
- **Shader hot-reload**: if you want to experiment with the shaders and 3DGS math, hot-reload is there to make your life easier.

<div align="center">
    <img src="./res/pbrShading.gif" width="850px">
</div>

## Performance
Mesh2Splat is able to convert a 3D mesh into a 3DGS on average in **<0.5ms**.
<br>

## Build Instructions (Windows)

To build **Mesh2Splat**, follow the following steps:

### Prerequisites
- **CMake** (>= 3.10)
- **Visual Studio 2019** (or a compatible C++17 compiler)
- **OpenGL-compatible GPU and drivers**

### Build Steps
1. Open a terminal (`cmd` or `PowerShell`) in the project root directory.
2. Run one of the provided batch scripts:
   - `run_build_debug.bat`
   - `run_build_release.bat`
<br>

   > **Tip**: Use the release build if you only need the final executable in optimized (Release) mode.


## Limitations
- Volumetric Data such as foliage, grass, hair, clouds, etc. has not being targeted and will probably not be converted correctly if using primitives different from triangles.<br>

# Authors

<div align="center">
<b>Search for Extraordinary Experiences Division (SEED) - Electronic Arts
<br>
<a href="https://seed.ea.com">seed.ea.com</a>
<br>
<a href="https://seed.ea.com"><img src="./res/seed-logo.png" width="150px"></a>
<br>
SEED is a pioneering group within Electronic Arts, combining creativity with applied research.</b> <br>
We explore, build, and help determine the future of interactive entertainment.
</p>

Mesh2splat is an Electronic Arts project created by [Stefano Scolari](https://www.linkedin.com/in/stefano-scolari/) for his Master's Thesis at [KTH](https://www.kth.se/en) while interning at [SEED](https://www.ea.com/seed) and supervision of Martin Mittring (Principal Rendering Engineer at [SEED](https://www.ea.com/seed)) and Christopher Peters (Professor in HCI & Computer Graphics at [KTH](https://www.kth.se/en)).

# Contributing

Before you can contribute, EA must have a Contributor License Agreement (CLA) on file that has been signed by each contributor. You can sign [here](https://electronicarts.na1.echosign.com/public/esignWidget?wid=CBFCIBAA3AAABLblqZhByHRvZqmltGtliuExmuV-WNzlaJGPhbSRg2ufuPsM3P0QmILZjLpkGslg24-UJtek*).

# License

The source code is released under an open license as detailed in [LICENSE.txt](./LICENSE.txt)







