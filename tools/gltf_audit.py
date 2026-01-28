#!/usr/bin/env python3
import argparse
import json
import os
import struct


def read_glb_json(path):
    with open(path, "rb") as f:
        header = f.read(12)
        if len(header) < 12:
            raise ValueError("short header")
        magic, _version, _length = struct.unpack("<4sII", header)
        if magic != b"glTF":
            raise ValueError("bad magic")
        chunk_len, chunk_type = struct.unpack("<I4s", f.read(8))
        chunk_data = f.read(chunk_len)
        if chunk_type != b"JSON":
            chunk_len, chunk_type = struct.unpack("<I4s", f.read(8))
            chunk_data = f.read(chunk_len)
            if chunk_type != b"JSON":
                raise ValueError("no JSON chunk")
        json_text = chunk_data.decode("utf-8").rstrip(" \t\r\n\x00")
        return json.loads(json_text)


def read_gltf_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def is_identity_trs(node):
    t = node.get("translation", [0, 0, 0])
    r = node.get("rotation", [0, 0, 0, 1])
    s = node.get("scale", [1, 1, 1])
    return (
        abs(t[0]) < 1e-6 and abs(t[1]) < 1e-6 and abs(t[2]) < 1e-6 and
        abs(r[0]) < 1e-6 and abs(r[1]) < 1e-6 and abs(r[2]) < 1e-6 and abs(r[3] - 1.0) < 1e-6 and
        abs(s[0] - 1.0) < 1e-6 and abs(s[1] - 1.0) < 1e-6 and abs(s[2] - 1.0) < 1e-6 and
        "matrix" not in node
    )


def summarize_file(path):
    ext = os.path.splitext(path)[1].lower()
    if ext == ".glb":
        gltf = read_glb_json(path)
    elif ext == ".gltf":
        gltf = read_gltf_json(path)
    else:
        raise ValueError("unsupported file: " + path)

    mesh_info = []
    for mesh in gltf.get("meshes", []):
        for prim in mesh.get("primitives", []):
            attrs = prim.get("attributes", {})
            mesh_info.append({
                "POSITION": "POSITION" in attrs,
                "NORMAL": "NORMAL" in attrs,
                "TEXCOORD_0": "TEXCOORD_0" in attrs,
                "TANGENT": "TANGENT" in attrs,
                "TEXCOORD_1": "TEXCOORD_1" in attrs,
                "COLOR_0": "COLOR_0" in attrs,
                "JOINTS_0": "JOINTS_0" in attrs,
                "WEIGHTS_0": "WEIGHTS_0" in attrs,
                "INDICES": "indices" in prim,
            })

    def count_true(key):
        return sum(1 for m in mesh_info if m.get(key))

    materials = gltf.get("materials", [])
    mat_info = {
        "materials": len(materials),
        "baseColorTexture": 0,
        "normalTexture": 0,
        "metallicRoughnessTexture": 0,
        "occlusionTexture": 0,
        "emissiveTexture": 0,
    }
    for mat in materials:
        pbr = mat.get("pbrMetallicRoughness", {})
        if "baseColorTexture" in pbr:
            mat_info["baseColorTexture"] += 1
        if "metallicRoughnessTexture" in pbr:
            mat_info["metallicRoughnessTexture"] += 1
        if "normalTexture" in mat:
            mat_info["normalTexture"] += 1
        if "occlusionTexture" in mat:
            mat_info["occlusionTexture"] += 1
        if "emissiveTexture" in mat:
            mat_info["emissiveTexture"] += 1

    nodes = gltf.get("nodes", [])
    parents = {i: [] for i in range(len(nodes))}
    for idx, node in enumerate(nodes):
        for child in node.get("children", []):
            parents[child].append(idx)

    node_identity = [is_identity_trs(n) for n in nodes]

    def has_non_identity_ancestor(idx, visited=None):
        if visited is None:
            visited = set()
        for p in parents.get(idx, []):
            if p in visited:
                continue
            visited.add(p)
            if not node_identity[p]:
                return True
            if has_non_identity_ancestor(p, visited):
                return True
        return False

    mesh_nodes = [i for i, n in enumerate(nodes) if "mesh" in n]
    mesh_nodes_non_identity = sum(1 for i in mesh_nodes if not node_identity[i])
    mesh_nodes_ancestor_non_identity = sum(1 for i in mesh_nodes if has_non_identity_ancestor(i))

    out = []
    out.append(os.path.basename(path))
    out.append(f"  primitives: {len(mesh_info)}")
    if mesh_info:
        out.append(f"  POSITION: {count_true('POSITION')} / {len(mesh_info)}")
        out.append(f"  NORMAL: {count_true('NORMAL')} / {len(mesh_info)}")
        out.append(f"  TEXCOORD_0: {count_true('TEXCOORD_0')} / {len(mesh_info)}")
        out.append(f"  TANGENT: {count_true('TANGENT')} / {len(mesh_info)}")
        out.append(f"  TEXCOORD_1: {count_true('TEXCOORD_1')} / {len(mesh_info)}")
        out.append(f"  COLOR_0: {count_true('COLOR_0')} / {len(mesh_info)}")
        out.append(f"  JOINTS_0: {count_true('JOINTS_0')} / {len(mesh_info)}")
        out.append(f"  WEIGHTS_0: {count_true('WEIGHTS_0')} / {len(mesh_info)}")
        out.append(f"  INDICES: {count_true('INDICES')} / {len(mesh_info)}")
    out.append("  materials: " + json.dumps(mat_info))
    out.append(f"  mesh nodes: {len(mesh_nodes)}")
    out.append(f"  mesh nodes with non-identity TRS: {mesh_nodes_non_identity}")
    out.append(f"  mesh nodes with non-identity ancestor: {mesh_nodes_ancestor_non_identity}")
    return "\n".join(out)


def main():
    parser = argparse.ArgumentParser(description="Audit glTF/GLB files for attributes/materials/transforms.")
    parser.add_argument("paths", nargs="+", help="Files or directories to scan.")
    args = parser.parse_args()

    files = []
    for p in args.paths:
        if os.path.isdir(p):
            for name in os.listdir(p):
                if name.lower().endswith((".glb", ".gltf")):
                    files.append(os.path.join(p, name))
        else:
            files.append(p)

    for path in files:
        try:
            print(summarize_file(path))
            print()
        except Exception as e:
            print(os.path.basename(path) + " ERROR " + str(e))


if __name__ == "__main__":
    main()
