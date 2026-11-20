# Model resource format

`nori::asset::cook_model()` writes a split model resource set. The requested model path owns a
small `NMDL` manifest binary and its children contain independent mesh, animation, and image data.
The previous single-binary payload is not supported and must be reimported.

```text
model/
  meshes/mesh-0000
  animations/clip-0000
  textures/texture-0000-bc7-srgb
```

## Manifest

The root binary starts with `NMDL`, little-endian version `1`, and five sized sections:

- `NODE`: node name, parent index, mesh index, local TRS, and material indices.
- `MATL`: PBR factors and six cooked texture indices.
- `TEXR`: cooked texture resource names and source names.
- `MESH`: `meshes/mesh-####` resource names.
- `ANIM`: `animations/clip-####` resource names.

Strings are UTF-8 and prefixed with their `uint32` byte length. Each section has a four-byte tag
and `uint64` payload size.

## Mesh and animation payloads

Each mesh resource starts with `NMSH` and version `1`, followed by its name, triangle-list vertex
and index data, per-triangle material slots, and optional skin clusters. A vertex stores position,
normal, tangent/sign, UV, four joint indices, and four weights. Missing joint indices use
`UINT32_MAX`.

Each animation resource starts with `NANI` and version `1`, followed by its name, frame rate,
frame count, node count, and frame-major node TRS samples.

Base color and emission textures use BC7 sRGB; normal maps use BC5 linear; roughness, metalness,
and opacity use BC7 linear. Every compressed texture stores its full offline mip chain.
