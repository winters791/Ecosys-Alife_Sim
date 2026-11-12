extends PanelContainer
## The species card: one species' profile, beside the tree of life.
##
## Top to bottom: its name and standing; a living portrait (real members,
## drawn by the same blob shader as the world, walking if they can walk, or
## the founder in stone once the species is extinct); its niche, worked out
## from where its food really comes from, with tags for what stands out; a
## short natural history written from its numbers; its diet; its body, part
## by part; its record book; whom it kills and who kills it; and where it
## lives.
##
## Everything comes from LucaWorld.get_species_profile() and get_sprites().
## The words are worked out here; nothing feeds back into the world.

signal species_link(species_id: int)   # a species named on the card was clicked

const WIDTH := 404
const PORTRAIT := Vector2i(372, 164)
const PX_SIZE := 0.125                 # world units per sprite pixel, as in main.gd
const STRIDE := 20                     # floats per sprite in the instance buffer
const PARTS := 12                      # part texels per sprite

const BG := Color(0.05, 0.056, 0.075)
const INK := Color(0.93, 0.94, 0.96)
const FAINT := Color(0.58, 0.63, 0.71)
const RULE := Color(1, 1, 1, 0.08)
const DIET_COLORS := [Color(0.88, 0.78, 0.30), Color(0.86, 0.32, 0.30), Color(0.62, 0.52, 0.44), Color(0.56, 0.58, 0.26)]
const DIET_WORDS := ["sun", "the living", "the dead", "waste"]
# The colour each part has in blob.gdshader, near enough.
const PART_COLORS := [Color(0.36, 0.72, 0.22), Color(0.70, 0.14, 0.16), Color(0.80, 0.68, 0.48), Color(0.58, 0.40, 0.24),
		Color(0.95, 0.95, 0.95), Color(0.55, 0.57, 0.62), Color(0.95, 0.88, 0.62), Color(0.78, 0.30, 0.95)]
const NUMBERS := ["no", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve"]

var world: Object                      # the LucaWorld, set by main.gd
var species_id := -1
var profile: Dictionary = {}

# The portrait.
var viewport: SubViewport
var cam: Camera3D
var multimesh: MultiMesh
var blob_mat: ShaderMaterial
var ground_mat: ShaderMaterial
var parts_tex: ImageTexture
var frame_style: StyleBoxFlat
var instances := PackedFloat32Array()
var last_parts := PackedFloat32Array()
var speeds := PackedFloat32Array()
var fossil := false
var walk := PackedVector2Array()       # where each specimen is shown, world units from the centre
var bounds := Vector2.ONE              # how far a walker may stray before it wraps round
var zoom := 3                          # screen pixels per sprite pixel
var sprite_clock := 0.0

# The rest of the card.
var content: VBoxContainer
var empty_label: Label
var swatch: ColorRect
var title: Label
var subtitle: Label
var caption: Label
var role: Label
var role_blurb: Label
var tags: HFlowContainer
var story: RichTextLabel
var diet_bar: Control
var diet_legend: RichTextLabel
var body_text: RichTextLabel
var records: GridContainer
var web_text: RichTextLabel
var range_text: RichTextLabel


func _ready() -> void:
	custom_minimum_size.x = WIDTH
	var style := StyleBoxFlat.new()
	style.bg_color = BG
	add_theme_stylebox_override("panel", style)

	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	add_child(scroll)
	var margin := MarginContainer.new()
	margin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 12)
	margin.add_theme_constant_override("margin_right", 16)   # room for the scrollbar
	scroll.add_child(margin)
	var outer := VBoxContainer.new()
	# Wrapped labels in a scroll container have no width of their own: give them one.
	outer.custom_minimum_size.x = WIDTH - 32
	margin.add_child(outer)

	empty_label = _label("Select a species in the tree to see its profile.", 13, FAINT)
	outer.add_child(empty_label)
	content = VBoxContainer.new()
	content.add_theme_constant_override("separation", 7)
	content.visible = false
	outer.add_child(content)

	# Name and standing.
	var head := HBoxContainer.new()
	head.add_theme_constant_override("separation", 10)
	content.add_child(head)
	swatch = ColorRect.new()
	swatch.custom_minimum_size = Vector2(14, 14)
	swatch.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	head.add_child(swatch)
	var names := VBoxContainer.new()
	names.add_theme_constant_override("separation", 0)
	names.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(names)
	title = _label("", 22, INK)
	names.add_child(title)
	subtitle = _label("", 12, FAINT)
	names.add_child(subtitle)

	_build_portrait()
	caption = _label("", 11, FAINT)
	content.add_child(caption)

	# Its niche.
	role = _label("", 18, INK)
	content.add_child(role)
	role_blurb = _label("", 12, FAINT)
	content.add_child(role_blurb)
	tags = HFlowContainer.new()
	tags.add_theme_constant_override("h_separation", 5)
	tags.add_theme_constant_override("v_separation", 5)
	content.add_child(tags)
	story = _rich(13)
	content.add_child(story)

	_heading("DIET")
	diet_bar = Control.new()
	diet_bar.custom_minimum_size = Vector2(0, 14)
	diet_bar.draw.connect(_draw_diet)
	content.add_child(diet_bar)
	diet_legend = _rich(12)
	content.add_child(diet_legend)

	_heading("BODY")
	body_text = _rich(12)
	content.add_child(body_text)

	_heading("RECORD BOOK")
	records = GridContainer.new()
	records.columns = 2
	records.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	records.add_theme_constant_override("h_separation", 14)
	records.add_theme_constant_override("v_separation", 3)
	content.add_child(records)

	_heading("FOOD WEB")
	web_text = _rich(12)
	web_text.meta_clicked.connect(func(meta: Variant) -> void: species_link.emit(int(str(meta))))
	content.add_child(web_text)

	_heading("RANGE")
	range_text = _rich(12)
	content.add_child(range_text)


func _label(text: String, font_size: int, color: Color) -> Label:
	var l := Label.new()
	l.text = text
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.add_theme_font_size_override("font_size", font_size)
	l.add_theme_color_override("font_color", color)
	return l


func _rich(font_size: int) -> RichTextLabel:
	var r := RichTextLabel.new()
	r.bbcode_enabled = true
	r.fit_content = true
	r.scroll_active = false
	r.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	r.add_theme_font_size_override("normal_font_size", font_size)
	r.add_theme_font_size_override("bold_font_size", font_size)
	r.add_theme_color_override("default_color", INK)
	return r


func _heading(text: String) -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 2)
	var spacer := Control.new()
	spacer.custom_minimum_size.y = 4
	box.add_child(spacer)
	box.add_child(_label(text, 11, FAINT))
	var line := ColorRect.new()
	line.color = RULE
	line.custom_minimum_size.y = 1
	box.add_child(line)
	content.add_child(box)


# --- The portrait ------------------------------------------------------------------

func _build_portrait() -> void:
	var frame := PanelContainer.new()
	frame_style = StyleBoxFlat.new()
	frame_style.bg_color = BG
	frame_style.set_border_width_all(2)
	frame_style.border_color = FAINT
	frame.add_theme_stylebox_override("panel", frame_style)
	frame.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	content.add_child(frame)

	var holder := SubViewportContainer.new()
	holder.custom_minimum_size = Vector2(PORTRAIT)
	holder.stretch = true
	frame.add_child(holder)
	viewport = SubViewport.new()
	viewport.size = PORTRAIT
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_WHEN_VISIBLE
	holder.add_child(viewport)

	cam = Camera3D.new()
	cam.projection = Camera3D.PROJECTION_ORTHOGONAL
	cam.near = 1.0
	cam.far = 400.0
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = BG
	cam.environment = env
	viewport.add_child(cam)
	cam.look_at_from_position(Vector3(0, 200, 0), Vector3(0, 0, 0), Vector3(0, 0, -1))

	# The ground: two tones on the world's pixel grid, coloured by where it lives.
	var ground_shader := Shader.new()
	ground_shader.code = """
shader_type spatial;
render_mode unshaded;
uniform vec3 base = vec3(0.3, 0.4, 0.2);
uniform vec3 speck = vec3(0.24, 0.34, 0.16);
uniform float px = 0.125;
varying vec3 world_pos;
void vertex() { world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; }
void fragment() {
	vec2 cell = floor(world_pos.xz / px);
	float h = fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453);
	ALBEDO = h < 0.14 ? speck : (h > 0.97 ? base * 1.12 : base);
}
"""
	ground_mat = ShaderMaterial.new()
	ground_mat.shader = ground_shader
	ground_mat.set_shader_parameter("px", PX_SIZE)
	var plane := PlaneMesh.new()
	plane.size = Vector2(200, 200)
	plane.material = ground_mat
	var ground := MeshInstance3D.new()
	ground.mesh = plane
	viewport.add_child(ground)

	# The bodies: the world's own sprite shader on a few quads.
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
	multimesh.custom_aabb = AABB(Vector3(-50, -1, -50), Vector3(100, 4, 100))
	var node := MultiMeshInstance3D.new()
	node.multimesh = multimesh
	viewport.add_child(node)


## Fetch the specimens' bodies again (they turn, sleep and grow fat as they live).
func _refresh_sprites(fresh: bool) -> void:
	if world == null or species_id < 0:
		return
	var ids: PackedInt64Array = profile.get("specimens", PackedInt64Array())
	var got: Dictionary = world.get_sprites(ids, species_id)
	if got.is_empty():
		return
	instances = got["instances"]
	speeds = got["speeds"]
	fossil = got["fossil"]
	var n := instances.size() / STRIDE
	var parts: PackedFloat32Array = got["parts"]
	last_parts = parts
	if fresh or n != walk.size():
		_place(n)

	var width := maxi(n, 1) * PARTS
	if parts.size() < width * 4:
		parts.resize(width * 4)
	var img := Image.create_from_data(width, 1, false, Image.FORMAT_RGBAF, parts.to_byte_array())
	if parts_tex == null or parts_tex.get_width() != width:
		parts_tex = ImageTexture.create_from_image(img)
		blob_mat.set_shader_parameter("parts_tex", parts_tex)
		blob_mat.set_shader_parameter("parts_width", width)
	else:
		parts_tex.update(img)
	blob_mat.set_shader_parameter("stone", 1.0 if fossil else 0.0)
	multimesh.instance_count = n
	_push()


## Lay the specimens out side by side and pick a zoom that fits them.
func _place(n: int) -> void:
	walk.resize(n)
	if n == 0:
		return
	# How far the biggest body reaches, in sprite pixels (as blob.gdshader sizes it).
	var reach := 8.0
	for k in n:
		var r := instances[k * STRIDE] * 4.5 * 1.1
		var e := clampf(instances[k * STRIDE + 4], 0.0, 1.0)
		var stalk := 0.0
		for t in PARTS:
			stalk = maxf(stalk, last_parts[(k * PARTS + t) * 4 + 3] if last_parts.size() > (k * PARTS + t) * 4 + 3 else 0.0)
		reach = maxf(reach, r * pow(1.0 + 2.0 * e, 0.6667) + 5.0 + stalk / PX_SIZE)
	var fit_w := float(PORTRAIT.x) / (n * 2.0 * reach + 8.0)
	var fit_h := float(PORTRAIT.y) / (2.0 * reach + 8.0)
	zoom = clampi(int(floor(minf(fit_w, fit_h))), 1, 8)
	var per_unit := zoom / PX_SIZE                     # screen pixels per world unit
	cam.size = PORTRAIT.y / per_unit
	var spacing := float(PORTRAIT.x) / n / per_unit
	for k in n:
		walk[k] = Vector2((k - (n - 1) * 0.5) * spacing, 0.0)
	bounds = Vector2(PORTRAIT.x * 0.5 / per_unit - reach * PX_SIZE * 0.5, PORTRAIT.y * 0.5 / per_unit - reach * PX_SIZE * 0.5)
	bounds = bounds.max(Vector2(0.5, 0.5))


func _push() -> void:
	var n := walk.size()
	if n == 0 or instances.size() < n * STRIDE:
		multimesh.instance_count = 0
		return
	var buf := instances.duplicate()
	for k in n:
		buf[k * STRIDE + 3] = walk[k].x
		buf[k * STRIDE + 11] = walk[k].y
	multimesh.buffer = buf


func _process(delta: float) -> void:
	if not is_visible_in_tree() or species_id < 0:
		return
	sprite_clock += delta
	if sprite_clock >= 0.25:
		sprite_clock = 0.0
		_refresh_sprites(false)
	# Walkers walk the way they face, a little faster than life so it shows,
	# and come round again from the other side.
	var moved := false
	for k in walk.size():
		if k >= speeds.size() or speeds[k] <= 0.0 or instances[k * STRIDE + 5] > 0.1:
			continue
		var dir := Vector2(instances[k * STRIDE + 6], instances[k * STRIDE + 10])
		var p := walk[k] + dir * clampf(speeds[k] * 40.0, 0.25, 1.6) * delta
		if absf(p.x) > bounds.x:
			p.x = -signf(p.x) * bounds.x
		if absf(p.y) > bounds.y:
			p.y = -signf(p.y) * bounds.y
		walk[k] = p
		moved = true
	if moved:
		_push()


# --- Showing a species -------------------------------------------------------------

func show_species(id: int) -> void:
	if id == species_id:
		refresh()
		return
	species_id = id
	walk.resize(0)
	refresh()
	_refresh_sprites(true)


## Re-read the profile (main.gd calls this a couple of times a second).
func refresh() -> void:
	if world == null or species_id < 0:
		content.visible = false
		empty_label.visible = true
		return
	profile = world.get_species_profile(species_id)
	if profile.is_empty():
		content.visible = false
		empty_label.visible = true
		return
	content.visible = true
	empty_label.visible = false

	var alive: bool = profile["extinct"] < 0
	var col := _hue(profile["hue"]) if alive else Color(0.55, 0.55, 0.6)
	swatch.color = col
	frame_style.border_color = col.darkened(0.35)
	title.text = profile["name"]
	if alive:
		subtitle.text = "%s alive · most ever %s · since tick %s" % [_commas(profile["population"]), _commas(profile["peak"]), _commas(profile["born"])]
	else:
		subtitle.text = "Extinct at tick %s, after %s ticks" % [_commas(profile["extinct"]), _commas(profile["extinct"] - profile["born"])]
	_caption()
	_ground()

	var niche := _niche()
	role.text = niche[0]
	role.add_theme_color_override("font_color", col.lightened(0.35))
	role_blurb.text = niche[1]
	for child in tags.get_children():
		tags.remove_child(child)
		child.queue_free()
	for t: Array in _tags():
		tags.add_child(_pill(t[0], t[1]))
	story.text = _story(niche)

	diet_bar.queue_redraw()
	diet_legend.text = _diet_legend()
	body_text.text = _body()
	_records()
	web_text.text = _web()
	range_text.text = _range()


func _caption() -> void:
	var names: PackedStringArray = profile.get("specimen_names", PackedStringArray())
	if profile["extinct"] >= 0 or names.is_empty():
		caption.text = "In stone: the founder's body, rebuilt from its blueprint."
		return
	var text := "Living now: " + ", ".join(names) + "."
	if profile.has("eldest_alive_name"):
		text += "  %s is the eldest, %s ticks old." % [profile["eldest_alive_name"], _commas(profile["eldest_alive_age"])]
	caption.text = text


## Ground under the portrait, in the colours of where it lives.
func _ground() -> void:
	var h: Dictionary = profile["habits"]
	var base := Color(0.30, 0.42, 0.20)
	var speck := Color(0.24, 0.34, 0.15)
	if profile["extinct"] >= 0:
		base = Color(0.33, 0.30, 0.27)
		speck = Color(0.27, 0.25, 0.22)
	elif h["count"] > 0:
		var t: float = h["temperature"]
		if t < 2.0:
			base = Color(0.74, 0.78, 0.82)
			speck = Color(0.64, 0.68, 0.75)
		elif t > 28.0:
			base = Color(0.62, 0.52, 0.34)
			speck = Color(0.54, 0.44, 0.28)
		elif h["light"] < 0.8 * profile["world_light"]:
			base = Color(0.17, 0.23, 0.15)
			speck = Color(0.13, 0.18, 0.11)
	ground_mat.set_shader_parameter("base", Vector3(base.r, base.g, base.b))
	ground_mat.set_shader_parameter("speck", Vector3(speck.r, speck.g, speck.b))


# --- Words -------------------------------------------------------------------------

## Its way of life, from where its food really comes from: [title, one line].
func _niche() -> Array:
	var h: Dictionary = profile["habits"]
	var sun: float = h["light_food"]
	var living: float = h["living_food"]
	var dead: float = h["dead_food"]
	var waste: float = h["waste_food"]
	var mobile: bool = h["movers"] >= 0.5
	var born: int = maxi(profile["ever_born"], 1)
	if h["count"] == 0 or sun + living + dead + waste < 0.01:
		return ["Way of life unknown", "Too few lived long enough to say how it fed."]
	if living >= 0.5:
		if float(profile["kills"]) / born < 0.02:
			return ["Parasite", "Feeds on living bodies, but seldom kills what it feeds on."]
		if mobile:
			return ["Roaming hunter", "Goes after living prey and drains it."]
		return ["Ambush predator", "Sits still and drains whatever comes within reach of its mouth."]
	if dead >= 0.5:
		if mobile:
			return ["Scavenger", "Roams in search of the dead and of food that falls from the sky."]
		return ["Decomposer", "Breaks down the dead where they fall, and frees their matter."]
	if waste >= 0.5:
		return ["Waste recycler", "Lives on what others leave behind, and cleans the ground for them."]
	if sun >= 0.75:
		var mouthed := false
		for part: Dictionary in profile["body"]["parts"]:
			mouthed = mouthed or part["type"] == 1
		if mouthed and float(profile["kills"]) / born >= 0.05:
			return ["Flytrap", "Lives on sunlight, but its mouth finishes off whatever grows too close."]
		if h["poisonous"] >= 0.5:
			return ["Poisonous plant", "Lives on sunlight, and makes whatever drains it pay for the meal."]
		if h["armoured"] >= 0.5:
			return ["Armoured plant", "Lives on sunlight behind plates that blunt the mouths of others."]
		if profile["body"]["canopy_lift"] > 0.2:
			return ["Canopy frond", "Holds its leaves up on stalks to take the light before anything else."]
		if mobile:
			return ["Drifting sun-feeder", "Lives on sunlight, and creeps on to fresh ground."]
		if h["light"] < 0.8 * profile["world_light"]:
			return ["Shade plant", "Makes do with the weak light of shaded ground."]
		return ["Sun-basker", "Sits in the open and lives on sunlight."]
	if sun >= 0.35 and living >= 0.15:
		return ["Mixotroph", "Lives on sunlight, and drains the living when it can."]
	if sun >= 0.35:
		return ["Sun-feeder and recycler", "Lives on sunlight, and on the dead and waste around it."]
	return ["Omnivore", "Takes whatever is going: the living, the dead and the light."]


## What stands out about it: [word, colour] pairs.
func _tags() -> Array:
	var out := []
	var h: Dictionary = profile["habits"]
	var b: Dictionary = profile["body"]
	var r: float = h["radius"] if h["count"] > 0 else b["radius"]
	var ratio: float = r / maxf(profile["world_radius"], 0.01)
	if ratio > 1.35: out.append(["Giant", Color(0.9, 0.75, 0.4)])
	elif ratio > 1.12: out.append(["Large", Color(0.85, 0.75, 0.5)])
	elif ratio < 0.75: out.append(["Tiny", Color(0.6, 0.8, 0.9)])
	elif ratio < 0.9: out.append(["Small", Color(0.65, 0.8, 0.85)])
	if h["armoured"] >= 0.5: out.append(["Armoured", Color(0.7, 0.72, 0.78)])
	if h["poisonous"] >= 0.5: out.append(["Poisonous", Color(0.78, 0.4, 0.95)])
	if h["sighted"] >= 0.5: out.append(["Sighted", Color(0.9, 0.9, 0.9)])
	if h["movers"] >= 0.5:
		out.append(["Fast" if h["speed"] > 0.05 else "Mobile", Color(0.85, 0.7, 0.5)])
	if h["sleepers"] >= 0.5: out.append(["Sleeps at night", Color(0.55, 0.6, 0.9)])
	for part: Dictionary in b["parts"]:
		if part["type"] == 1 and part["stalk"] > 0.25:
			out.append(["Tentacled", Color(0.9, 0.45, 0.45)])
			break
	if b["canopy_lift"] > 0.2: out.append(["Frond", Color(0.5, 0.85, 0.4)])
	if b["throw_range"] > 0.5: out.append(["Seed-flinger", Color(0.75, 0.85, 0.5)])
	if b["detox"] > 0.3: out.append(["Recycler", Color(0.7, 0.75, 0.4)])
	if h["count"] > 0:
		var dt: float = h["temperature"] - profile["world_temperature"]
		if dt < -4.0: out.append(["Cold-hardy", Color(0.6, 0.8, 1.0)])
		elif dt > 4.0: out.append(["Heat-lover", Color(1.0, 0.6, 0.4)])
	if b["offspring_share"] < 0.3: out.append(["Many small young", Color(0.8, 0.8, 0.6)])
	elif b["offspring_share"] > 0.65: out.append(["Few strong young", Color(0.8, 0.8, 0.6)])
	if b["repair"] > 0.3: out.append(["Long-lived", Color(0.7, 0.9, 0.8)])
	if b["mutability"] > 1.0: out.append(["Sloppy copier", Color(0.9, 0.7, 0.7)])
	elif b["mutability"] < -1.0: out.append(["Careful copier", Color(0.7, 0.8, 0.9)])
	if profile["cannibal_kills"] >= 10 and profile["cannibal_kills"] * 2 > profile["kills"]:
		out.append(["Cannibal", Color(0.95, 0.4, 0.4)])
	var deaths: int = profile["died_eaten"] + profile["died_other"]
	if deaths >= 20 and profile["died_eaten"] * 2 >= deaths: out.append(["Hunted", Color(0.95, 0.55, 0.45)])
	if float(profile["kills"]) / maxf(profile["ever_born"], 1.0) >= 0.1: out.append(["Killer", Color(0.95, 0.35, 0.3)])
	if b["pigment"] < 0.2: out.append(["Pale", Color(0.9, 0.88, 0.8)])
	elif b["pigment"] > 0.7: out.append(["Dark", Color(0.55, 0.5, 0.6)])
	return out


func _pill(text: String, color: Color) -> PanelContainer:
	var pill := PanelContainer.new()
	var style := StyleBoxFlat.new()
	style.bg_color = color.darkened(0.72)
	style.border_color = color.darkened(0.3)
	style.set_border_width_all(1)
	style.content_margin_left = 6
	style.content_margin_right = 6
	style.content_margin_top = 1
	style.content_margin_bottom = 2
	pill.add_theme_stylebox_override("panel", style)
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", 11)
	l.add_theme_color_override("font_color", color.lightened(0.2))
	pill.add_child(l)
	return pill


## A short natural history, written from its numbers.
func _story(niche: Array) -> String:
	var h: Dictionary = profile["habits"]
	var b: Dictionary = profile["body"]
	var name: String = profile["name"]
	var alive: bool = profile["extinct"] < 0
	var says := []

	# Who they are and where.
	var words := []
	var ratio: float = (h["radius"] if h["count"] > 0 else b["radius"]) / maxf(profile["world_radius"], 0.01)
	if ratio > 1.25: words.append("large")
	elif ratio < 0.85: words.append("small")
	if b["pigment"] < 0.25: words.append("pale")
	elif b["pigment"] > 0.7: words.append("dark")
	var who := ", ".join(words) + (" " if not words.is_empty() else "") + _plural_role(niche[0])
	var verb := "are" if alive else "were"
	if h["count"] > 0:
		says.append("The %s %s %s of the %s." % [name, verb, who, _habitat()])
	else:
		says.append("The %s %s %s." % [name, verb, who])

	# Where they came from.
	var daughters: int = profile["daughters"]
	var since := ""
	if daughters > 0:
		since = ", and %s since given rise to %s %s of %s own" % ["have" if alive else "had", _number(daughters), "species" if daughters != 1 else "species", "their"]
	if profile["parent_id"] < 0:
		says.append("They are the first of all life, the stock every other kind grew from%s." % since)
	else:
		says.append("They branched from the %s at tick %s%s." % [profile["parent"], _commas(profile["born"]), since])

	# What they are made of.
	says.append(_body_sentence())

	# How they live.
	if h["count"] > 0:
		if h["movers"] >= 0.5:
			says.append("They roam%s." % (", watching the world around them" if h["sighted"] >= 0.5 else " blind, feeling their way"))
		elif h["movers"] > 0.05:
			says.append("A few of them move about; most stay where they were born.")
		if h["sleepers"] >= 0.5:
			says.append("When dark or cold comes they sleep, helpless until it passes.")

	# How they die, and whom they kill.
	var eaten: int = profile["died_eaten"]
	var deaths: int = eaten + int(profile["died_other"])
	var predators: Array = profile["predators"]
	if deaths >= 20:
		var share := float(eaten) / deaths
		var by := (", mostly to the %s" % predators[0]["name"]) if not predators.is_empty() else ""
		if share >= 0.5:
			says.append("Most of them end up eaten: %d%% of all deaths%s." % [roundi(share * 100.0), by])
		elif share >= 0.15:
			says.append("About %d%% of them are eaten%s; the rest run out of energy." % [roundi(share * 100.0), by])
		else:
			says.append("Few are ever eaten; most simply run out of energy.")
	var kills: int = profile["kills"]
	var prey: Array = profile["prey"]
	if kills >= 5:
		var chiefly := (", chiefly %s" % prey[0]["name"]) if not prey.is_empty() else ""
		says.append("Between them they have finished off %s bodies%s." % [_commas(kills), chiefly])

	# The record holders.
	var eldest: Dictionary = profile["eldest"]
	if eldest["value"] > 0:
		if eldest["alive"]:
			says.append("The eldest on record, %s, is still alive at %s ticks." % [eldest["name"], _commas(eldest["value"])])
		else:
			says.append("The eldest on record, %s, lived %s ticks." % [eldest["name"], _commas(eldest["value"])])
	var deadliest: Dictionary = profile["deadliest"]
	if deadliest["value"] >= 3:
		says.append("%s alone killed %s." % [deadliest["name"], _commas(deadliest["value"])])
	if not alive:
		says.append("The last of them died at tick %s." % _commas(profile["extinct"]))
	return " ".join(says)


func _plural_role(title: String) -> String:
	match title:
		"Canopy frond": return "canopy fronds"
		"Drifting sun-feeder": return "drifting sun-feeders"
		"Shade plant": return "shade plants"
		"Sun-basker": return "sun-baskers"
		"Roaming hunter": return "roaming hunters"
		"Ambush predator": return "ambush predators"
		"Parasite": return "parasites"
		"Scavenger": return "scavengers"
		"Decomposer": return "decomposers"
		"Waste recycler": return "waste recyclers"
		"Flytrap": return "flytraps"
		"Poisonous plant": return "poisonous plants"
		"Armoured plant": return "armoured plants"
		"Mixotroph": return "mixotrophs"
		"Sun-feeder and recycler": return "recycling sun-feeders"
		"Omnivore": return "omnivores"
	return "creatures of no settled habit"


## "cold, shaded north-west"
func _habitat() -> String:
	var h: Dictionary = profile["habits"]
	var words := []
	var dt: float = h["temperature"] - profile["world_temperature"]
	words.append("cold" if dt < -3.0 else ("warm" if dt > 3.0 else "temperate"))
	var light: float = h["light"] / maxf(profile["world_light"], 0.01)
	if light > 1.1: words.append("sunlit")
	elif light < 0.85: words.append("shaded")
	return ", ".join(words) + " " + _compass(h["x"], h["z"])


func _compass(x: float, z: float) -> String:
	var ext: float = profile["world_extent"]
	var ns := "north" if z < ext / 3.0 else ("south" if z > ext * 2.0 / 3.0 else "")
	var ew := "west" if x < ext / 3.0 else ("east" if x > ext * 2.0 / 3.0 else "")
	if ns == "" and ew == "":
		return "middle lands"
	if ns == "" or ew == "":
		return ns + ew
	return ns + "-" + ew


## "Each carries a mouth on a tentacle at the front, a tail behind and two fronds."
func _body_sentence() -> String:
	var parts: Array = profile["body"]["parts"]
	if parts.is_empty():
		return "Each is a bare cell with no parts at all."
	var counts := {}
	var order := []
	for part: Dictionary in parts:
		var key := _part_phrase(part)
		if not counts.has(key):
			counts[key] = 0
			order.append(key)
		counts[key] += 1
	var bits := []
	for key: String in order:
		var n: int = counts[key]
		var noun: String = key.get_slice("|", 0)
		var where: String = key.get_slice("|", 1)
		var text := ("an " if noun.begins_with("e") or noun.begins_with("a") else "a ") + noun if n == 1 else _number(n) + " " + _plural_noun(noun)
		if noun in ["armour plates", "poison warts"]:
			text = noun
		bits.append(text + (" " + where if where != "" else ""))
	var joined: String = bits[0] if bits.size() == 1 else ", ".join(bits.slice(0, bits.size() - 1)) + " and " + bits[-1]
	return "Each carries %s." % joined


func _part_phrase(part: Dictionary) -> String:
	var t: int = part["type"]
	var stalk: float = part["stalk"]
	var theta: float = absf(wrapf(part["theta"], -PI, PI))
	var where := "at the front" if theta < 0.8 else ("behind" if theta > 2.35 else "at the side")
	match t:
		0: return ("frond" if stalk > 0.1 else "sun patch") + "|"
		1: return ("mouth on a tentacle" if stalk > 0.25 else "mouth") + "|" + where
		2: return "tail|" + where
		3: return "pair of legs|"
		4: return ("eye on a stalk" if stalk > 0.1 else "eye") + "|" + where
		5: return "armour plates|" + where
		6: return "fat store|"
		7: return "poison warts|"
	return "part|"


func _plural_noun(noun: String) -> String:
	match noun:
		"sun patch": return "sun patches"
		"mouth on a tentacle": return "mouths on tentacles"
		"pair of legs": return "pairs of legs"
		"eye on a stalk": return "eyes on stalks"
		"armour plates", "poison warts": return noun
	return noun + "s"


func _number(n: int) -> String:
	return NUMBERS[n] if n < NUMBERS.size() else _commas(n)


# --- Sections ----------------------------------------------------------------------

func _diet_shares() -> Array:
	var h: Dictionary = profile["habits"]
	return [h["light_food"], h["living_food"], h["dead_food"], h["waste_food"]]


func _draw_diet() -> void:
	if profile.is_empty():
		return
	var w := diet_bar.size.x
	var shares := _diet_shares()
	var total := 0.0
	for s: float in shares:
		total += s
	diet_bar.draw_rect(Rect2(0, 0, w, 14), Color(1, 1, 1, 0.05))
	if total <= 0.0:
		return
	var x := 0.0
	for k in 4:
		var seg := roundf(w * shares[k] / total)
		if k == 3:
			seg = w - x
		if seg > 0.0:
			diet_bar.draw_rect(Rect2(x, 0, seg, 14), DIET_COLORS[k])
			diet_bar.draw_rect(Rect2(x, 0, seg, 3), DIET_COLORS[k].lightened(0.25))
		x += seg


func _diet_legend() -> String:
	var shares := _diet_shares()
	var total := 0.0
	for s: float in shares:
		total += s
	if total <= 0.0:
		return "[color=#%s]No meals counted yet.[/color]" % FAINT.to_html(false)
	var bits := []
	for k in 4:
		var pct := roundi(100.0 * shares[k] / total)
		if pct > 0:
			# Non-breaking spaces keep each entry on one line.
			bits.append(("[color=#%s]██[/color] %s %d%%" % [DIET_COLORS[k].to_html(false), DIET_WORDS[k], pct]).replace(" ", "\u00a0"))
	return "   ".join(bits)


func _body() -> String:
	var b: Dictionary = profile["body"]
	var faint := FAINT.to_html(false)
	var lines := []
	var shape := _shape_words(b["elongation"], b["squareness"])
	lines.append("[color=#%s]Shape[/color]  %s, radius %.2f, mass %.2f" % [faint, shape, b["radius"], b["mass"]])
	var parts: Array = b["parts"]
	if parts.is_empty():
		lines.append("[color=#%s]Parts[/color]  none" % faint)
	else:
		lines.append("[color=#%s]Parts[/color]" % faint)
		for part: Dictionary in parts:
			var t: int = part["type"]
			var text := "   [color=#%s]██[/color] %s %.2f, %s" % [PART_COLORS[t].to_html(false), part["name"], part["size"], _side(part["theta"])]
			if part["stalk"] > 0.0:
				text += ", on a %.2f stalk" % part["stalk"]
			lines.append(text)
	var moves := "never moves" if b["max_speed"] <= 0.0 else "moves up to %.3f a tick" % b["max_speed"]
	var senses := "blind" if b["sense_cells"] == 0 else ("senses %d cell around" if b["sense_cells"] == 1 else "senses %d cells around") % b["sense_cells"]
	lines.append("[color=#%s]Lives[/color]  %s, %s" % [faint, moves, senses])
	var mind := "no brain"
	if b["brain_neurons"] >= 0:
		mind = "reflexes only (%d links)" % b["brain_links"] if b["brain_neurons"] == 0 else "a brain of %d neurons and %d links" % [b["brain_neurons"], b["brain_links"]]
	lines.append("[color=#%s]Mind[/color]  %s" % [faint, mind])
	lines.append("[color=#%s]Comfort[/color]  %.0f ± %.0f °C" % [faint, b["thermal_optimum"], b["thermal_tolerance"]])
	lines.append("[color=#%s]Young[/color]  divides at %.0f energy, gives each child %d%%%s" % [faint, b["repro_threshold"], roundi(100.0 * b["offspring_share"]),
			", flung up to %.1f cells" % b["throw_range"] if b["throw_range"] > 0.05 else ""])
	var eats := []
	if b["bite"] > 0.0: eats.append("bites ×%.1f" % (1.0 + b["bite"]))
	eats.append("keeps %d%% of a meal" % roundi(100.0 * b["efficiency"]))
	if b["detox"] > 0.0: eats.append("eats waste (%.2f)" % b["detox"])
	if b["toxicity"] > 0.0: eats.append("poisonous (%.2f)" % b["toxicity"])
	lines.append("[color=#%s]Feeding[/color]  %s" % [faint, ", ".join(eats)])
	var keep := []
	keep.append("upkeep %.2f a tick" % b["upkeep"])
	if b["dormancy"] > 0.0: keep.append("sleeps to %d%%" % roundi(100.0 * b["dormancy"]))
	if b["repair"] > 0.0: keep.append("ages %.1f× slower" % (1.0 + 2.0 * b["repair"]))
	var m: float = b["mutability"]
	if absf(m) >= 0.1: keep.append("copied with %s the usual mistakes" % ("%.1f×" % pow(2.0, m) if m > 0.0 else "1/%.1f of" % pow(2.0, -m)))
	lines.append("[color=#%s]Upkeep[/color]  %s" % [faint, ", ".join(keep)])
	lines.append("[color=#%s]Drawn from the %s.[/color]" % [faint, "eldest living member" if b["from"] == "eldest" else "founder's blueprint"])
	return "\n".join(lines)


func _side(theta: float) -> String:
	const SIDES := ["front", "front-left", "left", "back-left", "back", "back-right", "right", "front-right"]
	return SIDES[int(roundf(wrapf(theta, 0.0, TAU) / (PI / 4.0))) % 8]


func _shape_words(e: float, sq: float) -> String:
	var length := "round" if e < 0.15 else ("oval" if e < 0.5 else "long")
	if sq >= 0.5:
		length = "square" if e < 0.15 else ("boxy" if e < 0.5 else "a long box")
	return length


func _records() -> void:
	for child in records.get_children():
		records.remove_child(child)
		child.queue_free()
	var rows := []
	rows.append(["Eldest ever", _holder(profile["eldest"], "ticks")])
	rows.append(["Most kills by one", _holder(profile["deadliest"], "kills")])
	rows.append(["Most young by one", _holder(profile["most_children"], "young")])
	rows.append(["Born in all", _commas(profile["ever_born"])])
	rows.append(["Most alive at once", _commas(profile["peak"])])
	var eaten: int = profile["died_eaten"]
	var other: int = profile["died_other"]
	var deaths := maxi(eaten + other, 1)
	rows.append(["Died eaten", "%s  (%d%%)" % [_commas(eaten), roundi(100.0 * eaten / deaths)]])
	rows.append(["Died otherwise", "%s  (%d%%)" % [_commas(other), roundi(100.0 * other / deaths)]])
	rows.append(["Bodies it finished off", _commas(profile["kills"])])
	rows.append(["Daughter species", _commas(profile["daughters"])])
	for row: Array in rows:
		var k := _label(row[0], 12, FAINT)
		k.autowrap_mode = TextServer.AUTOWRAP_OFF
		records.add_child(k)
		var v := _label(row[1], 12, INK)
		v.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		records.add_child(v)


func _holder(r: Dictionary, unit: String) -> String:
	if r["value"] <= 0:
		return "none yet"
	return "%s, %s %s%s" % [r["name"], _commas(r["value"]), unit, "  (alive)" if r["alive"] else ""]


func _web() -> String:
	var faint := FAINT.to_html(false)
	var lines := []
	var prey: Array = profile["prey"]
	var predators: Array = profile["predators"]
	lines.append("[color=#%s]Kills[/color]" % faint)
	if prey.is_empty():
		lines.append("   nothing yet")
	for e: Dictionary in prey:
		lines.append("   " + _link(e) + "  %s" % _commas(e["kills"]))
	if profile["prey_species"] > prey.size():
		lines.append("   [color=#%s]and %d more species[/color]" % [faint, profile["prey_species"] - prey.size()])
	lines.append("[color=#%s]Killed by[/color]" % faint)
	if predators.is_empty():
		lines.append("   nothing yet")
	for e: Dictionary in predators:
		lines.append("   " + _link(e) + "  %s" % _commas(e["kills"]))
	if profile["predator_species"] > predators.size():
		lines.append("   [color=#%s]and %d more species[/color]" % [faint, profile["predator_species"] - predators.size()])
	if profile["cannibal_kills"] > 0:
		lines.append("[color=#%s]Its own kind[/color]  %s killed" % [faint, _commas(profile["cannibal_kills"])])
	return "\n".join(lines)


func _link(e: Dictionary) -> String:
	var col := _hue(e["hue"]) if e["alive"] else Color(0.55, 0.55, 0.6)
	return "[url=%d][color=#%s]██ %s[/color][/url]%s" % [e["id"], col.to_html(false), e["name"], "" if e["alive"] else " †"]


func _range() -> String:
	var h: Dictionary = profile["habits"]
	var faint := FAINT.to_html(false)
	if h["count"] == 0:
		return "[color=#%s]Never counted at a census.[/color]" % faint
	var lines := []
	var when := "" if profile["extinct"] < 0 else "Last seen at the census of tick %s.\n" % _commas(h["tick"])
	lines.append(when + "Lives around the %s, within about %d cells of (%d, %d)." % [_compass(h["x"], h["z"]), roundi(h["spread"]), roundi(h["x"]), roundi(h["z"])])
	lines.append("[color=#%s]Ground[/color]  %.0f °C (world %.0f °C), noon light %.2f (world %.2f)" % [faint, h["temperature"], profile["world_temperature"], h["light"], profile["world_light"]])
	lines.append("[color=#%s]Members[/color]  %d%% move, %d%% see, %d%% sleep, %d%% armoured, %d%% poisonous; mean age %s" % [faint,
			roundi(100.0 * h["movers"]), roundi(100.0 * h["sighted"]), roundi(100.0 * h["sleepers"]),
			roundi(100.0 * h["armoured"]), roundi(100.0 * h["poisonous"]), _commas(roundi(h["age"]))])
	return "\n".join(lines)


# --- Small things ------------------------------------------------------------------

## The species colour, as blob.gdshader and the tree paint it.
func _hue(hue: float) -> Color:
	return Color.from_hsv(hue, 0.62, 0.92)


func _commas(value: int) -> String:
	var s := str(absi(value))
	var i := s.length() - 3
	while i > 0:
		s = s.insert(i, ",")
		i -= 3
	return ("-" if value < 0 else "") + s
