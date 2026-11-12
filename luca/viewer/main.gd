extends Node3D
## The Sandbox viewer: draws the sim as top-down pixel art and turns mouse
## input into god powers.
##
## Everything here is presentation. The world, the creatures and the rules
## live in the C++ core (LucaWorld); this script only asks it for arrays to
## draw and tells it which power the player cast.
##
## Controls
##   Left mouse           use the selected tool (inspect / paint)
##   Right or middle drag move the map             (also W A S D / arrows / two-finger scroll)
##   Mouse wheel / pinch  zoom                     (also - and =)
##   Space                pause / resume           1-5: speed
##   Tab                  next overlay             [ ]: brush size
##   C                    colour creatures by species      T: tree of life
##   F                    follow the inspected creature's species (alerts in the chronicle)
##   P                    the inspected creature's species, in the tree and its card
##   B                    the creature designer (its own screen; V opens the seed vault)
##   Esc                  stop dropping a creature
##
## Command line (after `--`):  --size=128 --seed=42 --pop=600
##   --shot=PATH --frames=N   save a screenshot after N frames and quit
##   --demo                   cast a few powers by itself (used for testing)
##   --zoom=K --at=X,Z        start zoomed in on a spot (screen pixels per sprite pixel)
##   --tree  or --tree=NAME   start with the tree of life open (zoomed in on a species)
##   --test-poison            draw every body as poisonous (to check the art)
##   --inspect=x,z            open the inspector on the creature nearest that spot
##   --follow                 also follow that creature's species
##   --parade                 an empty world with one hand-made creature of each body plan
##   --species                start with creatures coloured by species
##   --warmup=N               run N ticks before the first frame (skip ahead to evolved life)
##   --events=F               how often the world acts on its own (default 1, 0 = never)
##   --designer[=TAB]         start in the creature designer (0 body .. 4 test tank)
##   --template=NAME          ... on that template (Luca, Swimmer, Hunter, ...)
##   --vault                  ... with the seed vault open
##   --bank                   bank the most numerous species in the seed vault at the start

const PX_SIZE := 0.125            # world units per sprite pixel: 8 pixels to a cell
const ZOOMS := [0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0]  # screen pixels per sprite pixel
const FRAME_BUDGET_MS := 10.0     # sim time allowed per frame; beyond it the world just runs slower
const SPEED_TICKS := [0, 1, 4, 16, 100000]
const SPEED_NAMES := ["Pause", "1x", "4x", "16x", "Max"]
const OVERLAY_NAMES := ["Natural", "Temperature", "Light", "Nutrients", "The dead", "Waste", "Vents"]
const GRADUAL_TICKS := 1000       # how long a power takes in Gradual mode
const NIGHT_GROUND := Vector3(0.30, 0.34, 0.58)
const NIGHT_BLOBS := Vector3(0.58, 0.62, 0.82)

## Vent strength runs 0..3 (`vent_max`), against tens of height units for the land brush.
const VENT_BRUSH := 0.05

## DROP is not in the tool grid: the designer and the seed vault pick it, to
## drop a creature file where the player clicks.
enum Tool { LOOK, RAISE, LOWER, WARM, COOL, VENT, SEAL, DROP }
const TOOL_NAMES := ["Look", "Raise land", "Lower land", "Warm spot", "Cold spot", "Open vent", "Close vent"]
const TOOL_COLORS := [Color(1, 1, 1), Color(1.0, 0.85, 0.4), Color(0.6, 0.75, 1.0), Color(1.0, 0.45, 0.3), Color(0.5, 0.9, 1.0), Color(1.0, 0.55, 0.15), Color(0.55, 0.5, 0.6), Color(0.98, 0.62, 0.22)]

var world: LucaWorld
var world_size := 128
var extent := 128.0

# Scene
var terrain_mat: ShaderMaterial
var blob_mat: ShaderMaterial
var height_tex: ImageTexture
var temp_tex: ImageTexture
var light_tex: ImageTexture
var soil_tex: ImageTexture
var height_rev := -1
var climate_rev := -1
var multimesh: MultiMesh
var capacity := 0
var shown_daylight := 1.0

# Camera: straight down, no perspective. Zoom is a whole number of screen
# pixels per sprite pixel where it can be, so the art stays crisp.
var cam: Camera3D
var cam_target := Vector3.ZERO
var zoom_index := 2
var dragging := false

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
var inspected_id := -1             # the creature being followed
var inspected_at := Vector2(-1, -1)

# UI
var stats_label: Label
var left_scroll: ScrollContainer   # the left column, scrolled when it is taller than the window
var left_column: VBoxContainer
var species_box: RichTextLabel
var tree_panel: PanelContainer
var tree_view: Control
var tree_title: Label
var species_card: PanelContainer   # the species profile beside the tree (species_card.gd)
var by_species := false
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
var warmup := 0
var event_rate := 1.0                   # storms, eruptions, meteors, climate drift (core/events.hpp)
var start_zoom := -1.0
var start_at := Vector2(-1, -1)
var start_tree := false
var test_poison := false       # draw every body as poisonous, to check the art
var parade := false            # an empty world with one hand-made creature of each body plan
var parts_tex: ImageTexture    # every creature's parts, one texel each (see LucaWorld::get_part_buffer)
var parts_rows := 0
var start_tree_root := ""   # species name to zoom the tree into (testing)
var start_inspect := Vector2(-1, -1)   # open the inspector on the creature nearest here (testing)
var start_follow := false               # and follow its species
var inspected_species := -1             # species of the inspected creature
var follow_button: Button
var alerts_seen := 0
const ALERT_COLOR := "ffc857"
var start_vents: Array[Vector3] = []   # --vent=x,z,strength: open a vent before the warmup (testing)

# The creature designer (designer.gd) and dropping what it makes.
var designer: Control
var designer_speed := 1                 # the world's speed before the designer paused it
var drop_text := ""                     # the creature file being dropped
var drop_name := ""
var drop_count := 20
var drop_label: Label
var start_designer := -1                # --designer=TAB (testing)
var start_template := ""
var start_vault := false
var start_bank := false


func _ready() -> void:
	var seed_value := 42
	var population := 600
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--size="): world_size = int(arg.get_slice("=", 1))
		elif arg.begins_with("--seed="): seed_value = int(arg.get_slice("=", 1))
		elif arg.begins_with("--pop="): population = int(arg.get_slice("=", 1))
		elif arg.begins_with("--shot="): shot_path = arg.get_slice("=", 1)
		elif arg.begins_with("--frames="): shot_frames = int(arg.get_slice("=", 1))
		elif arg == "--demo": demo = true
		elif arg == "--species": by_species = true
		elif arg == "--follow": start_follow = true
		elif arg == "--test-poison": test_poison = true
		elif arg == "--parade":
			parade = true
			population = 0
		elif arg == "--tree": start_tree = true
		elif arg.begins_with("--tree="):
			start_tree = true
			start_tree_root = arg.get_slice("=", 1)
		elif arg.begins_with("--warmup="): warmup = int(arg.get_slice("=", 1))
		elif arg.begins_with("--events="): event_rate = float(arg.get_slice("=", 1))
		elif arg.begins_with("--zoom="): start_zoom = float(arg.get_slice("=", 1))
		elif arg.begins_with("--inspect="):
			var at := arg.get_slice("=", 1).split(",")
			if at.size() == 2: start_inspect = Vector2(float(at[0]), float(at[1]))
		elif arg.begins_with("--vent="):
			var v := arg.get_slice("=", 1).split(",")
			if v.size() >= 2: start_vents.append(Vector3(float(v[0]), float(v[1]), float(v[2]) if v.size() > 2 else 2.0))
		elif arg == "--designer": start_designer = 0
		elif arg.begins_with("--designer="): start_designer = int(arg.get_slice("=", 1))
		elif arg.begins_with("--template="): start_template = arg.get_slice("=", 1)
		elif arg == "--vault": start_vault = true
		elif arg == "--bank": start_bank = true
		elif arg.begins_with("--at="):
			var at := arg.get_slice("=", 1).split(",")
			if at.size() == 2: start_at = Vector2(float(at[0]), float(at[1]))

	world = LucaWorld.new()
	world.setup(world_size, seed_value, population, true)
	if parade:
		event_rate = 0.0   # an art check: keep the ground still
	world.set_event_rate(event_rate)
	if parade:
		var placed: int = world.spawn_parade(world.get_extent() * 0.5 - 9.0, 20.0)
		if start_at.x < 0.0:
			start_at = Vector2(world.get_extent() * 0.5 - 12.5 + 1.6 * (placed - 1), 20.0)   # a little left of centre, clear of the panel
		if start_zoom <= 0.0:
			start_zoom = 6.0
	for v in start_vents:
		world.cast_power(LucaWorld.POWER_VENT, v.x, v.y, 4.0, v.z, 0, false)
	if warmup > 0:
		world.step(warmup, 1e12)
	world_size = world.get_world_size()
	extent = world.get_extent()
	if shot_path == "":
		_fit_window()   # screenshots keep the project's 1440x900, so they stay comparable
	cam_target = Vector3(extent * 0.5, 0.0, extent * 0.5)
	if start_at.x >= 0.0:
		cam_target = Vector3(start_at.x, 0.0, start_at.y)
	# Start at the closest zoom that still shows the whole map.
	zoom_index = 0
	for i in ZOOMS.size():
		var fits: bool = extent * ZOOMS[i] / PX_SIZE <= float(get_window().size.y)
		if (start_zoom > 0.0 and ZOOMS[i] <= start_zoom) or (start_zoom <= 0.0 and fits):
			zoom_index = i

	RenderingServer.set_default_clear_color(Color(0.05, 0.06, 0.09))
	_build_terrain()
	_build_blobs()
	_build_camera()
	_build_ui()
	_set_speed(1 if not demo else 3)
	_set_tool(Tool.LOOK)
	if by_species:
		(find_child("SpeciesCheck", true, false) as CheckBox).button_pressed = true
	if test_poison:
		blob_mat.set_shader_parameter("poison_test", 0.6)
	if start_tree:
		_toggle_tree()
		for i in tree_view.species.size():
			if tree_view.species[i]["name"] == start_tree_root:
				tree_view._zoom_to(i)
	get_window().size_changed.connect(_place_camera)
	get_window().size_changed.connect(_fit_left_column)
	_fit_left_column()
	if start_bank:
		var top: Array = world.get_top_species(1)
		if not top.is_empty():
			_bank(top[0]["id"])
	if start_designer >= 0 or start_vault:
		_open_designer()
		if start_template != "":
			for i in designer.templates.size():
				if designer.templates[i]["name"] == start_template:
					designer.template_pick.select(i)
					designer.load_genome(designer.templates[i]["genome"], start_template, "designed")
		designer._show_tab(clampi(start_designer, 0, 4))
		if start_vault:
			designer._open_library()


## Open the window at most of the screen. The project's 1440x900 is in
## pixels, which on a Retina display is a 720x450-point postage stamp. The
## UI is laid out for 1440x900 and the canvas_items stretch scales it with
## the window, so a bigger window means bigger, more readable panels.
func _fit_window() -> void:
	var win := get_window()
	if win.mode != Window.MODE_WINDOWED:
		return
	var usable := DisplayServer.screen_get_usable_rect(win.current_screen)
	var size := Vector2i((Vector2(usable.size) * 0.92).floor())
	# Keep the 16:10 shape the panels were laid out for, within the screen.
	size.x = mini(size.x, int(size.y * 1.6))
	if size.x <= win.size.x and size.y <= win.size.y:
		return
	win.size = size
	win.position = usable.position + (usable.size - size) / 2


## The left column is stats, the species list and every tool. With many
## species or followed ones it outgrows the window, so it scrolls instead of
## running off the bottom.
func _fit_left_column() -> void:
	if left_scroll == null:
		return
	var room := get_viewport().get_visible_rect().size.y - 24.0
	var want := left_column.get_combined_minimum_size()
	var bar := 0.0 if want.y <= room else left_scroll.get_v_scroll_bar().get_combined_minimum_size().x + 4.0
	left_scroll.size = Vector2(want.x + bar, minf(want.y, room))


# --- Scene construction --------------------------------------------------------

func _build_terrain() -> void:
	var plane := PlaneMesh.new()
	plane.size = Vector2(extent, extent)
	plane.center_offset = Vector3(extent * 0.5, 0.0, extent * 0.5)

	terrain_mat = ShaderMaterial.new()
	terrain_mat.shader = load("res://terrain.gdshader")
	terrain_mat.set_shader_parameter("extent", extent)
	terrain_mat.set_shader_parameter("cells", float(world_size))
	terrain_mat.set_shader_parameter("px_size", PX_SIZE)
	plane.material = terrain_mat

	var terrain := MeshInstance3D.new()
	terrain.mesh = plane
	add_child(terrain)
	_upload_fields()
	_upload_soil()


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


## The soil changes a little every tick (things die, things are built), so
## it is re-sent a few times a second rather than tracked with a counter.
func _upload_soil() -> void:
	var img := Image.create_from_data(world_size, world_size, false, Image.FORMAT_RGBAF, world.get_soil().to_byte_array())
	if soil_tex == null:
		soil_tex = ImageTexture.create_from_image(img)
		terrain_mat.set_shader_parameter("soil_tex", soil_tex)
	else:
		soil_tex.update(img)


func _build_blobs() -> void:
	# One flat 1x1 quad; the shader sizes it per creature and paints the sprite.
	var quad := PlaneMesh.new()
	quad.size = Vector2(1.0, 1.0)
	blob_mat = ShaderMaterial.new()
	blob_mat.shader = load("res://blob.gdshader")
	blob_mat.set_shader_parameter("px_size", PX_SIZE)
	blob_mat.set_shader_parameter("extent", extent)
	quad.material = blob_mat

	multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = true
	multimesh.use_custom_data = true
	multimesh.mesh = quad
	# Instances are placed by the shader, so the automatic bounds would be wrong.
	multimesh.custom_aabb = AABB(Vector3(-8, -1, -8), Vector3(extent + 16, 4, extent + 16))

	var node := MultiMeshInstance3D.new()
	node.multimesh = multimesh
	add_child(node)


func _build_camera() -> void:
	cam = Camera3D.new()
	cam.projection = Camera3D.PROJECTION_ORTHOGONAL
	cam.near = 1.0
	cam.far = 400.0
	add_child(cam)
	_place_camera()


## Screen pixels per world unit at the current zoom.
func _scale() -> float:
	return ZOOMS[zoom_index] / PX_SIZE


func _place_camera() -> void:
	cam_target.x = clampf(cam_target.x, 0.0, extent)
	cam_target.z = clampf(cam_target.z, 0.0, extent)
	cam.size = float(get_window().size.y) / _scale()
	# Sit on whole screen pixels so the picture doesn't shimmer while panning.
	var k := _scale()
	var eye := Vector3(roundf(cam_target.x * k) / k, 200.0, roundf(cam_target.z * k) / k)
	# Looking straight down with north (the sunny edge, z = 0) at the top.
	cam.look_at_from_position(eye, eye + Vector3.DOWN, Vector3(0, 0, -1))


func _zoom(steps: int, around: Vector3) -> void:
	var before := _scale()
	zoom_index = clampi(zoom_index + steps, 0, ZOOMS.size() - 1)
	# Keep the point under the mouse where it is.
	if around.y > -1e8:
		cam_target += (around - cam_target) * (1.0 - before / _scale())


# --- UI ------------------------------------------------------------------------

func _panel(anchor: int, min_size: Vector2, parent: Control = null) -> VBoxContainer:
	var panel := PanelContainer.new()
	if parent == null:
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
	if parent == null:
		$UI.add_child(panel)
	else:
		parent.add_child(panel)
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

	# Left column: time and stats, with the god powers underneath.
	left_scroll = ScrollContainer.new()
	left_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	left_scroll.position = Vector2(12, 12)
	layer.add_child(left_scroll)
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", 10)
	left_scroll.add_child(column)
	left_column = column
	column.minimum_size_changed.connect(_fit_left_column)
	var top := _panel(Control.PRESET_TOP_LEFT, Vector2(330, 0), column)
	stats_label = Label.new()
	top.add_child(stats_label)
	# The most numerous species, each with the colour the species view paints it.
	species_box = RichTextLabel.new()
	species_box.bbcode_enabled = true
	species_box.fit_content = true
	species_box.scroll_active = false
	species_box.mouse_filter = Control.MOUSE_FILTER_PASS
	species_box.meta_clicked.connect(func(meta: Variant) -> void: _open_species(int(str(meta))))
	species_box.add_theme_font_size_override("normal_font_size", 13)
	top.add_child(species_box)
	var speeds := HBoxContainer.new()
	top.add_child(speeds)
	for i in SPEED_NAMES.size():
		speed_buttons.append(_button(speeds, SPEED_NAMES[i], _set_speed.bind(i), true))

	var tools := _panel(Control.PRESET_TOP_LEFT, Vector2(330, 0), column)
	_heading(tools, "TOOLS")
	var tool_grid := GridContainer.new()
	tool_grid.columns = 3
	tools.add_child(tool_grid)
	for i in TOOL_NAMES.size():
		tool_buttons.append(_button(tool_grid, TOOL_NAMES[i], _set_tool.bind(i), true))
	drop_label = Label.new()
	drop_label.add_theme_color_override("font_color", TOOL_COLORS[Tool.DROP])
	drop_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	drop_label.custom_minimum_size.x = 300
	drop_label.visible = false
	tools.add_child(drop_label)
	_slider(tools, "Brush size", 2.0, 40.0, brush_radius, func(v: float) -> void: brush_radius = v)
	_slider(tools, "Strength", 1.0, 20.0, brush_strength, func(v: float) -> void: brush_strength = v)

	_heading(tools, "LIFE")
	var life_row := HBoxContainer.new()
	tools.add_child(life_row)
	_button(life_row, "Creature designer (B)", _open_designer)
	_button(life_row, "Seed vault (V)", _open_vault)

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
	# The world acting on its own: storms, eruptions, meteors, climate drift.
	_slider(tools, "World acts on its own (0 = never)", 0.0, 4.0, event_rate,
		func(v: float) -> void:
			event_rate = v
			world.set_event_rate(v))

	_heading(tools, "VIEW")
	overlay_button = OptionButton.new()
	overlay_button.focus_mode = Control.FOCUS_NONE
	for n in OVERLAY_NAMES:
		overlay_button.add_item(n)
	overlay_button.item_selected.connect(_set_overlay)
	tools.add_child(overlay_button)
	var species_check := CheckBox.new()
	species_check.text = "Colour by species (C)"
	species_check.name = "SpeciesCheck"
	species_check.focus_mode = Control.FOCUS_NONE
	species_check.toggled.connect(_set_by_species)
	tools.add_child(species_check)
	var view_row := HBoxContainer.new()
	tools.add_child(view_row)
	_button(view_row, "Tree of life (T)", _toggle_tree)
	_button(view_row, "Save powers", _save_powers)
	var zoom_row := HBoxContainer.new()
	tools.add_child(zoom_row)
	_button(zoom_row, "Zoom -", func() -> void: _zoom(-1, cam_target))
	_button(zoom_row, "Zoom +", func() -> void: _zoom(1, cam_target))

	# Right: the chronicle.
	var history := _panel(Control.PRESET_TOP_RIGHT, Vector2(380, 240))
	_heading(history, "CHRONICLE")
	chronicle_box = RichTextLabel.new()
	chronicle_box.bbcode_enabled = true
	chronicle_box.scroll_following = true
	chronicle_box.size_flags_vertical = Control.SIZE_EXPAND_FILL
	chronicle_box.custom_minimum_size = Vector2(360, 200)
	chronicle_box.add_theme_font_size_override("normal_font_size", 13)
	history.add_child(chronicle_box)

	# Middle: the tree of life, shown on demand.
	var tree_box := _panel(Control.PRESET_CENTER, Vector2(0, 0))
	tree_panel = tree_box.get_parent()
	tree_panel.grow_horizontal = Control.GROW_DIRECTION_BOTH
	tree_panel.grow_vertical = Control.GROW_DIRECTION_BOTH
	tree_panel.visible = false
	(tree_panel.get_theme_stylebox("panel") as StyleBoxFlat).bg_color = Color(0.065, 0.072, 0.095, 1.0)
	tree_title = Label.new()
	tree_title.add_theme_color_override("font_color", Color(0.65, 0.72, 0.80))
	tree_title.add_theme_font_size_override("font_size", 12)
	tree_box.add_child(tree_title)
	# The tree on the left, the selected species' card on the right.
	var tree_row := HBoxContainer.new()
	tree_row.add_theme_constant_override("separation", 8)
	tree_box.add_child(tree_row)
	tree_view = Control.new()
	tree_view.set_script(load("res://tree_view.gd"))
	tree_view.custom_minimum_size = Vector2(900, 570)
	tree_view.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	tree_view.close_requested.connect(_toggle_tree)
	tree_row.add_child(tree_view)
	species_card = PanelContainer.new()
	species_card.set_script(load("res://species_card.gd"))
	species_card.world = world
	tree_row.add_child(species_card)
	tree_view.selection_changed.connect(func(id: int) -> void: species_card.show_species(id))
	species_card.species_link.connect(_open_species)
	species_card.bank_requested.connect(_bank)

	# Bottom right: the inspected creature.
	var inspector := _panel(Control.PRESET_BOTTOM_RIGHT, Vector2(280, 0))
	_heading(inspector, "CREATURE")
	inspector_label = Label.new()
	inspector_label.text = "Pick Look, then click a blob."
	inspector.add_child(inspector_label)
	_button(inspector, "Its species  (P)", _open_inspected_species)
	follow_button = _button(inspector, "Follow its species  (F)", _toggle_follow, true)
	follow_button.disabled = true
	var vault_row := HBoxContainer.new()
	inspector.add_child(vault_row)
	_button(vault_row, "Edit in designer", _edit_inspected)
	_button(vault_row, "Bank its species", func() -> void: _bank(inspected_species))

	# The creature designer: a screen of its own, over everything.
	var designer_layer := CanvasLayer.new()
	designer_layer.layer = 5
	add_child(designer_layer)
	designer = Control.new()
	designer.set_script(load("res://designer.gd"))
	designer.world = world
	designer.visible = false
	designer_layer.add_child(designer)
	designer.closed.connect(func() -> void: _set_speed(designer_speed))
	designer.release_requested.connect(_start_drop)


# --- The creature designer and the seed vault ----------------------------------------

## Open the designer, pausing the world while it is open.
func _open_designer(genome: Dictionary = {}, title := "") -> void:
	if not designer.visible:
		designer_speed = speed if speed > 0 else 1
		_set_speed(0)
	if tool == Tool.DROP:
		_set_tool(Tool.LOOK)
	designer.open(genome, title)


func _open_vault() -> void:
	_open_designer()
	designer._open_library()


## Open the inspected creature's own genome in the designer.
func _edit_inspected() -> void:
	if inspected_id < 0:
		return
	var g: Dictionary = world.genome_of(inspected_id)
	if g.is_empty():
		return
	var info: Dictionary = world.inspect_id(inspected_id)
	_open_designer(g, "%s variant" % info.get("species", "Creature"))


## Bank a species in the seed vault: a sample of its living members.
func _bank(species_id: int) -> void:
	if species_id < 0:
		return
	chronicle_box.append_text("[color=#%s]%s[/color]\n" % [ALERT_COLOR, designer.bank_species(species_id)])


## The designer handed over a creature file: drop it where the player clicks next.
func _start_drop(text: String, title: String, count: int) -> void:
	drop_text = text
	drop_name = title
	drop_count = count
	_set_speed(designer_speed)
	_set_tool(Tool.DROP)


func _cancel_drop() -> void:
	if tool == Tool.DROP:
		_set_tool(Tool.LOOK)


func _drop_radius() -> float:
	return maxf(2.0, sqrt(float(drop_count)) * 0.8)


func _drop() -> void:
	if hover.y < -1e8 or drop_text == "":
		return
	if world.release(drop_text, hover.x, hover.z, _drop_radius(), drop_count):
		_set_tool(Tool.LOOK)
		drop_text = ""
	else:
		chronicle_box.append_text("[color=#f88]That creature file could not be read.[/color]\n")


## Follow the inspected creature's species, or stop. Observation only: a
## followed species gets no help, but what happens to it shows in the chronicle.
func _toggle_follow() -> void:
	if inspected_species < 0:
		follow_button.set_pressed_no_signal(false)
		return
	var on := not world.is_following(inspected_species)
	world.follow_species(inspected_species, on)
	var info: Dictionary = world.inspect_id(inspected_id)
	var name: String = info.get("species", "")
	chronicle_box.append_text("[color=#%s]%s %s.[/color]\n" % [ALERT_COLOR, "Now following" if on else "No longer following", name])
	_update_follow_button(on, name)


func _update_follow_button(on: bool, name: String) -> void:
	follow_button.disabled = inspected_species < 0
	follow_button.set_pressed_no_signal(on)
	follow_button.text = ("Following %s  (F to stop)" % name) if on else ("Follow %s  (F)" % name if name != "" else "Follow its species  (F)")


func _set_speed(index: int) -> void:
	speed = clampi(index, 0, SPEED_TICKS.size() - 1)
	for i in speed_buttons.size():
		speed_buttons[i].set_pressed_no_signal(i == speed)


func _set_tool(index: int) -> void:
	if index == Tool.DROP and drop_text == "":
		index = Tool.LOOK
	tool = index
	for i in tool_buttons.size():
		tool_buttons[i].set_pressed_no_signal(i == tool)
	if drop_label != null:
		drop_label.visible = tool == Tool.DROP
		drop_label.text = "Click the map to drop %d %s by meteor.  Esc to cancel." % [drop_count, drop_name]


func _set_overlay(index: int) -> void:
	overlay = index % OVERLAY_NAMES.size()
	overlay_button.select(overlay)
	terrain_mat.set_shader_parameter("overlay", overlay)


func _toggle_tree() -> void:
	tree_panel.visible = not tree_panel.visible
	if not tree_panel.visible:
		return
	# Nearly the whole window, over the other panels: it is a view of its own.
	var win := get_viewport().get_visible_rect().size
	var card_w: float = species_card.custom_minimum_size.x + 8.0
	tree_view.custom_minimum_size = Vector2(maxf(win.x - 80.0 - card_w, 480.0), maxf(win.y - 90.0, 360.0))
	tree_panel.reset_size()
	tree_panel.position = ((win - tree_panel.size) * 0.5).floor()
	tree_panel.move_to_front()
	_update_tree()
	tree_view.grab_focus()
	# Open on the species of the creature being followed, if any; otherwise on
	# whatever was picked last, or the most numerous species alive.
	if inspected_id >= 0:
		var info: Dictionary = world.inspect_id(inspected_id)
		if not info.is_empty():
			tree_view.select_id(info["species_id"])
			species_card.show_species(info["species_id"])
			return
	if tree_view.selected_id < 0:
		var top: Array = world.get_top_species(1)
		if not top.is_empty():
			tree_view.select_id(top[0]["id"])


## Open the tree and the card on one species (from the card's food web, the
## species list, or the inspector).
func _open_species(id: int) -> void:
	if id < 0:
		return
	if not tree_panel.visible:
		var keep := inspected_id
		inspected_id = -1      # don't let opening jump to the followed creature's species
		_toggle_tree()
		inspected_id = keep
	tree_view.select_id(id)
	species_card.show_species(id)   # also for species the tree doesn't list (never 10 alive)


func _open_inspected_species() -> void:
	if inspected_id < 0:
		return
	var info: Dictionary = world.inspect_id(inspected_id)
	if not info.is_empty():
		_open_species(info["species_id"])


func _update_tree() -> void:
	if not tree_panel.visible:
		return
	var tree: Array = world.get_species_tree(10)
	var alive := 0
	for s: Dictionary in tree:
		if s["extinct"] < 0:
			alive += 1
	tree_title.text = "TREE OF LIFE    %d species have taken hold, %d still alive    T or Esc to close" % [tree.size(), alive]
	tree_view.show_tree(tree, world.get_tick())
	species_card.refresh()


func _set_by_species(on: bool) -> void:
	by_species = on
	blob_mat.set_shader_parameter("by_species", on)


## Matches the species colour in blob.gdshader.
func _species_color(hue: float) -> Color:
	return Color.from_hsv(hue, 0.7, 0.92)


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
	if tool in [Tool.VENT, Tool.SEAL]:
		# Vent strength runs 0..3, so the brush is gentler: about half a unit a second.
		power = LucaWorld.POWER_VENT
		direction = (-1.0 if tool == Tool.SEAL else 1.0) * VENT_BRUSH
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
		inspected_id = -1
		inspected_species = -1
		inspector_label.text = "Nothing lives here."
		_update_follow_button(false, "")
		return
	inspected_id = info["id"]
	_show_creature(info)


## What a creature lives on, worked out from where its energy has come from.
func _diet(info: Dictionary) -> String:
	var light: float = info["fed_light"]
	var living: float = info["fed_living"]
	var dead: float = info["fed_dead"]
	var waste: float = info["fed_waste"]
	var vent: float = info["fed_vent"]
	var total := light + living + dead + waste + vent
	if total < 1.0:
		return "has not fed yet"
	var text := "sun %d%%, the living %d%%, the dead %d%%" % [
		roundi(100.0 * light / total), roundi(100.0 * living / total), roundi(100.0 * dead / total)]
	if waste > 0.0:
		text += ", waste %d%%" % roundi(100.0 * waste / total)
	if vent > 0.0:
		text += ", vents %d%%" % roundi(100.0 * vent / total)
	return text


## How the mutability gene (log2 of the mutation rate) reads in words.
func _rate_words(m: float) -> String:
	var rate := pow(2.0, m)
	if absf(m) < 0.1:
		return "about"
	return "%.1fx" % rate if rate >= 1.0 else "1/%.1f of" % (1.0 / rate)


func _shape_words(info: Dictionary) -> String:
	var e: float = info["elongation"]
	var sq: float = info["squareness"]
	var length := "round" if e < 0.15 else ("oval" if e < 0.5 else "long")
	if sq >= 0.5:
		length = "square" if e < 0.15 else ("boxy" if e < 0.5 else "a long box")
	if e < 0.15:
		return length
	return "%s, %.1f times as long as wide" % [length, 1.0 + 2.0 * e]


## The brain in words, with each hidden neuron's time constant and how it is
## firing right now (-1 .. +1), drawn as a small bar.
func _brain_words(info: Dictionary) -> String:
	if info["brain_neurons"] < 0:
		return "No brain"
	var n: int = info["brain_neurons"]
	var text := "Brain: %d hidden neuron%s, %d links" % [n, "" if n == 1 else "s", info["brain_links"]]
	var taus: PackedFloat32Array = info["brain_taus"]
	var firing: PackedFloat32Array = info["brain_firing"]
	for k in taus.size():
		var f := firing[k]
		var bar := "▮".repeat(roundi(absf(f) * 5.0))
		text += "\n  neuron %d: τ %s ticks   %s%s" % [k + 1, _commas(roundi(taus[k])), "+" if f >= 0.0 else "−", bar]
	return text


func _show_creature(info: Dictionary) -> void:
	inspected_at = Vector2(info["x"], info["z"])
	inspected_species = info["species_id"]
	_update_follow_button(info["species_followed"], info["species"])
	var parts: PackedStringArray = info["parts"]
	var senses := "blind" if info["sense_cells"] == 0 else "senses %d cells" % info["sense_cells"]
	inspector_label.text = "\n".join([
		"%s%s" % [info["species"], "" if info["species_parent"] == "" else "  (from %s)" % info["species_parent"]],
		"%s alive of its kind   generation %d" % [_commas(info["species_population"]), info["generation"]],
		"Age %d ticks" % info["age"],
		"Energy %.0f of %.0f  (divides at %.0f)" % [info["energy"], info["energy_capacity"], info["repro_threshold"]],
		"Eats: " + _diet(info),
		"Enzymes: %s" % (", ".join(info["enzymes"]) if info["enzymes"].size() > 0 else "none"),
		"Holds %s   body of %s" % [info["store_chemicals"] if info["store_chemicals"] != "" else "nothing", info["body_chemicals"]],
		"Radius %.2f   mass %.2f   %s" % [info["radius"], info["mass"], _shape_words(info)],
		"Skin %s, %s   %s" % [_skin_hue_word(info["hue"]), "pale" if info["pigment"] < 0.3 else ("dark" if info["pigment"] > 0.7 else "mid-toned"), "blends into the ground here" if info["shows"] < 0.5 else ("stands out a little here" if info["shows"] < 0.85 else "stands out here")],
		"Upkeep %.2f / tick   speed %.2f" % [info["upkeep"], info["max_speed"]],
		("Young land touching it" if info["throw_range"] < 0.05 else "Flings its young up to %.1f cells" % info["throw_range"]) + ", born at %d%% of its mass" % roundi(100.0 * info["birth_size"]),
		"Fully grown" if info["grown"] >= 1.0 else "Still growing: %d%% of its adult mass, can't breed yet" % roundi(100.0 * info["grown"]),
		"Pull %.2f   membrane %.2f   %s" % [info["absorb"], info["membrane"], senses],
		_brain_words(info),
		"Never sleeps" if info["dormancy"] < 0.01 else "Sleeps to %d%% depth   now %s" % [roundi(100.0 * info["dormancy"]), "awake" if info["sleep"] < 0.1 else "%d%% asleep" % roundi(100.0 * info["sleep"])],
		"Bite x%.1f   keeps %d%% of a meal" % [info["bite"], roundi(100.0 * info["efficiency"])],
		"Burns %s" % info["burns"] + ("   can live on what vents make" if info["burns_vent"] else "   can't feed at vents"),
		"Copied with %s the usual mistakes   %s" % [_rate_words(info["mutability"]), "no repair" if info["repair"] <= 0.0 else "repairs itself: ages %.1fx slower" % (1.0 + 2.0 * info["repair"])],
		"Not poisonous" if info["toxicity"] <= 0.0 else "Poisonous: eating 10 costs the eater %.0f" % (10.0 * info["toxicity"]),
		"Body %.1f °C   comfort %.0f ± %.0f °C" % [info["body_temperature"], info["thermal_optimum"], info["thermal_tolerance"]],
		"Here: light %.2f, %.1f °C, soil %.1f, waste %.1f%s" % [info["cell_light"], info["cell_temperature"], info["cell_nutrient"], info["cell_waste"], "" if info["cell_vent"] <= 0.0 else ", vent %.2f" % info["cell_vent"]],
		"Parts: " + ("\n  ".join(parts) if parts.size() > 0 else "none"),
	])


# --- Input -----------------------------------------------------------------------

func _unhandled_input(event: InputEvent) -> void:
	if designer.visible:
		return
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT:
			if event.pressed:
				if tool == Tool.LOOK:
					_inspect()
				elif tool == Tool.DROP:
					_drop()
				else:
					painting = not gradual
					stroke_started = false
					paint_clock = 0.0
					_dab(0.05)
			else:
				painting = false
		elif event.button_index == MOUSE_BUTTON_RIGHT or event.button_index == MOUSE_BUTTON_MIDDLE:
			dragging = event.pressed
		elif event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			_zoom(1, hover)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			_zoom(-1, hover)
	elif event is InputEventMouseMotion and dragging:
		_pan(-event.relative.x, -event.relative.y)
	elif event is InputEventMagnifyGesture:
		if event.factor > 1.04: _zoom(1, hover)
		elif event.factor < 0.96: _zoom(-1, hover)
	elif event is InputEventPanGesture:
		_pan(event.delta.x * 12.0, event.delta.y * 12.0)
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_SPACE: _set_speed(1 if speed == 0 else 0)
			KEY_1: _set_speed(0)
			KEY_2: _set_speed(1)
			KEY_3: _set_speed(2)
			KEY_4: _set_speed(3)
			KEY_5: _set_speed(4)
			KEY_TAB: _set_overlay(overlay + 1)
			KEY_T: _toggle_tree()
			KEY_F: _toggle_follow()
			KEY_P: _open_inspected_species()
			KEY_B: _open_designer()
			KEY_V: _open_vault()
			KEY_ESCAPE: _cancel_drop()
			KEY_C: (find_child("SpeciesCheck", true, false) as CheckBox).button_pressed = not by_species
			KEY_MINUS: _zoom(-1, cam_target)
			KEY_EQUAL: _zoom(1, cam_target)
			KEY_BRACKETLEFT: brush_radius = maxf(2.0, brush_radius - 2.0)
			KEY_BRACKETRIGHT: brush_radius = minf(40.0, brush_radius + 2.0)


func _input(event: InputEvent) -> void:
	# Releases must always arrive, even if the mouse ended up over a panel.
	if event is InputEventMouseButton and not event.pressed:
		if event.button_index == MOUSE_BUTTON_LEFT:
			painting = false
		elif event.button_index == MOUSE_BUTTON_RIGHT or event.button_index == MOUSE_BUTTON_MIDDLE:
			dragging = false


## Move the map by a number of screen pixels.
func _pan(right: float, down: float) -> void:
	cam_target += Vector3(right, 0.0, down) / _scale()


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
	var move := Vector2.ZERO
	var keys_free := not designer.visible   # the designer has keys of its own
	if not tree_panel.visible and keys_free:   # the tree uses the arrow keys itself
		move = Input.get_vector("ui_left", "ui_right", "ui_down", "ui_up")
	if keys_free:
		if Input.is_key_pressed(KEY_A): move.x -= 1.0
		if Input.is_key_pressed(KEY_D): move.x += 1.0
		if Input.is_key_pressed(KEY_W): move.y += 1.0
		if Input.is_key_pressed(KEY_S): move.y -= 1.0
	if move != Vector2.ZERO:
		_pan(move.x * delta * 700.0, -move.y * delta * 700.0)
	_place_camera()

	# 3. Where is the mouse on the ground? The camera looks straight down, so
	#    it is simply the point under the cursor.
	var under := cam.project_ray_origin(get_viewport().get_mouse_position())
	var on_map := under.x >= 0.0 and under.x <= extent and under.z >= 0.0 and under.z <= extent
	hover = Vector3(under.x, 0.0, under.z) if on_map else Vector3(0, -1e9, 0)

	# 4. Painting.
	if painting:
		paint_clock += delta
		if paint_clock >= 0.05:
			_dab(paint_clock)
			paint_clock = 0.0

	# 5. Draw.
	_upload_fields()
	if frame % 12 == 0:
		_upload_soil()
	_update_blobs()
	_update_sun()
	_update_brush_ring()
	_update_ui()
	if frame == 2 and start_inspect.x >= 0.0:
		var info: Dictionary = world.inspect(start_inspect.x, start_inspect.y, 3.0)
		if not info.is_empty():
			inspected_id = info["id"]
			_show_creature(info)
			if start_follow:
				_toggle_follow()

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
		multimesh.buffer = world.get_instance_buffer(capacity)
		# The parts texture follows the same capacity; it is remade only when that grows.
		var width: int = world.get_part_tex_width()
		var rows: int = maxi(1, ceili(float(capacity * world.get_parts_per_creature()) / float(width)))
		var img := Image.create_from_data(width, rows, false, Image.FORMAT_RGBAF, world.get_part_buffer(capacity).to_byte_array())
		if parts_tex == null or rows != parts_rows:
			parts_tex = ImageTexture.create_from_image(img)
			parts_rows = rows
			blob_mat.set_shader_parameter("parts_tex", parts_tex)
			blob_mat.set_shader_parameter("parts_width", width)
		else:
			parts_tex.update(img)


## Day and night: the whole picture cools and darkens after sunset.
func _update_sun() -> void:
	var target := float(world.get_daylight())
	# When days fly past faster than the eye can follow, hold a steady dusk
	# instead of strobing.
	var day_length := world.get_day_length()
	if day_length > 0 and ticks_per_second > day_length:
		target = 0.8
	shown_daylight = lerpf(shown_daylight, target, 0.2)
	var dim := clampf(0.25 + 0.75 * float(world.get_sun_scale()), 0.2, 1.3)  # the sun power dims it too
	terrain_mat.set_shader_parameter("tint", NIGHT_GROUND.lerp(Vector3.ONE, shown_daylight) * dim)
	blob_mat.set_shader_parameter("tint", NIGHT_BLOBS.lerp(Vector3.ONE, shown_daylight) * lerpf(1.0, dim, 0.5))


func _update_brush_ring() -> void:
	var ring := Vector3(0, 0, -1)
	var color: Color = TOOL_COLORS[tool]
	if tool == Tool.DROP and hover.y > -1e8:
		ring = Vector3(hover.x, hover.z, _drop_radius())
	elif tool != Tool.LOOK and hover.y > -1e8:
		ring = Vector3(hover.x, hover.z, brush_radius)
	elif tool == Tool.LOOK and inspected_id >= 0:
		ring = Vector3(inspected_at.x, inspected_at.y, 1.6)
	terrain_mat.set_shader_parameter("brush", ring)
	terrain_mat.set_shader_parameter("brush_color", Vector3(color.r, color.g, color.b))


func _clock() -> String:
	var day_length := world.get_day_length()
	if day_length <= 0:
		return ""
	var minutes := int(fposmod(world.get_day_phase() + 0.25, 1.0) * 1440.0)  # phase 0 is sunrise, 06:00
	return "Day %d, %02d:%02d    " % [world.get_tick() / day_length + 1, minutes / 60, minutes % 60]


func _update_ui() -> void:
	var census: Dictionary = world.get_census()
	stats_label.text = "%sTick %s\nPopulation %s\nSun-fed %s   Hunters %s   Scavengers %s\nCleaners %s   Vent-fed %s   Mixed diet %s   Able to move %s\n%d ticks/s   %.2f ms/tick   %d fps\nSun %.2fx    Climate %+.1f °C" % [
		_clock(), _commas(world.get_tick()), _commas(world.get_population()),
		_commas(census.get("producers", 0)), _commas(census.get("hunters", 0)), _commas(census.get("scavengers", 0)),
		_commas(census.get("cleaners", 0)), _commas(census.get("chemotrophs", 0)), _commas(census.get("mixed", 0)), _commas(census.get("movers", 0)),
		ticks_per_second, world.get_tick_ms(), Engine.get_frames_per_second(),
		world.get_sun_scale(), world.get_climate_offset()]

	if frame % 15 == 1:
		var lines: PackedStringArray = ["Species %d  (%d with 10 or more)" % [world.get_species_count(1), world.get_species_count(10)]]
		for s: Dictionary in world.get_top_species(6):
			lines.append("[url=%d][color=#%s]██[/color] %s[/url]  %s" % [s["id"], _species_color(s["hue"]).to_html(false), s["name"], _commas(s["population"])])
		var followed: Array = world.get_followed()
		if not followed.is_empty():
			lines.append("[color=#%s]Following[/color]" % ALERT_COLOR)
			for s: Dictionary in followed:
				lines.append("[url=%d][color=#%s]██[/color] %s[/url]  %s" % [s["id"], _species_color(s["hue"]).to_html(false), s["name"],
					_commas(s["population"]) if s["alive"] else "extinct"])
		species_box.text = "\n".join(lines)

	if frame % 30 == 2:
		_update_tree()

	var size := world.get_chronicle_size()
	if size > chronicle_seen:
		for line in world.get_chronicle(chronicle_seen):
			chronicle_box.append_text(line + "\n")
		chronicle_seen = size
	# Alerts about followed species, in their own colour.
	var alert_count := world.get_alerts_size()
	if alert_count > alerts_seen:
		for line in world.get_alerts(alerts_seen):
			chronicle_box.append_text("[color=#%s]%s[/color]\n" % [ALERT_COLOR, line])
		alerts_seen = alert_count

	# Follow the picked creature as it moves; say so when it dies.
	if inspected_id >= 0 and frame % 6 == 0:
		var info: Dictionary = world.inspect_id(inspected_id)
		if info.is_empty():
			inspected_id = -1
			inspected_species = -1
			inspector_label.text = "It died."
			_update_follow_button(false, "")
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


# A rough name for a skin hue (core/colour.hpp keeps every hue earthy).
func _skin_hue_word(v: float) -> String:
	var words := ["rust", "tan", "ochre", "olive", "sage", "sage", "grey-green", "slate", "slate", "dusky", "mauve", "rose"]
	return words[int(round(fposmod(v, 1.0) * 12.0)) % 12]
