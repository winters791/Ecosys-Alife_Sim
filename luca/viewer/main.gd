extends Node3D
## The Sandbox viewer: draws the sim and turns mouse input into god powers.
##
## Everything here is presentation. The world, the creatures and the rules
## live in the C++ core (LucaWorld); this script only asks it for arrays to
## draw and tells it which power the player cast.
##
## Controls
##   Left mouse           use the selected tool (inspect / paint)
##   Right or middle drag orbit the camera        (also Q / E)
##   W A S D / arrows     move                     (trackpad: two-finger scroll)
##   Mouse wheel / pinch  zoom
##   Space                pause / resume           1-5: speed
##   Tab                  next overlay             [ ]: brush size
##
## Command line (after `--`):  --size=128 --seed=42 --pop=200
##   --shot=PATH --frames=N   save a screenshot after N frames and quit
##   --demo                   cast a few powers by itself (used for testing)

const BLOB_SCALE := 0.45          # drawn radius per unit of core_radius
const FRAME_BUDGET_MS := 10.0     # sim time allowed per frame; beyond it the world just runs slower
const SPEED_TICKS := [0, 1, 4, 16, 100000]
const SPEED_NAMES := ["Pause", "1x", "4x", "16x", "Max"]
const OVERLAY_NAMES := ["Natural", "Temperature", "Light"]
const GRADUAL_TICKS := 1000       # how long a power takes in Gradual mode

enum Tool { LOOK, RAISE, LOWER, WARM, COOL }
const TOOL_NAMES := ["Look", "Raise land", "Lower land", "Warm spot", "Cold spot"]
const TOOL_COLORS := [Color(1, 1, 1), Color(1.0, 0.85, 0.4), Color(0.6, 0.75, 1.0), Color(1.0, 0.45, 0.3), Color(0.5, 0.9, 1.0)]

var world: LucaWorld
var world_size := 128
var extent := 128.0

# Scene
var terrain_mat: ShaderMaterial
var height_tex: ImageTexture
var temp_tex: ImageTexture
var light_tex: ImageTexture
var height_rev := -1
var climate_rev := -1
var multimesh: MultiMesh
var capacity := 0
var sun_light: DirectionalLight3D
var environment: Environment

# Camera (orbits a point on the ground)
var cam: Camera3D
var cam_target := Vector3.ZERO
var cam_distance := 110.0
var cam_yaw := 0.6
var cam_pitch := 0.9
var orbiting := false

# Player state
var tool: int = Tool.LOOK
var speed := 1
var overlay := 0
var brush_radius := 10.0
var brush_strength := 6.0
var gradual := false
var painting := false
var stroke_started := false
var paint_clock := 0.0
var hover := Vector3(0, -1e9, 0)   # where the mouse meets the ground
var inspected := Vector2(-1, -1)   # ground position of the inspected creature

# UI
var stats_label: Label
var inspector_label: Label
var chronicle_box: RichTextLabel
var chronicle_seen := 0
var speed_buttons: Array[Button] = []
var tool_buttons: Array[Button] = []
var overlay_button: OptionButton
var ticks_this_second := 0
var second_clock := 0.0
var ticks_per_second := 0

# Test hooks
var shot_path := ""
var shot_frames := 120
var demo := false
var frame := 0


func _ready() -> void:
	var seed_value := 42
	var population := 200
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--size="): world_size = int(arg.get_slice("=", 1))
		elif arg.begins_with("--seed="): seed_value = int(arg.get_slice("=", 1))
		elif arg.begins_with("--pop="): population = int(arg.get_slice("=", 1))
		elif arg.begins_with("--shot="): shot_path = arg.get_slice("=", 1)
		elif arg.begins_with("--frames="): shot_frames = int(arg.get_slice("=", 1))
		elif arg == "--demo": demo = true

	world = LucaWorld.new()
	world.setup(world_size, seed_value, population, true)
	world_size = world.get_world_size()
	extent = world.get_extent()
	cam_target = Vector3(extent * 0.5, 0.0, extent * 0.5)
	cam_distance = extent * 1.35

	_build_sky()
	_build_terrain()
	_build_blobs()
	_build_camera()
	_build_ui()
	_set_speed(1 if not demo else 3)
	_set_tool(Tool.LOOK)


# --- Scene construction --------------------------------------------------------

func _build_sky() -> void:
	sun_light = DirectionalLight3D.new()
	sun_light.rotation = Vector3(deg_to_rad(-52.0), deg_to_rad(35.0), 0.0)
	sun_light.shadow_enabled = true
	sun_light.directional_shadow_max_distance = extent * 2.5
	add_child(sun_light)

	var sky_mat := ProceduralSkyMaterial.new()
	sky_mat.sky_top_color = Color(0.22, 0.42, 0.72)
	sky_mat.sky_horizon_color = Color(0.72, 0.80, 0.88)
	sky_mat.ground_bottom_color = Color(0.20, 0.27, 0.36)
	sky_mat.ground_horizon_color = Color(0.72, 0.80, 0.88)
	var sky := Sky.new()
	sky.sky_material = sky_mat
	environment = Environment.new()
	environment.background_mode = Environment.BG_SKY
	environment.sky = sky
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	var world_env := WorldEnvironment.new()
	world_env.environment = environment
	add_child(world_env)


func _build_terrain() -> void:
	var plane := PlaneMesh.new()
	plane.size = Vector2(extent, extent)
	plane.center_offset = Vector3(extent * 0.5, 0.0, extent * 0.5)
	plane.subdivide_width = world_size - 1
	plane.subdivide_depth = world_size - 1
	# The shader moves vertices up, so tell the engine how tall the mesh can get.
	plane.custom_aabb = AABB(Vector3.ZERO, Vector3(extent, world.get_height_limit(), extent))

	terrain_mat = ShaderMaterial.new()
	terrain_mat.shader = load("res://terrain.gdshader")
	terrain_mat.set_shader_parameter("extent", extent)
	terrain_mat.set_shader_parameter("cells", float(world_size))
	plane.material = terrain_mat

	var terrain := MeshInstance3D.new()
	terrain.mesh = plane
	add_child(terrain)
	_upload_fields()


func _field_image(data: PackedFloat32Array) -> Image:
	return Image.create_from_data(world_size, world_size, false, Image.FORMAT_RF, data.to_byte_array())


## Copy the sim's fields into textures, but only the ones that changed.
func _upload_fields() -> void:
	if world.get_height_revision() != height_rev:
		height_rev = world.get_height_revision()
		var img := _field_image(world.get_heights())
		if height_tex == null:
			height_tex = ImageTexture.create_from_image(img)
			terrain_mat.set_shader_parameter("height_tex", height_tex)
		else:
			height_tex.update(img)
	if world.get_climate_revision() != climate_rev:
		climate_rev = world.get_climate_revision()
		var t_img := _field_image(world.get_temperatures())
		var l_img := _field_image(world.get_lights())
		if temp_tex == null:
			temp_tex = ImageTexture.create_from_image(t_img)
			light_tex = ImageTexture.create_from_image(l_img)
			terrain_mat.set_shader_parameter("temp_tex", temp_tex)
			terrain_mat.set_shader_parameter("light_tex", light_tex)
		else:
			temp_tex.update(t_img)
			light_tex.update(l_img)


func _build_blobs() -> void:
	var sphere := SphereMesh.new()
	sphere.radius = 1.0
	sphere.height = 2.0
	sphere.radial_segments = 12
	sphere.rings = 6
	var mat := ShaderMaterial.new()
	mat.shader = load("res://blob.gdshader")
	sphere.material = mat

	multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = true
	multimesh.use_custom_data = true
	multimesh.mesh = sphere
	# Instances are placed by the buffer, so the automatic bounds would be wrong.
	multimesh.custom_aabb = AABB(Vector3(-5, -5, -5), Vector3(extent + 10, world.get_height_limit() + 10, extent + 10))

	var node := MultiMeshInstance3D.new()
	node.multimesh = multimesh
	add_child(node)


func _build_camera() -> void:
	cam = Camera3D.new()
	cam.fov = 50.0
	cam.near = 0.5
	cam.far = extent * 12.0
	add_child(cam)
	_place_camera()


func _place_camera() -> void:
	cam_pitch = clampf(cam_pitch, 0.25, 1.5)
	cam_distance = clampf(cam_distance, 8.0, extent * 3.0)
	cam_target.x = clampf(cam_target.x, 0.0, extent)
	cam_target.z = clampf(cam_target.z, 0.0, extent)
	cam_target.y = world.height_at(cam_target.x, cam_target.z)
	var offset := Vector3(sin(cam_yaw) * cos(cam_pitch), sin(cam_pitch), cos(cam_yaw) * cos(cam_pitch))
	cam.position = cam_target + offset * cam_distance
	cam.look_at(cam_target, Vector3.UP)


# --- UI ------------------------------------------------------------------------

func _panel(anchor: int, min_size: Vector2) -> VBoxContainer:
	var panel := PanelContainer.new()
	panel.set_anchors_and_offsets_preset(anchor, Control.PRESET_MODE_MINSIZE, 12)
	panel.custom_minimum_size = min_size
	# Grow away from the screen edge the panel is pinned to.
	if anchor in [Control.PRESET_TOP_RIGHT, Control.PRESET_BOTTOM_RIGHT]:
		panel.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	if anchor == Control.PRESET_BOTTOM_RIGHT:
		panel.grow_vertical = Control.GROW_DIRECTION_BEGIN
	if anchor == Control.PRESET_CENTER_LEFT:
		panel.grow_vertical = Control.GROW_DIRECTION_BOTH
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.08, 0.09, 0.11, 0.82)
	style.set_corner_radius_all(8)
	style.set_content_margin_all(10)
	panel.add_theme_stylebox_override("panel", style)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 6)
	panel.add_child(box)
	$UI.add_child(panel)
	return box


func _heading(box: Container, text: String) -> void:
	var label := Label.new()
	label.text = text
	label.add_theme_color_override("font_color", Color(0.65, 0.72, 0.80))
	label.add_theme_font_size_override("font_size", 12)
	box.add_child(label)


func _button(box: Container, text: String, action: Callable, toggle := false) -> Button:
	var b := Button.new()
	b.text = text
	b.toggle_mode = toggle
	b.focus_mode = Control.FOCUS_NONE
	b.pressed.connect(action)
	box.add_child(b)
	return b


func _slider(box: Container, text: String, low: float, high: float, value: float, changed: Callable) -> HSlider:
	var label := Label.new()
	label.text = text
	box.add_child(label)
	var s := HSlider.new()
	s.min_value = low
	s.max_value = high
	s.step = 0.5
	s.value = value
	s.focus_mode = Control.FOCUS_NONE
	s.value_changed.connect(changed)
	box.add_child(s)
	return s


func _build_ui() -> void:
	var layer := CanvasLayer.new()
	layer.name = "UI"
	add_child(layer)

	# Top left: time and stats.
	var top := _panel(Control.PRESET_TOP_LEFT, Vector2(300, 0))
	stats_label = Label.new()
	top.add_child(stats_label)
	var speeds := HBoxContainer.new()
	top.add_child(speeds)
	for i in SPEED_NAMES.size():
		speed_buttons.append(_button(speeds, SPEED_NAMES[i], _set_speed.bind(i), true))

	# Left: god powers.
	var tools := _panel(Control.PRESET_CENTER_LEFT, Vector2(190, 0))
	_heading(tools, "TOOLS")
	for i in TOOL_NAMES.size():
		tool_buttons.append(_button(tools, TOOL_NAMES[i], _set_tool.bind(i), true))
	_slider(tools, "Brush size", 2.0, 40.0, brush_radius, func(v: float) -> void: brush_radius = v)
	_slider(tools, "Strength", 1.0, 20.0, brush_strength, func(v: float) -> void: brush_strength = v)

	_heading(tools, "WHOLE WORLD")
	var sun_row := HBoxContainer.new()
	tools.add_child(sun_row)
	_button(sun_row, "Dim sun", _cast_global.bind(LucaWorld.POWER_SUN, -0.1))
	_button(sun_row, "Brighten", _cast_global.bind(LucaWorld.POWER_SUN, 0.1))
	var climate_row := HBoxContainer.new()
	tools.add_child(climate_row)
	_button(climate_row, "Cool -3°", _cast_global.bind(LucaWorld.POWER_CLIMATE, -3.0))
	_button(climate_row, "Warm +3°", _cast_global.bind(LucaWorld.POWER_CLIMATE, 3.0))

	var gradual_box := CheckBox.new()
	gradual_box.text = "Gradual (%d ticks)" % GRADUAL_TICKS
	gradual_box.focus_mode = Control.FOCUS_NONE
	gradual_box.toggled.connect(func(on: bool) -> void: gradual = on)
	tools.add_child(gradual_box)

	_heading(tools, "VIEW")
	overlay_button = OptionButton.new()
	overlay_button.focus_mode = Control.FOCUS_NONE
	for n in OVERLAY_NAMES:
		overlay_button.add_item(n)
	overlay_button.item_selected.connect(_set_overlay)
	tools.add_child(overlay_button)
	_button(tools, "Save powers", _save_powers)

	# Right: the chronicle.
	var history := _panel(Control.PRESET_TOP_RIGHT, Vector2(380, 300))
	_heading(history, "CHRONICLE")
	chronicle_box = RichTextLabel.new()
	chronicle_box.scroll_following = true
	chronicle_box.size_flags_vertical = Control.SIZE_EXPAND_FILL
	chronicle_box.custom_minimum_size = Vector2(360, 260)
	chronicle_box.add_theme_font_size_override("normal_font_size", 13)
	history.add_child(chronicle_box)

	# Bottom right: the inspected creature.
	var inspector := _panel(Control.PRESET_BOTTOM_RIGHT, Vector2(280, 0))
	_heading(inspector, "CREATURE")
	inspector_label = Label.new()
	inspector_label.text = "Pick Look, then click a blob."
	inspector.add_child(inspector_label)


func _set_speed(index: int) -> void:
	speed = clampi(index, 0, SPEED_TICKS.size() - 1)
	for i in speed_buttons.size():
		speed_buttons[i].set_pressed_no_signal(i == speed)


func _set_tool(index: int) -> void:
	tool = index
	for i in tool_buttons.size():
		tool_buttons[i].set_pressed_no_signal(i == tool)


func _set_overlay(index: int) -> void:
	overlay = index % OVERLAY_NAMES.size()
	overlay_button.select(overlay)
	terrain_mat.set_shader_parameter("overlay", overlay)


func _save_powers() -> void:
	var path := ProjectSettings.globalize_path("user://powers.txt")
	if world.save_powers(path):
		chronicle_box.append_text("[color=#8fa]Saved every power cast to %s[/color]\n" % path)
	else:
		chronicle_box.append_text("[color=#f88]Could not write %s[/color]\n" % path)


# --- God powers ------------------------------------------------------------------

func _cast_global(power: int, amount: float) -> void:
	world.cast_power(power, 0.0, 0.0, 0.0, amount, GRADUAL_TICKS if gradual else 0, false)


## One dab of the current brush at the hover point. `seconds` scales an
## instant stroke so painting speed doesn't depend on frame rate.
func _dab(seconds: float) -> void:
	if hover.y < -1e8 or tool == Tool.LOOK:
		return
	var power := LucaWorld.POWER_TERRAIN if tool in [Tool.RAISE, Tool.LOWER] else LucaWorld.POWER_HEAT
	var direction := -1.0 if tool in [Tool.LOWER, Tool.COOL] else 1.0
	if gradual:
		# One slow change per click: the full strength, spread over many ticks.
		world.cast_power(power, hover.x, hover.z, brush_radius, direction * brush_strength * 2.0, GRADUAL_TICKS, false)
	else:
		# Only the first dab of a stroke is written to the chronicle.
		world.cast_power(power, hover.x, hover.z, brush_radius, direction * brush_strength * 2.0 * seconds, 0, stroke_started)
		stroke_started = true


func _inspect() -> void:
	if hover.y < -1e8:
		return
	var info: Dictionary = world.inspect(hover.x, hover.z, 3.0)
	if info.is_empty():
		inspected = Vector2(-1, -1)
		inspector_label.text = "Nothing lives here."
		return
	inspected = Vector2(info["x"], info["z"])
	_show_creature(info)


func _show_creature(info: Dictionary) -> void:
	var parts: PackedStringArray = info["parts"]
	inspector_label.text = "\n".join([
		"Lineage %d, generation %d" % [info["lineage"], info["generation"]],
		"Age %d ticks" % info["age"],
		"Energy %.2f  (divides at %.1f)" % [info["energy"], info["repro_threshold"]],
		"Radius %.2f   mass %.2f" % [info["radius"], info["mass"]],
		"Upkeep %.3f / tick" % info["upkeep"],
		"Light capture %.2f   speed %.2f" % [info["light_capture"], info["max_speed"]],
		"Comfort %.0f ± %.0f °C" % [info["thermal_optimum"], info["thermal_tolerance"]],
		"Here: light %.2f, %.1f °C" % [info["cell_light"], info["cell_temperature"]],
		"Parts: " + ", ".join(parts),
	])


# --- Input -----------------------------------------------------------------------

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT:
			if event.pressed:
				if tool == Tool.LOOK:
					_inspect()
				else:
					painting = not gradual
					stroke_started = false
					paint_clock = 0.0
					_dab(0.05)
			else:
				painting = false
		elif event.button_index == MOUSE_BUTTON_RIGHT or event.button_index == MOUSE_BUTTON_MIDDLE:
			orbiting = event.pressed
		elif event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			cam_distance *= 0.9
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			cam_distance *= 1.1
	elif event is InputEventMouseMotion and orbiting:
		cam_yaw -= event.relative.x * 0.006
		cam_pitch += event.relative.y * 0.006
	elif event is InputEventMagnifyGesture:
		cam_distance /= event.factor
	elif event is InputEventPanGesture:
		_pan(event.delta.x * 0.02, event.delta.y * 0.02)
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_SPACE: _set_speed(1 if speed == 0 else 0)
			KEY_1: _set_speed(0)
			KEY_2: _set_speed(1)
			KEY_3: _set_speed(2)
			KEY_4: _set_speed(3)
			KEY_5: _set_speed(4)
			KEY_TAB: _set_overlay(overlay + 1)
			KEY_BRACKETLEFT: brush_radius = maxf(2.0, brush_radius - 2.0)
			KEY_BRACKETRIGHT: brush_radius = minf(40.0, brush_radius + 2.0)


func _input(event: InputEvent) -> void:
	# Releases must always arrive, even if the mouse ended up over a panel.
	if event is InputEventMouseButton and not event.pressed:
		if event.button_index == MOUSE_BUTTON_LEFT:
			painting = false
		elif event.button_index == MOUSE_BUTTON_RIGHT or event.button_index == MOUSE_BUTTON_MIDDLE:
			orbiting = false


## Move the camera target across the ground, relative to where we're looking.
func _pan(right: float, forward: float) -> void:
	var f := Vector3(-sin(cam_yaw), 0.0, -cos(cam_yaw))
	var r := Vector3(cos(cam_yaw), 0.0, -sin(cam_yaw))
	cam_target += (r * right - f * forward) * cam_distance


# --- Every frame -------------------------------------------------------------------

func _process(delta: float) -> void:
	frame += 1
	if demo:
		_run_demo()

	# 1. Advance the sim. If the world is too crowded to keep up, it runs
	#    slower rather than dropping frames or capping anything.
	var ran := 0
	if SPEED_TICKS[speed] > 0:
		ran = world.step(SPEED_TICKS[speed], FRAME_BUDGET_MS)
	ticks_this_second += ran
	second_clock += delta
	if second_clock >= 1.0:
		ticks_per_second = roundi(ticks_this_second / second_clock)
		ticks_this_second = 0
		second_clock = 0.0

	# 2. Camera.
	var move := Input.get_vector("ui_left", "ui_right", "ui_down", "ui_up")
	if Input.is_key_pressed(KEY_A): move.x -= 1.0
	if Input.is_key_pressed(KEY_D): move.x += 1.0
	if Input.is_key_pressed(KEY_W): move.y += 1.0
	if Input.is_key_pressed(KEY_S): move.y -= 1.0
	if move != Vector2.ZERO:
		_pan(move.x * delta * 0.8, -move.y * delta * 0.8)
	if Input.is_key_pressed(KEY_Q): cam_yaw += delta * 1.5
	if Input.is_key_pressed(KEY_E): cam_yaw -= delta * 1.5
	_place_camera()

	# 3. Where is the mouse on the ground?
	var mouse := get_viewport().get_mouse_position()
	hover = world.raycast(cam.project_ray_origin(mouse), cam.project_ray_normal(mouse))

	# 4. Painting.
	if painting:
		paint_clock += delta
		if paint_clock >= 0.05:
			_dab(paint_clock)
			paint_clock = 0.0

	# 5. Draw.
	_upload_fields()
	_update_blobs()
	_update_sun()
	_update_brush_ring()
	_update_ui()

	if shot_path != "" and frame >= shot_frames:
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(shot_path)
		get_tree().quit()


func _update_blobs() -> void:
	var count := world.get_population()
	if count > capacity:
		# Grow in big steps so the buffer isn't reallocated every frame.
		capacity = maxi(1024, nearest_po2(count + 1))
		multimesh.instance_count = capacity
	multimesh.visible_instance_count = count
	if capacity > 0:
		multimesh.buffer = world.get_instance_buffer(capacity, BLOB_SCALE)


func _update_sun() -> void:
	var s := float(world.get_sun_scale())
	sun_light.light_energy = 0.1 + 0.9 * s
	environment.background_energy_multiplier = clampf(0.2 + 0.8 * s, 0.2, 1.6)
	environment.ambient_light_energy = clampf(0.2 + 0.45 * s, 0.2, 1.0)


func _update_brush_ring() -> void:
	var ring := Vector3(0, 0, -1)
	var color: Color = TOOL_COLORS[tool]
	if tool != Tool.LOOK and hover.y > -1e8:
		ring = Vector3(hover.x, hover.z, brush_radius)
	elif tool == Tool.LOOK and inspected.x >= 0.0:
		ring = Vector3(inspected.x, inspected.y, 1.2)
	terrain_mat.set_shader_parameter("brush", ring)
	terrain_mat.set_shader_parameter("brush_color", Vector3(color.r, color.g, color.b))


func _update_ui() -> void:
	stats_label.text = "Tick %s    Population %s\n%d ticks/s   %.2f ms/tick   %d fps\nSun %.2fx    Climate %+.1f °C" % [
		_commas(world.get_tick()), _commas(world.get_population()),
		ticks_per_second, world.get_tick_ms(), Engine.get_frames_per_second(),
		world.get_sun_scale(), world.get_climate_offset()]

	var size := world.get_chronicle_size()
	if size > chronicle_seen:
		for line in world.get_chronicle(chronicle_seen):
			chronicle_box.append_text(line + "\n")
		chronicle_seen = size

	if inspected.x >= 0.0 and frame % 10 == 0:
		var info: Dictionary = world.inspect(inspected.x, inspected.y, 0.05)
		if info.is_empty():
			inspected = Vector2(-1, -1)
			inspector_label.text = "It died."
		else:
			_show_creature(info)


func _commas(value: int) -> String:
	var s := str(value)
	var i := s.length() - 3
	while i > 0:
		s = s.insert(i, ",")
		i -= 3
	return s


## Scripted powers so a screenshot run can prove the whole pipeline works.
func _run_demo() -> void:
	var c := extent * 0.5
	match frame:
		20: world.cast_power(LucaWorld.POWER_TERRAIN, c, c, extent * 0.22, 45.0, 0, false)
		40: world.cast_power(LucaWorld.POWER_TERRAIN, c * 0.5, c * 1.3, extent * 0.12, -30.0, 0, false)
		60: world.cast_power(LucaWorld.POWER_CLIMATE, 0.0, 0.0, 0.0, -9.0, 400, false)
		80: world.cast_power(LucaWorld.POWER_HEAT, c * 1.5, c * 0.5, extent * 0.1, 15.0, 0, false)
