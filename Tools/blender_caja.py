# -*- coding: utf-8 -*-
"""
Genera la Caja de Conversion de Energia (cofre con tapa pesada, herrajes y ranura luminosa)
y la exporta a Content/Gateway/Models/SM_Caja.fbx. Correr con Blender headless:

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b --python Tools/blender_caja.py

Unidades: 1 unidad Blender = 1 m; la caja mide 1.2 x 0.7 x 0.7 m. El FBX se exporta en cm.
"""
import os
import bpy
import bmesh
from math import radians

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "Content", "Gateway", "Models")
os.makedirs(OUT_DIR, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene


def mat(name, color, metallic=0.0, rough=0.5, emission=None, strength=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = rough
    if emission:
        bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
        bsdf.inputs["Emission Strength"].default_value = strength
    return m


M_WOOD = mat("Caja_Madera", (0.06, 0.035, 0.025), 0.0, 0.55)
M_METAL = mat("Caja_Herraje", (0.85, 0.62, 0.22), 1.0, 0.35)
M_GLOW = mat("Caja_Luz", (1.0, 0.75, 0.35), 0.0, 0.4, (1.0, 0.7, 0.3), 12.0)


def box(name, size, loc, material, bevel=0.012):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    o = bpy.context.active_object
    o.name = name
    o.scale = (size[0], size[1], size[2])
    bpy.ops.object.transform_apply(scale=True)
    if bevel > 0:
        b = o.modifiers.new("Bevel", "BEVEL")
        b.width = bevel
        b.segments = 3
    o.data.materials.append(material)
    return o


W, D, H = 1.2, 0.7, 0.45   # cuerpo
LID_H = 0.22

body = box("Cuerpo", (W, D, H), (0, 0, H / 2), M_WOOD)
# tapa: bloque con bisel grande (abovedada), levantada 3 cm para que se vea la luz de dentro
GAP = 0.03
lid = box("Tapa", (W, D, LID_H), (0, 0, H + GAP + LID_H / 2), M_WOOD, bevel=0.09)

# herrajes: bandas metalicas verticales y esquinas
for x in (-W * 0.36, 0.0, W * 0.36):
    box("Banda_%d" % int(x * 100), (0.06, D + 0.02, H + 0.01), (x, 0, H / 2), M_METAL, 0.004)
    box("BandaTapa_%d" % int(x * 100), (0.06, D + 0.02, LID_H * 0.45), (x, 0, H + GAP + LID_H * 0.3), M_METAL, 0.004)
for sx in (-1, 1):
    for sy in (-1, 1):
        box("Esquina_%d_%d" % (sx, sy), (0.08, 0.08, H + 0.01), (sx * (W / 2 - 0.03), sy * (D / 2 - 0.03), H / 2), M_METAL, 0.004)

# cerradura
box("Cerradura", (0.14, 0.03, 0.16), (0, -D / 2 - 0.01, H - 0.05), M_METAL, 0.004)
# ranura luminosa entre cuerpo y tapa (lo que hay dentro brilla)
box("Luz", (W - 0.04, D - 0.04, GAP), (0, 0, H + GAP / 2), M_GLOW, 0.0)

# unir todo en un solo objeto
bpy.ops.object.select_all(action="SELECT")
bpy.context.view_layer.objects.active = body
bpy.ops.object.join()
body.name = "SM_Caja"
# suavizado
bpy.ops.object.shade_smooth()
try:
    bpy.ops.object.shade_auto_smooth(angle=radians(35))
except Exception:
    pass

fbx = os.path.join(OUT_DIR, "SM_Caja.fbx")
bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, apply_unit_scale=True, global_scale=1.0, apply_scale_options="FBX_SCALE_ALL",
                         axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", use_mesh_modifiers=True, path_mode="COPY", embed_textures=False, bake_space_transform=True)
print("FBX exportado:", fbx)

# Render de referencia (imagen T_Caja01 para el modo 'caja' del domo)
for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
    try:
        scene.render.engine = eng
        break
    except TypeError:
        continue
scene.render.resolution_x = 1024
scene.render.resolution_y = 1024
scene.render.film_transparent = False
world = bpy.data.worlds.new("W")
scene.world = world
world.use_nodes = True
bg = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
bg.inputs[0].default_value = (0.0, 0.0, 0.0, 1.0)
bpy.ops.object.light_add(type="AREA", location=(1.5, -2.0, 2.5))
L = bpy.context.active_object
L.data.energy = 600
L.data.color = (1.0, 0.85, 0.7)
L.data.size = 2.0
L.rotation_euler = (radians(50), 0, radians(35))
bpy.ops.object.camera_add(location=(1.9, -2.3, 1.3), rotation=(radians(70), 0, radians(40)))
cam = bpy.context.active_object
scene.camera = cam
if hasattr(scene.render.image_settings, "media_type"):
    scene.render.image_settings.media_type = "IMAGE"
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = os.path.join(ROOT, "Content", "Gateway", "Images", "src", "Caja01.png")
bpy.ops.render.render(write_still=True)
print("Render:", scene.render.filepath)
