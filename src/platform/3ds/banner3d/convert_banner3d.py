#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""glTF -> HOME Menu banner CGFX, using skyfloogle/pycgfx as a library, plus the CGFX-only post-patches.

Run with the pycgfx venv's python (make_banner3d.sh does this):
    .tools/pycgfx/.venv/bin/python convert_banner3d.py banner_scene.gltf banner3d.cgfx --pycgfx-root .tools/pycgfx

Post-patches (after Epic0522's hardware-tested tools/banner/convert_banner_cgfx.py, ClouDS-Music-FA):
  * matte materials: pycgfx's PBR conversion adds a white specular term that washes the model out under the
    HOME Menu light -> constant0 = black, bypass TEV stage 2 (specular add), no distribution LUT.
    Diffuse lighting (TEV stage 1) is kept so the logo keeps its shape while the menu rotates it.
  * every SOBJ mesh gets mesh_node_name = the bone (glTF node) it belongs to.
  * optional 'backdrop' / 'name' nodes -> YAxial billboard (mode 5).  The animated 'logo' is never billboarded
    (that would cancel its sway).
Validation (fails the build): CMDL named COMMON, no skinning (skinning_mode 0), no bone index / weight
streams, file < 512 KiB; if the glTF is animated: skeletal CANM named COMMON whose members are exactly the
expected rigid nodes ({'logo'}) and all ordinary Transform (5) members (guide: soft skinning or other member
kinds can freeze the real HOME Menu).
"""
import argparse
import sys
from pathlib import Path

MAX_CGFX = 0x80000  # HOME Menu limit for the decompressed banner CGFX (3dbrew CBMD)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("input_gltf", type=Path)
    ap.add_argument("output_cgfx", type=Path)
    ap.add_argument("--pycgfx-root", type=Path, default=Path(".tools/pycgfx"))
    ap.add_argument("--keep-specular", action="store_true", help="do not apply the matte patch")
    ap.add_argument("--animated-nodes", default="logo", help="comma list of nodes that must be animated")
    args = ap.parse_args()

    root = args.pycgfx_root.resolve()
    if not (root / "main.py").is_file():
        raise SystemExit(f"pycgfx not found at {root}")
    sys.path.insert(0, str(root))
    import gltflib  # type: ignore
    import main as pycgfx  # type: ignore
    from cgfx.mtob import ColorFloat, FragmentLightingFlags  # type: ignore
    from cgfx.primitives import VertexAttributeUsage  # type: ignore
    from cgfx.sobj import BillboardMode  # type: ignore
    from cgfx.canm import CANMBoneTransform, PrimitiveType  # type: ignore

    gltf = gltflib.GLTF.load(str(args.input_gltf.resolve()), load_file_resources=True)
    animated = bool(gltf.model.animations)
    cgfx = pycgfx.convert_gltf(gltf)

    model_names = list(cgfx.data.models)
    if model_names != ["COMMON"]:
        raise SystemExit(f"expected exactly one CMDL named COMMON, got {model_names}")
    model = cgfx.data.models["COMMON"]
    skeleton = getattr(model, "skeleton", None)
    if skeleton is None:
        raise SystemExit("converted model has no skeleton (pycgfx always makes one - wrong pycgfx version?)")
    bone_names = list(skeleton.bones)

    # mesh_node_name: pycgfx names each SOBJ mesh "<glTF mesh name>_<material>"; our glTF mesh names equal
    # their node names ("logo", "backdrop").
    nodes = []
    for mesh in model.meshes.data.contents:
        node = next((b for b in bone_names if mesh.name == b or mesh.name.startswith(b + "_")), None)
        if node is None:
            raise SystemExit(f"cannot resolve the node of SOBJ mesh {mesh.name!r} (bones: {bone_names})")
        mesh.mesh_node_name = node
        nodes.append(f"{mesh.name}->{node}")

    billboarded = []
    for bb in ("backdrop", "name"):
        if skeleton.bones[bb] is not None:
            skeleton.bones[bb].billboard_mode = BillboardMode.YAxial
            billboarded.append(bb)

    anim_info = "static"
    if animated:
        expected = set(x for x in args.animated_nodes.split(",") if x)
        canm = cgfx.data.skeletal_animations["COMMON"]
        if canm is None:
            raise SystemExit("glTF is animated but the CGFX has no skeletal CANM named COMMON")
        names = list(cgfx.data.skeletal_animations)
        if names != ["COMMON"]:
            raise SystemExit(f"expected exactly one skeletal animation COMMON, got {names}")
        members = {n: canm.member_animations_data[n] for n in canm.member_animations_data}
        if set(members) != expected:
            raise SystemExit(f"animation members {sorted(members)} != expected rigid nodes {sorted(expected)}")
        for n, mem in members.items():
            if not isinstance(mem, CANMBoneTransform) or mem.primitive_type != PrimitiveType.Transform:
                raise SystemExit(f"animation member {n} is not an ordinary Transform (5) member")
            if skeleton.bones[n] is None or skeleton.bones[n].billboard_mode != BillboardMode.Off:
                raise SystemExit(f"animated node {n} missing or billboarded")
        if cgfx.data.material_animations.len() or cgfx.data.visibility_animations.len():
            raise SystemExit("unexpected material/visibility CANMs (only the rigid COMMON skeletal one is allowed)")
        anim_info = (f"CANM COMMON, members {sorted(members)} = Transform(5), "
                     f"{canm.frame_size:g} frames @60, looping {canm.looping}")

    for shape in model.shapes.data.contents:
        for ps in shape.primitive_sets.data.contents:
            if ps.skinning_mode != 0:
                raise SystemExit(f"{shape.name}: skinning mode {ps.skinning_mode} (must be 0)")
        for attr in shape.vertex_attributes.data.contents:
            usages = attr.vertex_streams.data.contents if hasattr(attr, "vertex_streams") else (attr,)
            for u in usages:
                if u.usage in (VertexAttributeUsage.BoneIndex, VertexAttributeUsage.BoneWeight):
                    raise SystemExit(f"{shape.name}: contains bone index/weight streams")

    matte = []
    if not args.keep_specular:
        for name in model.materials:
            m = model.materials[name]
            m.material_color.constant[0] = ColorFloat(0, 0, 0, 1)
            stage = m.fragment_shader.texture_combiners[2]
            stage.src_rgb = 0xFFF        # previous, previous, previous
            stage.combine_rgb = 0        # replace -> pass the diffuse-lit colour through
            m.fragment_shader.fragment_lighting.flags = FragmentLightingFlags(0)
            m.fragment_shader.fragment_lighting_table.distribution_0_sampler = None
            matte.append(name)

    data = pycgfx.write(cgfx)
    if data[:4] != b"CGFX":
        raise SystemExit("pycgfx produced no CGFX")
    if len(data) >= MAX_CGFX:
        raise SystemExit(f"CGFX is {len(data)} bytes; the HOME Menu limit is {MAX_CGFX} (512 KiB)")
    args.output_cgfx.parent.mkdir(parents=True, exist_ok=True)
    args.output_cgfx.write_bytes(data)
    print(f"wrote {args.output_cgfx} ({len(data)} bytes = {len(data) / 1024:.1f} KiB, limit 512 KiB)")
    print(f"  model COMMON, bones {bone_names}")
    print(f"  meshes {nodes}; billboard YAxial: {billboarded or 'none'}; matte: {matte or 'off'}")
    print(f"  animation: {anim_info}")


if __name__ == "__main__":
    main()
