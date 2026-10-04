# -*- coding: utf-8 -*-
"""
Crea los assets de Unreal que el codigo C++ espera (se ejecuta DENTRO del editor):

  "C:\\Program Files\\Epic Games\\UE_5.5\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" ^
      C:\\Discos\\Proyectos\\NEXCODE\\gateway\\Gateway.uproject -run=pythonscript ^
      -script=C:\\Discos\\Proyectos\\NEXCODE\\gateway\\Tools\\setup_assets.py -unattended -nopause

  - /Game/Gateway/Materials/M_Dome : material unlit del domo (nodo Custom -> Shaders/GatewayDome.ush)
  - /Game/Gateway/Materials/M_Post : material de post-proceso (nodo Custom -> Shaders/GatewayPost.ush)
  - /Game/Gateway/Images/T_*       : texturas importadas desde Content/Gateway/Images/src/*.png|jpg
  - /Game/Gateway/Maps/Gateway     : mapa vacio con PlayerStart
"""
import os
import unreal

ROOT = "/Game/Gateway"
MAT_DIR = ROOT + "/Materials"
IMG_DIR = ROOT + "/Images"
MAP_DIR = ROOT + "/Maps"
PROJECT_DIR = unreal.Paths.project_dir()
SRC_IMG_DIR = os.path.join(unreal.Paths.project_content_dir(), "Gateway", "Images", "src")

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("[gateway-setup] " + str(msg))


def ensure_dir(path):
    if not EAL.does_directory_exist(path):
        EAL.make_directory(path)


def new_material(path):
    folder, name = path.rsplit("/", 1)
    ensure_dir(folder)
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mat = AT.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    return mat


def scalar(mat, name, default, x, y):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", float(default))
    return n


def vector(mat, name, default, x, y):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(*default))
    return n


def texture_param(mat, name, tex, x, y):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, x, y)
    n.set_editor_property("parameter_name", name)
    if tex:
        n.set_editor_property("texture", tex)
    return n


def custom_node(mat, code, inputs, includes, x, y, desc):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    n.set_editor_property("code", code)
    n.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    n.set_editor_property("description", desc)
    ins = []
    for name in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    n.set_editor_property("inputs", ins)
    n.set_editor_property("include_file_paths", includes)
    return n


def connect(src, dst, dst_input, src_output=""):
    ok = MEL.connect_material_expressions(src, src_output, dst, dst_input)
    if not ok:
        log("FALLO al conectar -> %s" % dst_input)


# ---------------------------------------------------------------- texturas

def import_images():
    ensure_dir(IMG_DIR)
    textures = {}
    if not os.path.isdir(SRC_IMG_DIR):
        log("sin carpeta de imagenes: " + SRC_IMG_DIR)
        return textures
    tasks = []
    for f in sorted(os.listdir(SRC_IMG_DIR)):
        if not f.lower().endswith((".png", ".jpg", ".jpeg", ".exr", ".hdr")):
            continue
        base = os.path.splitext(f)[0]
        name = base if base.startswith("T_") else "T_" + base
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(SRC_IMG_DIR, f))
        t.set_editor_property("destination_path", IMG_DIR)
        t.set_editor_property("destination_name", name)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("automated", True)
        t.set_editor_property("save", True)
        tasks.append(t)
    if tasks:
        AT.import_asset_tasks(tasks)
    for t in tasks:
        for p in t.get_editor_property("imported_object_paths"):
            tex = unreal.load_asset(p)
            if tex:
                # Las imagenes son "arte": sin mipmaps raros ni compresion que las ensucie
                try:
                    tex.set_editor_property("srgb", True)
                    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
                    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
                    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
                    EAL.save_loaded_asset(tex)
                except Exception as e:
                    log("textura %s: %s" % (p, e))
                textures[tex.get_name()] = tex
                log("textura " + p)
    return textures


# ---------------------------------------------------------------- modelos (FBX de Blender)

def import_models():
    src = os.path.join(unreal.Paths.project_content_dir(), "Gateway", "Models")
    dst = ROOT + "/Models"
    ensure_dir(dst)
    if not os.path.isdir(src):
        return
    tasks = []
    for f in sorted(os.listdir(src)):
        if not f.lower().endswith(".fbx"):
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(src, f))
        t.set_editor_property("destination_path", dst)
        t.set_editor_property("destination_name", os.path.splitext(f)[0])
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("automated", True)
        t.set_editor_property("save", True)
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", False)
        ui.set_editor_property("import_materials", True)
        ui.set_editor_property("import_textures", True)
        ui.set_editor_property("import_animations", False)
        ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
        ui.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", False)
        ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
        t.set_editor_property("options", ui)
        tasks.append(t)
    if tasks:
        AT.import_asset_tasks(tasks)
        for t in tasks:
            for p in t.get_editor_property("imported_object_paths"):
                log("modelo " + p)


# ---------------------------------------------------------------- M_Dome

DOME_CODE = """return GatewayDome(-Dir, Fwd, ModeA, ModeB, Mix, Color, Intensity, Speed, Complexity, Hue, Breath, SceneTime,
    Guide, ImageA, ImageASampler, ImageB, ImageBSampler);"""


def make_dome(textures):
    mat = new_material(MAT_DIR + "/M_Dome")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("use_material_attributes", False)

    cam = MEL.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -900, -400)
    fwd = vector(mat, "ViewForward", (1, 0, 0, 0), -900, -300)
    mode_a = scalar(mat, "ModeA", 4, -900, -200)
    mode_b = scalar(mat, "ModeB", 4, -900, -140)
    mix = scalar(mat, "Mix", 0, -900, -80)
    color = vector(mat, "Color", (0.3, 0.4, 0.9, 1), -900, -20)
    inten = scalar(mat, "Intensity", 0.5, -900, 60)
    speed = scalar(mat, "Speed", 0.3, -900, 120)
    cplx = scalar(mat, "Complexity", 0.5, -900, 180)
    hue = scalar(mat, "Hue", 0, -900, 240)
    breath = scalar(mat, "Breath", 0.5, -900, 300)
    stime = scalar(mat, "SceneTime", 0, -900, 360)
    guide = scalar(mat, "Guide", 0, -900, 400)
    default_tex = textures.get("T_Nebula01") or unreal.load_asset("/Engine/EngineResources/DefaultTexture")
    img_a = texture_param(mat, "ImageA", default_tex, -900, 440)
    img_b = texture_param(mat, "ImageB", default_tex, -900, 560)

    inputs = ["Dir", "Fwd", "ModeA", "ModeB", "Mix", "Color", "Intensity", "Speed", "Complexity", "Hue", "Breath", "SceneTime", "Guide", "ImageA", "ImageB"]
    cu = custom_node(mat, DOME_CODE, inputs, ["/Gateway/GatewayDome.ush"], -350, 0, "Gateway dome (Shaders/GatewayDome.ush)")
    for node, name in [(cam, "Dir"), (fwd, "Fwd"), (mode_a, "ModeA"), (mode_b, "ModeB"), (mix, "Mix"), (color, "Color"), (inten, "Intensity"),
                       (speed, "Speed"), (cplx, "Complexity"), (hue, "Hue"), (breath, "Breath"), (stime, "SceneTime"), (guide, "Guide"), (img_a, "ImageA"), (img_b, "ImageB")]:
        connect(node, cu, name)
    MEL.connect_material_property(cu, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    log("M_Dome creado")
    return mat


# ---------------------------------------------------------------- M_Post

POST_CODE = """float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float2 q = uv - 0.5;
float ab = Chroma * dot(q, q);
float r = SceneTextureLookup(uv + q * ab, 14, false).r;
float g = SceneTextureLookup(uv, 14, false).g;
float b = SceneTextureLookup(uv - q * ab, 14, false).b;
return GatewayPost(uv, float3(r, g, b), FlickerHz, FlickerDepth, FlickerShape, FlickerColor, Fade, SceneTime, Breath);"""


def make_post():
    mat = new_material(MAT_DIR + "/M_Post")
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    try:
        mat.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    except Exception as e:
        log("blendable_location: %s" % e)
        try:
            mat.set_editor_property("blendable_location", unreal.BlendableLocation.BL_AFTER_TONEMAPPING)
        except Exception as e2:
            log("blendable_location (2): %s" % e2)

    scene = MEL.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -900, -300)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    hz = scalar(mat, "FlickerHz", 0, -900, -150)
    depth = scalar(mat, "FlickerDepth", 0, -900, -90)
    shape = scalar(mat, "FlickerShape", 0, -900, -30)
    fcol = vector(mat, "FlickerColor", (1, 1, 1, 1), -900, 30)
    fade = scalar(mat, "Fade", 0, -900, 110)
    stime = scalar(mat, "SceneTime", 0, -900, 170)
    breath = scalar(mat, "Breath", 0.5, -900, 230)
    chroma = scalar(mat, "Chroma", 0.015, -900, 290)

    inputs = ["SceneIn", "FlickerHz", "FlickerDepth", "FlickerShape", "FlickerColor", "Fade", "SceneTime", "Breath", "Chroma"]
    cu = custom_node(mat, POST_CODE, inputs, ["/Gateway/GatewayPost.ush"], -350, 0, "Gateway post (Shaders/GatewayPost.ush)")
    connect(scene, cu, "SceneIn", "Color")
    for node, name in [(hz, "FlickerHz"), (depth, "FlickerDepth"), (shape, "FlickerShape"), (fcol, "FlickerColor"), (fade, "Fade"), (stime, "SceneTime"), (breath, "Breath"), (chroma, "Chroma")]:
        connect(node, cu, name)
    MEL.connect_material_property(cu, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    log("M_Post creado")
    return mat


# ---------------------------------------------------------------- M_Panel (menu VR)

def make_panel():
    mat = new_material(MAT_DIR + "/M_Panel")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)
    color = vector(mat, "Color", (0.02, 0.02, 0.05, 1), -500, -100)
    opac = scalar(mat, "Opacity", 0.75, -500, 100)
    MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(opac, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    log("M_Panel creado")


# ---------------------------------------------------------------- M_Star (particulas 3D)

def make_star():
    mat = new_material(MAT_DIR + "/M_Star")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    color = vector(mat, "Color", (0.6, 0.5, 1.0, 1), -700, -100)
    rnd = MEL.create_material_expression(mat, unreal.MaterialExpressionPerInstanceRandom, -700, 100)
    add = MEL.create_material_expression(mat, unreal.MaterialExpressionAdd, -500, 100)
    add.set_editor_property("const_a", 0.4)
    connect(rnd, add, "B")
    mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 0)
    connect(color, mul, "A")
    connect(add, mul, "B")
    # borde suave: fresnel invertido para que la esfera parezca un punto de luz
    fres = MEL.create_material_expression(mat, unreal.MaterialExpressionFresnel, -500, 250)
    fres.set_editor_property("exponent", 2.0)
    one_minus = MEL.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -350, 250)
    connect(fres, one_minus, "")
    mul2 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 100)
    connect(mul, mul2, "A")
    connect(one_minus, mul2, "B")
    MEL.connect_material_property(mul2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    log("M_Star creado")


# ---------------------------------------------------------------- mapa

def make_map():
    ensure_dir(MAP_DIR)
    path = MAP_DIR + "/Gateway"
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if EAL.does_asset_exist(path):
        log("mapa ya existe, se conserva: " + path)
        return
    if not les.new_level(path):
        log("no se pudo crear el mapa")
        return
    eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 0))
    # El director y el escenario los crea el GameMode en BeginPlay; el mapa solo necesita un inicio.
    les.save_current_level()
    log("mapa creado: " + path)


def main():
    ensure_dir(ROOT)
    textures = import_images()
    import_models()
    make_dome(textures)
    make_post()
    make_panel()
    make_star()
    make_map()
    EAL.save_directory(ROOT, only_if_is_dirty=False, recursive=True)
    log("listo")


main()
