import json
import re
import sys
from pathlib import Path

import bpy


TARGET_NODE_TREES = {
    "Paint Filter",
    ".Brush Alpha",
    ".Painter's Color Filter",
    ".Fake Lighting",
    ".Camera Plane",
    ".Contrast Node",
    ".Flatten in Camera Space",
    ".Vector at Opposite Edges",
    "Geometry Nodes",
}


def scalar(value):
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if hasattr(value, "__iter__") and not isinstance(value, (bytes, bytearray)):
        try:
            items = list(value)
            if len(items) <= 32 and all(isinstance(x, (bool, int, float, str)) for x in items):
                return items
        except TypeError:
            pass
    if hasattr(value, "name"):
        return getattr(value, "name")
    return str(value)


def clean(value):
    if isinstance(value, str):
        return re.sub(r"\s+", " ", value.strip())
    return value


def socket_info(socket, index):
    data = {
        "index": index,
        "name": socket.name,
        "identifier": getattr(socket, "identifier", ""),
        "bl_idname": getattr(socket, "bl_idname", ""),
        "type": getattr(socket, "type", ""),
        "enabled": getattr(socket, "enabled", True),
        "hide": getattr(socket, "hide", False),
        "is_linked": getattr(socket, "is_linked", False),
    }
    for prop in ("default_value", "min_value", "max_value", "subtype", "attribute_domain"):
        if hasattr(socket, prop):
            try:
                data[prop] = scalar(getattr(socket, prop))
            except Exception as exc:
                data[f"{prop}_error"] = str(exc)
    return data


def node_props(node):
    skip = {
        "rna_type",
        "name",
        "label",
        "location",
        "width",
        "height",
        "inputs",
        "outputs",
        "internal_links",
        "select",
        "parent",
        "id_data",
    }
    out = {}
    for prop in node.bl_rna.properties:
        ident = prop.identifier
        if ident in skip or prop.is_readonly:
            continue
        try:
            value = getattr(node, ident)
        except Exception:
            continue
        try:
            out[ident] = clean(scalar(value))
        except Exception:
            pass

    if hasattr(node, "image") and node.image:
        out["image"] = node.image.name
        out["image_filepath"] = bpy.path.abspath(node.image.filepath) if node.image.filepath else ""
    if hasattr(node, "node_tree") and node.node_tree:
        out["node_tree"] = node.node_tree.name
    if hasattr(node, "color_ramp") and node.color_ramp:
        out["color_ramp"] = [
            {"position": element.position, "color": scalar(element.color)}
            for element in node.color_ramp.elements
        ]
    if hasattr(node, "mapping") and node.mapping:
        try:
            out["mapping"] = {
                "curve": [
                    [{"location": scalar(point.location)} for point in curve.points]
                    for curve in node.mapping.curves
                ]
            }
        except Exception:
            pass
    return out


def dump_tree(tree):
    nodes = []
    node_index = {node: index for index, node in enumerate(tree.nodes)}
    for index, node in enumerate(tree.nodes):
        nodes.append(
            {
                "index": index,
                "name": node.name,
                "label": node.label,
                "bl_idname": node.bl_idname,
                "type": node.type,
                "parent": node.parent.name if node.parent else "",
                "location": scalar(node.location),
                "dimensions": scalar(getattr(node, "dimensions", [])),
                "mute": getattr(node, "mute", False),
                "hide": getattr(node, "hide", False),
                "properties": node_props(node),
                "inputs": [socket_info(socket, i) for i, socket in enumerate(node.inputs)],
                "outputs": [socket_info(socket, i) for i, socket in enumerate(node.outputs)],
            }
        )

    links = []
    for link in tree.links:
        links.append(
            {
                "from_node": link.from_node.name,
                "from_node_index": node_index.get(link.from_node, -1),
                "from_socket": link.from_socket.name,
                "from_socket_index": list(link.from_node.outputs).index(link.from_socket),
                "from_socket_identifier": getattr(link.from_socket, "identifier", ""),
                "to_node": link.to_node.name,
                "to_node_index": node_index.get(link.to_node, -1),
                "to_socket": link.to_socket.name,
                "to_socket_index": list(link.to_node.inputs).index(link.to_socket),
                "to_socket_identifier": getattr(link.to_socket, "identifier", ""),
                "is_muted": getattr(link, "is_muted", False),
            }
        )

    interface = []
    if hasattr(tree, "interface"):
        for item in tree.interface.items_tree:
            row = {
                "name": item.name,
                "identifier": getattr(item, "identifier", ""),
                "item_type": item.item_type,
                "in_out": getattr(item, "in_out", ""),
                "socket_type": getattr(item, "socket_type", ""),
                "parent": item.parent.name if getattr(item, "parent", None) else "",
            }
            for prop in ("default_value", "min_value", "max_value", "subtype", "attribute_domain", "description"):
                if hasattr(item, prop):
                    try:
                        row[prop] = clean(scalar(getattr(item, prop)))
                    except Exception:
                        pass
            interface.append(row)

    return {
        "name": tree.name,
        "bl_idname": tree.bl_idname,
        "type": getattr(tree, "type", ""),
        "interface": interface,
        "nodes": nodes,
        "links": links,
    }


def modifier_data(modifier):
    props = {}
    for key in modifier.keys():
        try:
            props[key] = scalar(modifier[key])
        except Exception:
            props[key] = "<unserializable>"
    return {
        "name": modifier.name,
        "type": modifier.type,
        "node_group": modifier.node_group.name if getattr(modifier, "node_group", None) else "",
        "properties": props,
    }


def main():
    if "--" not in sys.argv:
        raise SystemExit("usage: blender --background file.blend --python tools/dump_live_paint_graph.py -- out.json")
    out_path = Path(sys.argv[sys.argv.index("--") + 1])
    out_path.parent.mkdir(parents=True, exist_ok=True)

    material_trees = {}
    for material in bpy.data.materials:
        if material.use_nodes and material.node_tree and ("Refractive" in material.name or material.name == "Material"):
            material_trees[material.name] = dump_tree(material.node_tree)

    data = {
        "filepath": bpy.data.filepath,
        "blender_version": bpy.app.version_string,
        "node_groups": {
            tree.name: dump_tree(tree)
            for tree in bpy.data.node_groups
            if tree.name in TARGET_NODE_TREES
        },
        "materials": material_trees,
        "objects": [
            {
                "name": obj.name,
                "type": obj.type,
                "modifiers": [modifier_data(mod) for mod in obj.modifiers],
            }
            for obj in bpy.data.objects
            if any(getattr(mod, "node_group", None) and mod.node_group.name in TARGET_NODE_TREES for mod in obj.modifiers)
        ],
    }

    out_path.write_text(json.dumps(data, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
