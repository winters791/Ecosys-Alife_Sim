extends SubViewportContainer
## A little stage that draws creatures with the world's own sprite shader
## (blob.gdshader): the creature designer's canvas and its test tank.
##
## Feed it instance and part buffers laid out like LucaWorld's
## (get_instance_buffer / get_part_buffer / design_sprite). The camera looks
## straight down: world +x is screen right, world +z is screen down, so a body
## facing +x faces right and an angle theta on its outline is the screen angle.

const PX_SIZE := 0.125   # world units per sprite pixel, as in main.gd
const STRIDE := 20       # floats per sprite

var viewport: SubViewport
var cam: Camera3D
var multimesh: MultiMesh
var blob_mat: ShaderMaterial
var ground_mat: ShaderMaterial
var parts_tex: ImageTexture
var parts_width := 0
var parts_rows := 0
var zoom := 4.0                  # screen pixels per sprite pixel
var center := Vector2.ZERO       # the world point (x, z) at the middle of the stage


func _init() -> void:
	stretch = true
	viewport = SubViewport.new()
	viewport.own_world_3d = true
	viewport.transparent_bg = false
	viewport.render_target_update_mode = SubViewport.UPDATE_WHEN_VISIBLE
	add_child(viewport)

	cam = Camera3D.new()
	cam.projection = Camera3D.PROJECTION_ORTHOGONAL
	cam.near = 1.0
	cam.far = 400.0
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.05, 0.056, 0.075)
	cam.environment = env
	viewport.add_child(cam)

	# The ground: a pixel grid of two tones, so the scale is readable.
	var ground_shader := Shader.new()
	ground_shader.code = """
shader_type spatial;
render_mode unshaded;
uniform vec3 base = vec3(0.16, 0.19, 0.15);
uniform vec3 speck = vec3(0.13, 0.155, 0.12);
uniform vec3 line = vec3(0.19, 0.22, 0.18);
uniform float px = 0.125;
uniform float cell_lines = 0.0;   // 1 = draw the cell grid (one cell = 8 sprite pixels)
uniform vec4 wall = vec4(-1e9, -1e9, 1e9, 1e9);   // outside this (x0, z0, x1, z1) it is dark
varying vec3 world_pos;
void vertex() { world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; }
void fragment() {
	vec2 cell = floor(world_pos.xz / px);
	float h = fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453);
	vec3 c = h < 0.14 ? speck : base;
	vec2 g = mod(cell, 8.0);
	if (cell_lines > 0.5 && (g.x < 0.5 || g.y < 0.5)) c = line;
	if (world_pos.x < wall.x || world_pos.z < wall.y || world_pos.x > wall.z || world_pos.z > wall.w) c *= 0.35;
	ALBEDO = c;
}
"""
	ground_mat = ShaderMaterial.new()
	ground_mat.shader = ground_shader
	ground_mat.set_shader_parameter("px", PX_SIZE)
	var plane := PlaneMesh.new()
	plane.size = Vector2(600, 600)
	plane.material = ground_mat
	var ground := MeshInstance3D.new()
	ground.mesh = plane
	ground.position.y = -1.0
	viewport.add_child(ground)

	var quad := PlaneMesh.new()
	quad.size = Vector2(1, 1)
	blob_mat = ShaderMaterial.new()
	blob_mat.shader = load("res://blob.gdshader")
	blob_mat.set_shader_parameter("px_size", PX_SIZE)
	blob_mat.set_shader_parameter("extent", 128.0)
	quad.material = blob_mat
	multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = true
	multimesh.use_custom_data = true
	multimesh.mesh = quad
	multimesh.custom_aabb = AABB(Vector3(-300, -2, -300), Vector3(600, 6, 600))
	var node := MultiMeshInstance3D.new()
	node.multimesh = multimesh
	viewport.add_child(node)


func _ready() -> void:
	resized.connect(_place_camera)
	_place_camera()


## Look at `world_center` with `screen_px_per_sprite_px` screen pixels to a sprite pixel.
func look(world_center: Vector2, screen_px_per_sprite_px: float) -> void:
	center = world_center
	zoom = maxf(screen_px_per_sprite_px, 0.25)
	_place_camera()


func _place_camera() -> void:
	if cam == null:
		return
	var h := maxf(size.y, 1.0)
	cam.size = h * PX_SIZE / zoom
	cam.look_at_from_position(Vector3(center.x, 200, center.y), Vector3(center.x, 0, center.y), Vector3(0, 0, -1))


## Screen point (local to this control) -> world (x, z).
func to_world(local: Vector2) -> Vector2:
	return center + (local - size * 0.5) * PX_SIZE / zoom


## World (x, z) -> screen point local to this control.
func to_screen(world: Vector2) -> Vector2:
	return size * 0.5 + (world - center) * zoom / PX_SIZE


func set_ground(cell_lines: bool, wall := Rect2(-1e9, -1e9, 2e9, 2e9)) -> void:
	ground_mat.set_shader_parameter("cell_lines", 1.0 if cell_lines else 0.0)
	ground_mat.set_shader_parameter("wall", Vector4(wall.position.x, wall.position.y, wall.end.x, wall.end.y))


## Draw `count` sprites. `parts` holds PARTS_PER_CREATURE texels per sprite,
## laid out `width` texels to a row (one row is fine for a few sprites).
func show_sprites(instances: PackedFloat32Array, parts: PackedFloat32Array, count: int, width: int) -> void:
	count = mini(count, instances.size() / STRIDE)
	if count <= 0:
		multimesh.instance_count = 0
		return
	var rows := maxi(1, parts.size() / 4 / maxi(width, 1))
	var img := Image.create_from_data(width, rows, false, Image.FORMAT_RGBAF, parts.to_byte_array())
	if parts_tex == null or width != parts_width or rows != parts_rows:
		parts_tex = ImageTexture.create_from_image(img)
		parts_width = width
		parts_rows = rows
		blob_mat.set_shader_parameter("parts_tex", parts_tex)
		blob_mat.set_shader_parameter("parts_width", width)
	else:
		parts_tex.update(img)
	if multimesh.instance_count != instances.size() / STRIDE:
		multimesh.instance_count = instances.size() / STRIDE
	multimesh.visible_instance_count = count
	multimesh.buffer = instances
