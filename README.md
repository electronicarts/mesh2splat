# Mesh2Splat

## Introduction
Welcome to **Mesh2Splat**, a novel approach to convert 3D meshes into 3DGS (3D Gaussian Splatting) models.<br>
3DGS was born to recontruct a photorealistic 3D representation from a series of images. This is possible thanks to an optimization process which relies on a series of reference images which are compared to 3DGS rendered scene obtained via a differentiable renderer that makes it possible to optimize a set of initial scene parameters [1].
<div align="center"> 
    <img src="res/seminalTechnique.png" alt="Seminal technique 3dGS" style="width: 80%;">
</div><br>
What if we want to represent a synthetic object (3D model) in 3DGS format rather than a real scene? Currently, the only way to do so is to generate a synthetic dataset (camera poses, renders and sparse point cloud) of the 3D model, and feed this into the 3DGS pipeline. This process can take up to 30min-45min.<br>

**Mesh2Splat** instead, by directly using the geometry, materials and texture information from the 3D model, rather than going through the classical 3DGS pipeline, is able to obtain a 3DGS representation of the input 3D models in seconds.<br>
This methodology sidesteps the need for greater interoperability between classical 3D mesh representation and the novel 3DGS format.<br>


## Concept
The (current) core concept behind **Mesh2Splat** is rather simple:
- Auto-unwrap 3D mesh in **normalized UV space** (should respects relative dimensions)
- Initialize a 2D covariance matrix for our 2D Gaussians as: <br>
$\\
{\Sigma_{2D}} = \begin{bmatrix} \sigma^{2}_x & 0 \\\ 0 & \sigma^{2}_y \end{bmatrix}
\\$
where:
$\\
{\sigma_{x}}\sim {\sigma_{y}}\sim 0.5\\$
and $\\{\rho} = 0\\$
<br>
- Then, for each triangle primitive in the Geometry Shader stage, we do the following:
    - Gram-Schmidt orthonormalization to compute the rotation matrix (and quaternion).
    - Compute Jacobian matrix from *normalized UV space* to *3D space* for each triangle.
    - Now, in order to compute the 3D Covariance Matrix we do: $\\{\Sigma_{3D}} = J * \Sigma_{2D} * J^{T}  \\$
    - At this point, what we are interested from our 3D Covariance Matrix is not the rotation matrix made up of the eigenvectors, but just the eigenvalues. In order to compute the eigenvalues, we apply a matrix diagonalization method.
    - The packed scale values will be: $\\{packedScale_x} = {packedScale_y} = log(max(...eigenvalues))\\$ while instead $\\{packedScale_z} = log(1e-7)\\$
    <br>

- Now that we have the **Scale** and **Rotation** for a *3D Gaussian* part of a triangle, what we do is to emit one 3D Gaussian for each vertex of this triangle, setting their respective 3D position to the 3D position of the vertex while setting ``` gl_Position             = vec4(gs_in[i].normalizedUv * 2.0 - 1.0, 0.0, 1.0); ```. This means that the rasterizer will interpolate these values and generate one 3D Gaussian per fragment.
- In the Fragment/Pixel Shader we can do all our texture fetches and set information regarding Metallic, Roughness, Normals, etc...
- The `.ply` file format was modified in order to account for roughness and metallic properties

## Features

- **Direct 3D Model Processing**: Directly obtain a 3DGS model from a 3D mesh.
- **Texture map support**: For now, Mesh2Splat supports the following texture maps:
    - Diffuse
    - MetallicRoughness
    - Normal
    - Ambient Occlusion (wip)
    - Emissive (wip)
- **Enhanced Performance**: Significantly reduce the time needed to transform a 3D mesh into a 3DGS.
- **Relightability**: Compared to 3DGS scenes which are already lit, the 3DGS models obtained from this method have consistent normal information and are totally unlit.

## Results
Example of result 3DGS .ply file obtained by converting a 3D mesh. The results are rendered with a PBR shader in Halcyon. 
3D mesh viewed in Blender. <br>
The resulting .ply (on the right) is rendered in [Halcyon](https://gitlab.ea.com/seed/ray-machine/halcyon)

Here You can see on the LEFT the true normals extracted from the rotation matrix of the 3D Gaussians, in the center the normal computed by interpolating the tangent vector per vertex and retrieving the normal in tangent space from the normal map. On the right the final 3DGS model lit in real-time with PBR (diffuse + GGX).

<div style="display:flex;"> 
    <img src="res/normalFromRotMatrix.png" alt="Image Description 1" style="width: 200;">
    <img src="res/embeddedNormal.png" alt="Image Description 2" style="width: 200;"> 
    <img src="res/scifiMaskPreview.png" alt="Image Description 2" style="width: 200;"> 
</div>

<br>
Following are some other 3D meshes converted into 3DGS format.
<div style="display:flex;"> 
    <img src="res/mayan.png" alt="Image Description 1" style="width: 200;">
    <img src="res/robot.png" alt="Image Description 2" style="width: 200;"> 
    <img src="res/katana.png" alt="Image Description 2" style="width: 200;"> 
</div>

## Usage
Currently, in order to convert a 3D mesh into a 3DGS, you need to specify all the different parameters in ```src/utils/params.hpp```:
- ```OBJ_NAME```: the name of the object (will reflect both on the folder name and the .glb and texture name). Currently only `.gltf/.glb` file format is supported. The name of the folder and model has to be the same (I know, I need to change this).
- Modifying how this is handled is simple, but at the moment the code expects the following folder structure (do not nest your textures in subfolders):<br>
```
dataset/
├─ object_x/
│  ├─ object_x.<glb/gltf>
│  ├─ texture_diffuse_x.<png/jpeg/etc.>
│  ├─ texture_metallicRoughness_x.<png/jpeg/etc.>
│  ├─ texture_normal_x.<png/jpeg/etc.>
├─ object_y/
│  ├─ object_y.<glb/gltf>
│  ├─ texture_diffuse_y.<png/jpeg/etc.>
│  ├─ texture_metallicRoughness_y.<png/jpeg/etc.>
│  ├─ texture_normal_y.<png/jpeg/etc.>
```
- For the output, as I am working with Halcyon, I directly output the 3DGS model into ```/halcyon/Content/GaussianSplatting/```, in order to easily reload them from the selection menu.
- Modify in ```src/utils/params.hpp``` the `GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_1`. If you do not need a second output folder, just remove `GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_2`.


## Current Limitations and Next Steps

- **View-Dependent Accuracy**: Mesh2Splat is not capable of capturing view-dependant lighting effects. The main goal is to enable relighting. But embedding view-dependant effects in an efficient way without requiring the optimizer wants to be explored.
- **Material information**: Currently not all texture maps are supported, even if .gltf does. 
- **Textures still WIP for .glTF**: I am currently in the process of changing how I read the data from ```.obj``` to ```.glTF```, and how texture information is read is not too robust yet.

## Known issues
- **jpg format**: if using Blender and UV mapping with a ```.jpg``` texture, it will save it's `mimetype` as ```.jpeg```, invalidating some preliminary code I wrote. Just save it as ```.jpeg```, as it is equivalent to ```.jpg```


## Roadmap
- **GPU rasterization / GPU tesselation**: even though this approach is orders of magnitude faster than current ones which require the optimization stages, it is a full CPU implementation. The goal is to re-write it in order to perform most operation on the GPU. 
- **Make code more easily usable**: right now this is not a tool yet, it is code I wrote just for myself and was not expecting to be used by others. My goal is to make it a bit more user-friendly.
- **Explore better sampling strategies**: I want to improve an explore better sampling strategies in order to maximize gaussian coverage while also limiting wasted details. 

## References

[1] Bernhard Kerbl, Georgios Kopanas, Thomas Leimkühler, & George Drettakis. (2023). 3D Gaussian Splatting for Real-Time Radiance Field Rendering.<br>
[2] Antoine Guédon, & Vincent Lepetit. (2023). SuGaR: Surface-Aligned Gaussian Splatting for Efficient 3D Mesh Reconstruction and High-Quality Mesh Rendering.<br>
[3] Joanna Waczyńska, Piotr Borycki, Sławomir Tadeja, Jacek Tabor, & Przemysław Spurek. (2024). GaMeS: Mesh-Based Adapting and Modification of Gaussian Splatting.







