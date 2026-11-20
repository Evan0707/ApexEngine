"""Generates the demo assets and the importer test files (decision 0014).

Run from the repository root, with the environment of Tools/requirements.txt:

    py -m venv Tools/.venv
    Tools/.venv/Scripts/python -m pip install -r Tools/requirements.txt
    Tools/.venv/Scripts/python Tools/generate_assets.py

The outputs are committed: the build never runs this script. Same inputs, same bytes.
"""

import base64
import io
import json
import math
import pathlib
import struct

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
MODELS = ROOT / "Assets" / "Models"
ENVIRONMENTS = ROOT / "Assets" / "Environments"
TEST_DATA = ROOT / "Tests" / "Data"

UNSIGNED_BYTE, UNSIGNED_SHORT, UNSIGNED_INT, FLOAT = 5121, 5123, 5125, 5126
ARRAY_BUFFER, ELEMENT_ARRAY_BUFFER = 34962, 34963
FORMATS = {UNSIGNED_BYTE: "B", UNSIGNED_SHORT: "H", UNSIGNED_INT: "I", FLOAT: "f"}
COMPONENTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}


class GltfBuilder:
    """A glTF 2.0 document and its single binary buffer."""

    def __init__(self):
        self.doc = {
            "asset": {"version": "2.0", "generator": "Apex Engine Tools/generate_assets.py"},
            "scene": 0,
            "scenes": [{"nodes": []}],
        }
        self.binary = bytearray()

    def _append(self, key, value):
        self.doc.setdefault(key, []).append(value)
        return len(self.doc[key]) - 1

    def buffer_view(self, data, target=None):
        while len(self.binary) % 4:
            self.binary.append(0)
        view = {"buffer": 0, "byteOffset": len(self.binary), "byteLength": len(data)}
        if target is not None:
            view["target"] = target
        self.binary.extend(data)
        return self._append("bufferViews", view)

    def accessor(self, values, component_type, kind, target=None):
        flat = [component for value in values for component in (value if isinstance(value, tuple) else (value,))]
        data = struct.pack("<%d%s" % (len(flat), FORMATS[component_type]), *flat)
        accessor = {
            "bufferView": self.buffer_view(data, target),
            "componentType": component_type,
            "count": len(values),
            "type": kind,
        }
        if kind == "VEC3" and component_type == FLOAT:
            accessor["min"] = [min(value[i] for value in values) for i in range(3)]
            accessor["max"] = [max(value[i] for value in values) for i in range(3)]
        return self._append("accessors", accessor)

    def primitive(self, positions, normals=None, uvs=None, indices=None, index_type=UNSIGNED_SHORT, material=None):
        attributes = {"POSITION": self.accessor(positions, FLOAT, "VEC3", ARRAY_BUFFER)}
        if normals is not None:
            attributes["NORMAL"] = self.accessor(normals, FLOAT, "VEC3", ARRAY_BUFFER)
        if uvs is not None:
            attributes["TEXCOORD_0"] = self.accessor(uvs, FLOAT, "VEC2", ARRAY_BUFFER)
        primitive = {"attributes": attributes}
        if indices is not None:
            primitive["indices"] = self.accessor(indices, index_type, "SCALAR", ELEMENT_ARRAY_BUFFER)
        if material is not None:
            primitive["material"] = material
        return primitive

    def mesh(self, name, primitives):
        return self._append("meshes", {"name": name, "primitives": primitives})

    def node(self, root=True, **fields):
        index = self._append("nodes", fields)
        if root:
            self.doc["scenes"][0]["nodes"].append(index)
        return index

    def texture_from_bytes(self, data, mime):
        image = self._append("images", {"bufferView": self.buffer_view(data), "mimeType": mime})
        return self._texture(image)

    def texture_from_uri(self, uri):
        return self._texture(self._append("images", {"uri": uri}))

    def _texture(self, image):
        if "samplers" not in self.doc:
            # Linear filtering, repeat: what the engine uses anyway (decision 0014).
            self.doc["samplers"] = [{"magFilter": 9729, "minFilter": 9729, "wrapS": 10497, "wrapT": 10497}]
        return self._append("textures", {"source": image, "sampler": 0})

    def material(self, name, color=(1.0, 1.0, 1.0, 1.0), metallic=1.0, roughness=1.0, color_texture=None,
                 metal_rough_texture=None, normal_texture=None, normal_scale=None):
        pbr = {"baseColorFactor": list(color), "metallicFactor": metallic, "roughnessFactor": roughness}
        if color_texture is not None:
            pbr["baseColorTexture"] = {"index": color_texture}
        if metal_rough_texture is not None:
            pbr["metallicRoughnessTexture"] = {"index": metal_rough_texture}
        material = {"name": name, "pbrMetallicRoughness": pbr}
        if normal_texture is not None:
            material["normalTexture"] = {"index": normal_texture}
            if normal_scale is not None:
                material["normalTexture"]["scale"] = normal_scale
        return self._append("materials", material)

    def _json_bytes(self):
        return json.dumps(self.doc, indent=2, sort_keys=True).encode("utf-8")

    def write_gltf(self, path, binary_uri=None):
        """External .bin next to the file when binary_uri is given, otherwise a base64 data URI."""
        if binary_uri is None:
            uri = "data:application/octet-stream;base64," + base64.b64encode(bytes(self.binary)).decode("ascii")
        else:
            uri = binary_uri
            (path.parent / binary_uri).write_bytes(bytes(self.binary))
        self.doc["buffers"] = [{"byteLength": len(self.binary), "uri": uri}]
        path.write_bytes(self._json_bytes() + b"\n")

    def write_glb(self, path):
        while len(self.binary) % 4:
            self.binary.append(0)
        self.doc["buffers"] = [{"byteLength": len(self.binary)}]
        text = self._json_bytes()
        text += b" " * (-len(text) % 4)
        chunks = struct.pack("<II", len(text), 0x4E4F534A) + text
        chunks += struct.pack("<II", len(self.binary), 0x004E4942) + bytes(self.binary)
        path.write_bytes(struct.pack("<III", 0x46546C67, 2, 12 + len(chunks)) + chunks)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def normalize(v):
    length = math.sqrt(sum(c * c for c in v))
    return tuple(c / length for c in v)


# Same layout as resources::makeCube: (normal, tangent), up = cross(normal, tangent).
CUBE_FACES = [
    ("+X", (1, 0, 0), (0, 0, -1)),
    ("-X", (-1, 0, 0), (0, 0, 1)),
    ("+Y", (0, 1, 0), (1, 0, 0)),
    ("-Y", (0, -1, 0), (1, 0, 0)),
    ("+Z", (0, 0, 1), (1, 0, 0)),
    ("-Z", (0, 0, -1), (-1, 0, 0)),
]


def cube_geometry(size, uv_rect):
    """Counter-clockwise from outside; uv_rect(face_index) gives (u0, v0, u1, v1), v0 at the top."""
    half = size / 2
    positions, normals, uvs, indices = [], [], [], []
    for face_index, (_, normal, tangent) in enumerate(CUBE_FACES):
        up = cross(normal, tangent)
        u0, v0, u1, v1 = uv_rect(face_index)
        base = len(positions)
        for cx, cy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            positions.append(tuple(float((normal[i] + cx * tangent[i] + cy * up[i]) * half) for i in range(3)))
            normals.append(tuple(float(n) for n in normal))
            uvs.append((u0 + (u1 - u0) * (cx + 1) / 2, v0 + (v1 - v0) * (1 - cy) / 2))
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]
    return positions, normals, uvs, indices


def sphere_geometry(radius, segments, rings):
    """Same layout as resources::makeSphere: poles on Y, u around from +Z towards +X, v from the north pole."""
    positions, normals, uvs, indices = [], [], [], []
    for ring in range(rings + 1):
        v = ring / rings
        polar = v * math.pi
        for segment in range(segments + 1):
            u = segment / segments
            azimuth = u * 2 * math.pi
            normal = (math.sin(polar) * math.sin(azimuth), math.cos(polar), math.sin(polar) * math.cos(azimuth))
            positions.append(tuple(component * radius for component in normal))
            normals.append(normal)
            uvs.append((u, v))
    stride = segments + 1
    for ring in range(rings):
        for segment in range(segments):
            top_left = ring * stride + segment
            bottom_left = top_left + stride
            # The triangle that would collapse on a pole is skipped.
            if ring + 1 < rings:
                indices += [top_left, bottom_left, bottom_left + 1]
            if ring > 0:
                indices += [top_left, bottom_left + 1, top_left + 1]
    return positions, normals, uvs, indices


def primitives():
    """A cube of 1 m and a sphere of 1 m in diameter, white and untextured: scenes color them (decision 0017)."""
    for name, geometry in (("cube", cube_geometry(1.0, lambda _: (0.0, 0.0, 1.0, 1.0))),
                           ("sphere", sphere_geometry(0.5, 48, 24))):
        builder = GltfBuilder()
        material = builder.material("White", metallic=0.0, roughness=0.5)
        positions, normals, uvs, indices = geometry
        title = name.capitalize()
        mesh = builder.mesh(title, [builder.primitive(positions, normals, uvs, indices, material=material)])
        builder.node(name=title, mesh=mesh)
        builder.write_glb(MODELS / (name + ".glb"))


def capsule_geometry(radius, height, segments, rings):
    """An upright capsule whose bottom is at y = 0: a sphere cut at its equator, the halves pulled apart."""
    half_cylinder = height / 2 - radius
    rows = [(ring / rings * math.pi, half_cylinder) for ring in range(rings // 2 + 1)]
    rows += [(ring / rings * math.pi, -half_cylinder) for ring in range(rings // 2, rings + 1)]
    positions, normals, uvs, indices = [], [], [], []
    for row, (polar, offset) in enumerate(rows):
        for segment in range(segments + 1):
            u = segment / segments
            azimuth = u * 2 * math.pi
            normal = (math.sin(polar) * math.sin(azimuth), math.cos(polar), math.sin(polar) * math.cos(azimuth))
            positions.append((normal[0] * radius, normal[1] * radius + offset + height / 2, normal[2] * radius))
            normals.append(normal)
            uvs.append((u, row / (len(rows) - 1)))
    stride = segments + 1
    for row in range(len(rows) - 1):
        for segment in range(segments):
            top_left = row * stride + segment
            bottom_left = top_left + stride
            # The triangle that would collapse on a pole is skipped.
            if row + 2 < len(rows):
                indices += [top_left, bottom_left, bottom_left + 1]
            if row > 0:
                indices += [top_left, bottom_left + 1, top_left + 1]
    return positions, normals, uvs, indices


def character():
    """The demo's character (spec 5.8): a capsule of 1.8 m, feet on the origin, with a visor on its front (-Z)."""
    builder = GltfBuilder()
    body = builder.material("CharacterBody", color=(0.85, 0.45, 0.1, 1.0), metallic=0.0, roughness=0.6)
    visor = builder.material("CharacterVisor", color=(0.05, 0.05, 0.07, 1.0), metallic=0.0, roughness=0.2)
    positions, normals, uvs, indices = capsule_geometry(0.3, 1.8, 32, 16)
    # A box 36 cm wide across the face, sticking 6 cm out of the capsule at eye height.
    box_positions, box_normals, box_uvs, box_indices = cube_geometry(1.0, lambda _: (0.0, 0.0, 1.0, 1.0))
    box_positions = [(x * 0.36, y * 0.12 + 1.5, z * 0.2 - 0.26) for x, y, z in box_positions]
    mesh = builder.mesh("Character", [
        builder.primitive(positions, normals, uvs, indices, material=body),
        builder.primitive(box_positions, box_normals, box_uvs, box_indices, material=visor)])
    builder.node(name="Character", mesh=mesh)
    builder.write_glb(MODELS / "character.glb")


def image_bytes(image, fmt, **options):
    stream = io.BytesIO()
    image.save(stream, fmt, **options)
    return stream.getvalue()


def reference_cube():
    """Spec 6.1: 1 m, each face labeled with its axis, the text upright on the face."""
    cell = 256
    atlas = Image.new("RGB", (3 * cell, 2 * cell))
    draw = ImageDraw.Draw(atlas)
    font = ImageFont.load_default(size=110)
    colors = {"X": (200, 60, 50), "Y": (60, 170, 70), "Z": (60, 90, 200)}
    for index, (label, _, _) in enumerate(CUBE_FACES):
        left, top = (index % 3) * cell, (index // 3) * cell
        color = colors[label[1]] if label[0] == "+" else tuple(c // 2 for c in colors[label[1]])
        draw.rectangle([left, top, left + cell - 1, top + cell - 1], fill=color, outline=(240, 240, 240), width=8)
        draw.text((left + cell / 2, top + cell / 2), label, fill=(255, 255, 255), font=font, anchor="mm")
        # A bar along the top edge: the top of the texture, whatever the label's shape.
        draw.rectangle([left + 24, top + 20, left + cell - 25, top + 34], fill=(255, 255, 255))

    def cell_rect(index):
        col, row = index % 3, index // 3
        return (col / 3, row / 2, (col + 1) / 3, (row + 1) / 2)

    builder = GltfBuilder()
    texture = builder.texture_from_bytes(image_bytes(atlas, "PNG", optimize=True), "image/png")
    material = builder.material("ReferenceCube", metallic=0.0, roughness=0.6, color_texture=texture)
    positions, normals, uvs, indices = cube_geometry(1.0, cell_rect)
    mesh = builder.mesh("ReferenceCube", [builder.primitive(positions, normals, uvs, indices, material=material)])
    builder.node(name="ReferenceCube", mesh=mesh)
    builder.write_glb(MODELS / "reference_cube.glb")


def crate_height(size):
    """The relief of the crate's face, matching its color texture: grooves between the planks, grain lines a
    little lower, a raised frame and raised metal corners."""
    height = Image.new("L", (size, size), 128)
    draw = ImageDraw.Draw(height)
    for plank in range(6):
        top = plank * size // 6
        draw.rectangle([0, top, size - 1, top + 2], fill=40)
        draw.rectangle([0, top + size // 6 - 3, size - 1, top + size // 6 - 1], fill=40)
        for grain in range(5):
            y = top + 12 + grain * 13
            draw.line([(20, y), (size - 20, y + (grain % 2) * 4)], fill=112, width=2)
    frame = 40
    for rect in ([0, 0, size - 1, frame], [0, size - frame, size - 1, size - 1], [0, 0, frame, size - 1],
                 [size - frame, 0, size - 1, size - 1]):
        draw.rectangle(rect, fill=176)
    corner = 72
    for x, y in ((0, 0), (size - corner, 0), (0, size - corner), (size - corner, size - corner)):
        draw.rectangle([x, y, x + corner - 1, y + corner - 1], fill=200)
    return height.filter(ImageFilter.GaussianBlur(1.5))


def normal_map(height, strength=2.0):
    """A tangent-space normal map from a height map, glTF convention: +X right, +Y towards the top of the
    image, +Z out of the surface."""
    width, rows = height.size
    values = list(height.tobytes())
    pixels = []
    for row in range(rows):
        for column in range(width):
            def at(x, y):
                return values[min(max(y, 0), rows - 1) * width + min(max(x, 0), width - 1)] / 255.0
            dx = at(column + 1, row) - at(column - 1, row)
            # Rows grow downwards: up the image is -row.
            dy = at(column, row + 1) - at(column, row - 1)
            normal = normalize((-dx * strength, dy * strength, 1.0))
            pixels.append(tuple(int(round((c * 0.5 + 0.5) * 255.0)) for c in normal))
    image = Image.new("RGB", (width, rows))
    image.putdata(pixels)
    return image


def crate():
    """A wooden crate with metal corners: external .bin, JPG color and PNG metallic-roughness."""
    size = 512
    color = Image.new("RGB", (size, size), (120, 72, 36))
    metal_rough = Image.new("RGB", (size, size), (0, 180, 0))
    draw_color, draw_mr = ImageDraw.Draw(color), ImageDraw.Draw(metal_rough)
    for plank in range(6):
        top = plank * size // 6
        shade = (128 + 9 * (plank % 3), 78 + 5 * (plank % 2), 38)
        draw_color.rectangle([0, top + 3, size - 1, top + size // 6 - 4], fill=shade)
        for grain in range(5):
            y = top + 12 + grain * 13
            draw_color.line([(20, y), (size - 20, y + (grain % 2) * 4)], fill=(100, 58, 28), width=2)
    frame = 40
    for rect in ([0, 0, size - 1, frame], [0, size - frame, size - 1, size - 1], [0, 0, frame, size - 1],
                 [size - frame, 0, size - 1, size - 1]):
        draw_color.rectangle(rect, fill=(92, 54, 26))
        draw_mr.rectangle(rect, fill=(0, 200, 0))
    corner = 72
    for x, y in ((0, 0), (size - corner, 0), (0, size - corner), (size - corner, size - corner)):
        draw_color.rectangle([x, y, x + corner - 1, y + corner - 1], fill=(150, 150, 155))
        # Metallic in blue, roughness in green (glTF).
        draw_mr.rectangle([x, y, x + corner - 1, y + corner - 1], fill=(0, 90, 255))

    color.save(MODELS / "crate_color.jpg", "JPEG", quality=90)
    metal_rough.save(MODELS / "crate_metal_rough.png", "PNG", optimize=True)
    normal_map(crate_height(size)).save(MODELS / "crate_normal.png", "PNG", optimize=True)

    builder = GltfBuilder()
    material = builder.material("Crate", metallic=1.0, roughness=1.0,
                                color_texture=builder.texture_from_uri("crate_color.jpg"),
                                metal_rough_texture=builder.texture_from_uri("crate_metal_rough.png"),
                                normal_texture=builder.texture_from_uri("crate_normal.png"))
    positions, normals, uvs, indices = cube_geometry(0.5, lambda _: (0.0, 0.0, 1.0, 1.0))
    mesh = builder.mesh("Crate", [builder.primitive(positions, normals, uvs, indices, material=material)])
    # Centered on its origin, as its box collider (decision 0019). The mesh is half size and its node doubles
    # it, so that the demo exercises a node transform (flattened at import).
    body = builder.node(root=False, name="CrateBody", mesh=mesh, scale=[2.0, 2.0, 2.0])
    builder.node(name="Crate", children=[body])
    builder.write_gltf(MODELS / "crate.gltf", binary_uri="crate.bin")


# --- Importer test files ---------------------------------------------------------------------------

TRIANGLE = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
TRIANGLE_NORMALS = [(0.0, 0.0, 1.0)] * 3
TRIANGLE_UVS = [(0.0, 1.0), (1.0, 1.0), (0.0, 0.0)]


def write_test_gltf(name, build):
    builder = GltfBuilder()
    build(builder)
    builder.write_gltf(TEST_DATA / "Gltf" / name)


def node_transforms(builder):
    # Index type: unsigned byte. World of the child: translate(10, 0, 0) * rotateY(90) * scale(2).
    mesh = builder.mesh("Triangle", [builder.primitive(TRIANGLE, TRIANGLE_NORMALS, TRIANGLE_UVS, [0, 1, 2],
                                                       UNSIGNED_BYTE)])
    half = math.sqrt(0.5)
    child = builder.node(root=False, name="Child", mesh=mesh, rotation=[0.0, half, 0.0, half], scale=[2.0, 2.0, 2.0])
    builder.node(name="Parent", children=[child], translation=[10.0, 0.0, 0.0])


def mirrored(builder):
    mesh = builder.mesh("Triangle", [builder.primitive(TRIANGLE, TRIANGLE_NORMALS, TRIANGLE_UVS, [0, 1, 2])])
    builder.node(name="Mirror", mesh=mesh, scale=[-1.0, 1.0, 1.0])


def without_normals(builder):
    # Unsigned int indices, no NORMAL, no TEXCOORD_0.
    mesh = builder.mesh("Triangle", [builder.primitive(TRIANGLE, indices=[0, 1, 2], index_type=UNSIGNED_INT)])
    builder.node(name="Triangle", mesh=mesh)


def non_indexed(builder):
    mesh = builder.mesh("Triangle", [builder.primitive(TRIANGLE, TRIANGLE_NORMALS)])
    builder.node(name="Triangle", mesh=mesh)


def two_materials(builder):
    red = builder.material("Red", color=(1.0, 0.0, 0.0, 1.0), metallic=0.0, roughness=0.25)
    gold = builder.material("Gold", color=(1.0, 0.8, 0.3, 1.0), metallic=1.0, roughness=0.5)
    first = builder.primitive(TRIANGLE, TRIANGLE_NORMALS, indices=[0, 1, 2], material=red)
    second = builder.primitive(TRIANGLE, TRIANGLE_NORMALS, indices=[0, 1, 2], material=gold)
    mesh = builder.mesh("TwoMaterials", [first, second])
    builder.node(name="A", mesh=mesh)
    # The same mesh again, higher up: its red triangle merges into the first part.
    builder.node(name="B", mesh=mesh, translation=[0.0, 5.0, 0.0])


def textured(builder):
    embedded = Image.new("RGB", (2, 2), (0, 128, 255))
    material = builder.material(
        "Textured", color=(0.5, 0.5, 0.5, 1.0), metallic=0.25, roughness=0.75,
        color_texture=builder.texture_from_uri("../Images/rgba_2x2.png"),
        metal_rough_texture=builder.texture_from_bytes(image_bytes(embedded, "PNG"), "image/png"),
        normal_texture=builder.texture_from_bytes(image_bytes(Image.new("RGB", (2, 2), (128, 128, 255)), "PNG"),
                                                  "image/png"),
        normal_scale=0.5)
    mesh = builder.mesh("Textured", [builder.primitive(TRIANGLE, TRIANGLE_NORMALS, TRIANGLE_UVS, [0, 1, 2],
                                                       material=material)])
    builder.node(name="Textured", mesh=mesh)


def missing_texture(builder):
    material = builder.material("Missing", color_texture=builder.texture_from_uri("absent.png"))
    mesh = builder.mesh("Triangle", [builder.primitive(TRIANGLE, TRIANGLE_NORMALS, TRIANGLE_UVS, [0, 1, 2],
                                                       material=material)])
    builder.node(name="Triangle", mesh=mesh)


def required_extension(builder):
    builder.doc["extensionsUsed"] = ["KHR_draco_mesh_compression"]
    builder.doc["extensionsRequired"] = ["KHR_draco_mesh_compression"]
    non_indexed(builder)


def test_data():
    (TEST_DATA / "Gltf").mkdir(parents=True, exist_ok=True)
    (TEST_DATA / "Images").mkdir(parents=True, exist_ok=True)

    pixels = Image.new("RGBA", (2, 2))
    pixels.putdata([(255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 255), (255, 255, 255, 0)])
    pixels.save(TEST_DATA / "Images" / "rgba_2x2.png", "PNG")
    Image.new("RGB", (8, 8), (128, 128, 128)).save(TEST_DATA / "Images" / "gray_8x8.jpg", "JPEG", quality=95)
    valid = (TEST_DATA / "Images" / "rgba_2x2.png").read_bytes()
    (TEST_DATA / "Images" / "corrupt.png").write_bytes(valid[: len(valid) // 2])

    for name, build in (("node_transforms.gltf", node_transforms), ("mirrored.gltf", mirrored),
                        ("without_normals.gltf", without_normals), ("non_indexed.gltf", non_indexed),
                        ("two_materials.gltf", two_materials), ("textured.gltf", textured),
                        ("missing_texture.gltf", missing_texture), ("required_extension.gltf", required_extension)):
        write_test_gltf(name, build)

    cube = (MODELS / "reference_cube.glb").read_bytes()
    (TEST_DATA / "Gltf" / "truncated.glb").write_bytes(cube[: len(cube) // 2])


# The demo's sun (Assets/Scenes/demo.json): the way to it. Its disk is not in the sky: the directional light
# already lights the scene (decision 0022).
TO_SUN = (0.4, 1.0, 0.35)
# Colored panels on the horizon, to read reflections: azimuth in degrees from -Z towards +X, linear color.
PANELS = ((0.0, (0.90, 0.12, 0.08)), (60.0, (0.12, 0.75, 0.18)), (120.0, (0.10, 0.25, 0.95)),
          (180.0, (0.95, 0.80, 0.15)), (240.0, (0.90, 0.90, 0.90)), (300.0, (0.65, 0.15, 0.85)))
PANEL_HALF_WIDTH = math.radians(9.0)
PANEL_TOP = math.radians(11.0)
SUN_DIRECTION = normalize(TO_SUN)


def sky_radiance(direction):
    """Linear RGB radiance seen along a unit direction."""
    x, y, z = direction
    if y < 0.0:
        # Dark ground, a little lighter towards the horizon.
        haze = math.exp(y * 12.0)
        return tuple(dark + (light - dark) * haze for dark, light in zip((0.07, 0.065, 0.06), (0.30, 0.30, 0.30)))
    elevation = math.asin(min(y, 1.0))
    azimuth = math.atan2(x, -z)
    if elevation < PANEL_TOP:
        for center, color in PANELS:
            offset = (azimuth - math.radians(center) + math.pi) % (2.0 * math.pi) - math.pi
            if abs(offset) < PANEL_HALF_WIDTH:
                return color
    t = math.sqrt(y)
    sky = [h + (zenith - h) * t for h, zenith in zip((0.80, 0.86, 0.95), (0.22, 0.38, 0.80))]
    sun = SUN_DIRECTION
    angle = math.acos(max(-1.0, min(1.0, x * sun[0] + y * sun[1] + z * sun[2])))
    halo = 4.0 * math.exp(-(angle / 0.08) ** 2) + 0.5 * math.exp(-(angle / 0.4) ** 2)
    return tuple(c + halo * s for c, s in zip(sky, (1.0, 0.92, 0.80)))


def rgbe(color):
    """Radiance's shared-exponent encoding of one linear RGB value."""
    largest = max(color)
    if largest < 1e-32:
        return (0, 0, 0, 0)
    mantissa, exponent = math.frexp(largest)
    scale = mantissa * 256.0 / largest
    return tuple(min(255, int(c * scale)) for c in color) + (exponent + 128,)


def rle_channel(values):
    """One channel of a scanline in Radiance's run-length encoding: runs of 3 to 127, literals of up to 128."""
    out = bytearray()
    i = 0
    while i < len(values):
        run = 1
        while i + run < len(values) and run < 127 and values[i + run] == values[i]:
            run += 1
        if run >= 3:
            out += bytes((128 + run, values[i]))
            i += run
            continue
        start = i
        i += 1
        while i < len(values) and i - start < 128:
            if i + 2 < len(values) and values[i] == values[i + 1] == values[i + 2]:
                break
            i += 1
        out.append(i - start)
        out += bytes(values[start:i])
    return out


def write_hdr(path, width, height, radiance):
    """An equirectangular Radiance HDR image: rows from +Y down, the middle column towards -Z
    (Engine/Resources/EnvironmentData.h)."""
    data = bytearray(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n" + f"-Y {height} +X {width}\n".encode("ascii"))
    for row in range(height):
        polar = (row + 0.5) / height * math.pi
        pixels = []
        for column in range(width):
            azimuth = ((column + 0.5) / width - 0.5) * 2.0 * math.pi
            direction = (math.sin(polar) * math.sin(azimuth), math.cos(polar), -math.sin(polar) * math.cos(azimuth))
            pixels.append(rgbe(radiance(direction)))
        data += bytes((2, 2, width >> 8, width & 0xFF))
        for channel in range(4):
            data += rle_channel([pixel[channel] for pixel in pixels])
    path.write_bytes(bytes(data))


def environment():
    ENVIRONMENTS.mkdir(parents=True, exist_ok=True)
    write_hdr(ENVIRONMENTS / "sky.hdr", 1024, 512, sky_radiance)


def main():
    MODELS.mkdir(parents=True, exist_ok=True)
    reference_cube()
    crate()
    primitives()
    character()
    environment()
    test_data()
    print("Assets written to", MODELS, ENVIRONMENTS, "and", TEST_DATA)


if __name__ == "__main__":
    main()
