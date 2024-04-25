# Mesh2Splat

## Introduction
Welcome to **Mesh2Splat**, a novel approach to convert 3D meshes into 3DGS (3D Gaussian Splatting) models.<br>
3DGS was born to recontruct a photorealistic 3D representation from a series of images. This is possible thanks to an optimization process which relies on a series of reference images which are compared to 3DGS rendered scene obtained via a differentiable renderer that makes it possible to optimize a set of initial scene parameters [1].<br>
What if we want to represent a synthetic object (3D model) in 3DGS format rather than a real scene? Currently, the only way to do so is to generate a synthetic dataset (camera poses, renders and sparse point cloud) of the 3D model, and feed this into the 3DGS pipeline. This process can take up to 30min-45min.<br><br>
**Mesh2Splat** instead, by directly using the geometry, materials and texture information from the 3D model, rather than going through the classical 3DGS pipeline, is able to obtain a 3DGS representation of the input 3D models in seconds.<br>
This methodology sidesteps the need for greater interoperability between classical 3D mesh representation and the novel 3DGS format.<br>


## Concept
The (current) core concept behind Mesh2Splat is quite simple:
- Initially "rasterize" a series of 3D Gaussians in 2D UV space, with isotropic scaling in 2D space.
- Use the UV mapping to place the 3D gaussians back in 3D space
- As previous work suggests [1, 2], flatten the Gaussians along the normal making them anisotropic helps to better approximate the surface of the mesh.
- Retrieve the material information from the model and embed this information into the 3DGS .ply file.

## Features

- **Direct 3D Model Processing**: Directly obtain a 3DGS model from a 3D mesh.
- **Enhanced Performance**: Significantly reduce the time needed to transform a 3D mesh into a 3DGS.
- **Relightability**: Compared to 3DGS scenes which are already lit, the 3DGS models obtained from this method have consistent normal information and are totally unlit.

## Usage
Currently, in order to convert a 3D mesh into a 3DGS, you need to specify all the different parameters in ```src/utils/params.hpp```:
- ```OBJ_NAME```: the name of the object (will reflect both on the folder name and the .glb and texture name)
- Modifying how this is handled is simple, but at the moment the code expects the following folder structure:<br>
**dataset/**<br>
├─ **object_x/**<br>
│  ├─ object_x.glb<br>
│  ├─ object_x.png<br>
├─ **object_y/**<br>
│  ├─ object_y.glb<br>
│  ├─ object_x.png<br>



## Current Limitations

- **View-Dependent Accuracy**: Compared to traditional

## References

[1] Bernhard Kerbl, Georgios Kopanas, Thomas Leimkühler, & George Drettakis. (2023). 3D Gaussian Splatting for Real-Time Radiance Field Rendering.
[2] Antoine Guédon, & Vincent Lepetit. (2023). SuGaR: Surface-Aligned Gaussian Splatting for Efficient 3D Mesh Reconstruction and High-Quality Mesh Rendering.
[3] Joanna Waczyńska, Piotr Borycki, Sławomir Tadeja, Jacek Tabor, & Przemysław Spurek. (2024). GaMeS: Mesh-Based Adapting and Modification of Gaussian Splatting.







