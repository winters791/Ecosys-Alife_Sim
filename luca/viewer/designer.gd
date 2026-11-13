extends Control
## The creature designer: a screen of its own for building a creature by hand,
## testing it, saving it to the seed vault and dropping it into the world.
##
##   Body       the outline (pen), parts stamped on the rim, sizes, stalks
##   Brain      the evolved CTRNN as a graph: wire senses to urges, try it
##   Chemistry  its enzymes: what it can burn, and so what it can live on
##   Life       when it breeds, how big its young are, comfort, sleep
##   Test tank  a small walled-off world: watch twelve of them try to live
##
## The energy budget on the right is the game: income (the sun, as the real
## light rules give it) against the upkeep bill (from the real phenotype
## compiler, line by line). Nothing here is a rule of its own; every number
## comes from LucaWorld (core/design.cpp).
##
## A design is only a genome. Dropped into the world it is an invasive species
## under the world's physics (design doc section 5): it pays its costs, and
## evolution takes over from the first child.
##
## The seed vault is a folder of creature files (user://creatures): designs
## saved here, and species banked from the world (a sample of up to 20 living
## members, 4.9). Any of them can be opened, edited or released again.
##
## Keys: Esc close · Cmd/Ctrl+Z undo · Shift+Cmd/Ctrl+Z redo · Delete removes
## what is picked · V select · P pen · M mirror · 1-8 stamp a part (Body tab)

signal closed
signal release_requested(text: String, name: String, count: int)
signal message(text: String)

const VAULT_DIR := "user://creatures"
const TAB_NAMES := ["Body", "Brain", "Chemistry", "Life", "Test tank"]
const PART_NAMES := ["Sun patch", "Mouth", "Tail", "Legs", "Eye", "Armour", "Fat store", "Poison"]
const PART_BLURBS := [
	"Catches light, if it has open skin to sit on. Bigger covers more of the rim. Folds shut while the body moves. On a stalk it stands above its neighbours in the canopy.",
	"Drains whatever it touches within its arc (red). Bigger: a wider arc and a harder pull. On a stalk it reaches further and armour never clogs it.",
	"Pushes the body along, best from straight behind (a quarter as well from the front). Needs a brain to drive it.",
	"A pair of legs, mirrored on both sides. Push best at the sides. Heavier than a tail. Needs a brain to drive them.",
	"Lets the brain feel which way things lie: light, the living, the dead, kin, bigger bodies. Bigger sees further.",
	"Plates over the arc it covers: bodies there are harder to drain. Heavy, and it clogs mouths and tails beneath it.",
	"Holds more energy: a bigger store survives nights and winters. Heavy.",
	"Whatever drains this body loses energy for every unit it takes. Light but dear to keep.",
]
const CHEM_NAMES := ["A", "B", "C", "D"]
const CHEM_BLURB := ["spent: the soil's matter", "a little energy", "made by vents", "rich: plants and bodies"]
const INK := Color(0.93, 0.94, 0.96)
const FAINT := Color(0.58, 0.63, 0.71)
const GOOD := Color(0.45, 0.88, 0.50)
const OK := Color(0.98, 0.82, 0.35)
const BAD := Color(0.98, 0.42, 0.38)
const PANEL_BG := Color(0.075, 0.083, 0.105)
const SCREEN_BG := Color(0.045, 0.05, 0.068)
const GUEST := Color(0.98, 0.62, 0.22)
const NATIVE := Color(0.35, 0.80, 0.78)
const TANK_SPEEDS := [10, 40, 160, 1000]

var world: Object
var layout: Dictionary = {}
var genome: Dictionary = {}
var design_name := "Unnamed"
var origin := "designed"
var report: Dictionary = {}
var sun := 1.0
var air := 20.0
var tab := 0
var templates: Array = []

# Undo: snapshots of the genome before each change.
var undo_stack: Array = []
var redo_stack: Array = []
var committed: Dictionary = {}
var last_merge := ""
var last_merge_time := 0.0

# Built UI.
var name_edit: LineEdit
var template_pick: OptionButton
var count_spin: SpinBox
var status_label: Label
var tab_buttons: Array[Button] = []
var left_pages: Array[Control] = []
var center_pages: Array[Control] = []
var canvas: Control
var brain: Control
var chem_list: VBoxContainer
var tool_buttons: Dictionary = {}
var mirror_check: CheckBox
var part_box: VBoxContainer
var part_title: Label
var part_blurb: Label
var part_size: HSlider
var part_angle: HSlider
var part_stalk: HSlider
var part_stalk_label: Label
var part_size_label: Label
var part_angle_label: Label
var bindings: Array = []   # {slider, label, get: Callable, text: Callable}
var link_box: VBoxContainer
var link_title: Label
var link_weight: HSlider
var link_weight_label: Label
var neuron_box: VBoxContainer
var neuron_title: Label
var neuron_tau: HSlider
var neuron_tau_label: Label
var neuron_bias: HSlider
var neuron_bias_label: Label
var probe_sliders: Array[HSlider] = []
var probe_ticks := 60
var probe_inputs := PackedFloat32Array()
var verdict_label: Label
var verdict_sub: Label
var budget_bars: Control
var costs_text: RichTextLabel
var stats_text: RichTextLabel
var warn_text: RichTextLabel
var role_label: Label
var library: PanelContainer
var library_list: VBoxContainer
var chem_note: RichTextLabel

# The test tank.
var tank_stage: SubViewportContainer
var tank_chart: Control
var tank_info: RichTextLabel
var tank_running := false
var tank_speed := 1
var tank_history: Array = []    # [tick, guests, natives]
var tank_count: SpinBox
var tank_luca: CheckBox
var tank_capacity := 0
var tank_extent := 32.0
var tank_seed := 1


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_STOP
	layout = world.get_brain_layout()
	templates = world.get_design_templates()
	probe_inputs.resize((layout["inputs"] as PackedStringArray).size())
	probe_inputs[0] = 1.0
	probe_inputs[1] = 0.5
	probe_inputs[2] = 0.5
	_build()
	load_genome(templates[0]["genome"], "Unnamed", "designed")


# --- Opening and closing -----------------------------------------------------------

## Open on a genome (a creature from the world), or on what was there before.
func open(with_genome: Dictionary = {}, title := "") -> void:
	visible = true
	if not with_genome.is_empty():
		load_genome(with_genome, title if title != "" else "Unnamed", "edited from a creature in the world")
	_show_tab(tab)
	await get_tree().process_frame
	canvas.fit()


func close() -> void:
	if library.visible:
		library.visible = false
		return
	visible = false
	tank_running = false
	world.trial_stop()
	closed.emit()


func load_genome(g: Dictionary, title: String, from: String) -> void:
	genome = world.clamp_design(g)
	design_name = title
	origin = from
	if name_edit != null:
		name_edit.text = title
	undo_stack.clear()
	redo_stack.clear()
	committed = genome.duplicate(true)
	canvas.select(-1)
	brain.sel_kind = ""
	brain.sel_index = -1
	_after_change(true)
	if is_inside_tree():
		canvas.fit()


# --- Building the screen -----------------------------------------------------------

func _build() -> void:
	var bg := ColorRect.new()
	bg.color = SCREEN_BG
	bg.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(bg)
	var root := VBoxContainer.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.offset_left = 12
	root.offset_top = 10
	root.offset_right = -12
	root.offset_bottom = -10
	root.add_theme_constant_override("separation", 8)
	add_child(root)

	# Top bar.
	var bar := HBoxContainer.new()
	bar.add_theme_constant_override("separation", 8)
	root.add_child(bar)
	var title := _label("CREATURE DESIGNER", 16, INK)
	bar.add_child(title)
	name_edit = LineEdit.new()
	name_edit.custom_minimum_size.x = 170
	name_edit.placeholder_text = "Name it"
	name_edit.max_length = 32
	name_edit.text_changed.connect(func(t: String) -> void: design_name = t)
	bar.add_child(name_edit)
	template_pick = OptionButton.new()
	template_pick.focus_mode = Control.FOCUS_NONE
	for t: Dictionary in templates:
		template_pick.add_item(t["name"])
	template_pick.item_selected.connect(func(i: int) -> void:
		load_genome(templates[i]["genome"], design_name if design_name != "Unnamed" else str(templates[i]["name"]), "designed")
		_status("Started from the %s template." % templates[i]["name"]))
	bar.add_child(template_pick)
	_button(bar, "Undo", _undo)
	_button(bar, "Redo", _redo)
	_button(bar, "Seed vault…", _open_library)
	_button(bar, "Save to vault", _save)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	bar.add_child(spacer)
	status_label = _label("", 12, FAINT)
	status_label.autowrap_mode = TextServer.AUTOWRAP_OFF
	status_label.clip_text = true
	status_label.custom_minimum_size.x = 80
	status_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	bar.add_child(status_label)
	count_spin = SpinBox.new()
	count_spin.min_value = 1
	count_spin.max_value = 100
	count_spin.value = layout.get("vault_sample", 20)
	count_spin.prefix = "×"
	count_spin.tooltip_text = "How many to drop (section 5: about 20 keeps some variety)"
	bar.add_child(count_spin)
	var release := _button(bar, "Release ▸", _release)
	release.add_theme_color_override("font_color", GUEST)
	_button(bar, "Close", close).tooltip_text = "Back to the world (Esc)"

	var body := HBoxContainer.new()
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	body.add_theme_constant_override("separation", 8)
	root.add_child(body)

	# Left: the tools for the open tab.
	var left_scroll := _panel_scroll(body, 300)
	var left := VBoxContainer.new()
	left.custom_minimum_size.x = 276
	left.add_theme_constant_override("separation", 6)
	left_scroll.get_child(0).add_child(left)

	# Middle: tabs over the view.
	var middle := VBoxContainer.new()
	middle.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	middle.add_theme_constant_override("separation", 0)
	body.add_child(middle)
	var tabs := HBoxContainer.new()
	tabs.add_theme_constant_override("separation", 2)
	middle.add_child(tabs)
	for i in TAB_NAMES.size():
		var b := _button(tabs, "  %s  " % TAB_NAMES[i], _show_tab.bind(i), true)
		tab_buttons.append(b)
	var view := PanelContainer.new()
	view.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var vs := StyleBoxFlat.new()
	vs.bg_color = PANEL_BG
	view.add_theme_stylebox_override("panel", vs)
	middle.add_child(view)

	# Right: the energy budget, always in view.
	var right_scroll := _panel_scroll(body, 330)
	var right := VBoxContainer.new()
	right.custom_minimum_size.x = 306
	right.add_theme_constant_override("separation", 6)
	right_scroll.get_child(0).add_child(right)
	_build_budget(right)

	# Pages.
	canvas = Control.new()
	canvas.set_script(load("res://design_canvas.gd"))
	canvas.world = world
	canvas.layout = layout
	canvas.edited.connect(_edited)
	canvas.selection_changed.connect(func(_k: int) -> void: _sync_part_panel())
	brain = Control.new()
	brain.set_script(load("res://brain_graph.gd"))
	brain.layout = layout
	brain.edited.connect(_edited)
	brain.selection_changed.connect(func(_k: String, _i: int) -> void: _sync_brain_panel())
	var chem_page := _scroll_page()
	chem_list = VBoxContainer.new()
	chem_list.add_theme_constant_override("separation", 8)
	chem_list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	chem_page.get_child(0).add_child(chem_list)
	var life_page := _scroll_page()
	var tank_page := VBoxContainer.new()
	for page: Control in [canvas, brain, chem_page, life_page, tank_page]:
		page.size_flags_vertical = Control.SIZE_EXPAND_FILL
		view.add_child(page)
		center_pages.append(page)

	left_pages.append(_build_body_tools(left))
	left_pages.append(_build_brain_tools(left))
	left_pages.append(_build_chem_tools(left))
	left_pages.append(_build_life_help(left))
	left_pages.append(_build_tank_tools(left))
	_build_life(life_page.get_child(0))
	_build_tank_view(tank_page)
	_build_library()


func _build_body_tools(parent: VBoxContainer) -> VBoxContainer:
	var box := _page(parent)
	_heading(box, "TOOLS")
	var row := HBoxContainer.new()
	box.add_child(row)
	tool_buttons["select"] = _button(row, "Select (V)", _set_tool.bind("select"), true)
	tool_buttons["pen"] = _button(row, "Pen (P)", _set_tool.bind("pen"), true)
	mirror_check = CheckBox.new()
	mirror_check.text = "Mirror (M)"
	mirror_check.button_pressed = true
	mirror_check.focus_mode = Control.FOCUS_NONE
	mirror_check.toggled.connect(func(on: bool) -> void: canvas.mirror = on)
	row.add_child(mirror_check)
	box.add_child(_note("Pen: pull the outline (nose for length, sides for width, a corner for boxiness); wheel for size."))

	_heading(box, "PARTS  (click one, then click the rim)")
	var grid := GridContainer.new()
	grid.columns = 2
	box.add_child(grid)
	for t in PART_NAMES.size():
		var b := _button(grid, "%d  %s" % [t + 1, PART_NAMES[t]], _set_stamp.bind(t), true)
		b.icon = _swatch(canvas.PART_COLORS[t])
		b.alignment = HORIZONTAL_ALIGNMENT_LEFT
		b.custom_minimum_size.x = 134
		tool_buttons["stamp%d" % t] = b

	part_box = VBoxContainer.new()
	part_box.add_theme_constant_override("separation", 4)
	box.add_child(part_box)
	_heading(part_box, "PICKED PART")
	part_title = _label("", 14, INK)
	part_box.add_child(part_title)
	part_blurb = _note("")
	part_box.add_child(part_blurb)
	part_size_label = _label("", 12, INK)
	part_box.add_child(part_size_label)
	part_size = _bare_slider(part_box, layout["min_part"], 2.5, 0.05, func(v: float) -> void: _set_part("size", v))
	part_angle_label = _label("", 12, INK)
	part_box.add_child(part_angle_label)
	part_angle = _bare_slider(part_box, -180, 180, 1, func(v: float) -> void: _set_part("theta", deg_to_rad(v)))
	part_stalk_label = _label("", 12, INK)
	part_box.add_child(part_stalk_label)
	part_stalk = _bare_slider(part_box, 0.0, layout["max_stalk"], 0.02, func(v: float) -> void: _set_part("stalk", v))
	_button(part_box, "Remove it (Delete)", func() -> void: canvas.remove_part(canvas.selected))

	_heading(box, "BODY")
	_bind_slider(box, "Size", layout["min_radius"], 3.0, 0.02, "radius", func(v: float) -> String: return "%.2f" % v)
	_bind_slider(box, "Length", 0.0, 1.0, 0.01, "elongation", func(v: float) -> String: return "%.1f × as long as wide" % (1.0 + 2.0 * v))
	_bind_slider(box, "Boxiness", 0.0, 1.0, 0.01, "squareness", func(v: float) -> String: return "%d%%" % roundi(v * 100.0))
	_bind_slider(box, "Skin darkness", 0.0, 1.0, 0.01, "pigment", func(v: float) -> String: return "pale" if v < 0.3 else ("dark" if v > 0.7 else "tan"))
	_bind_slider(box, "Skin hue", 0.0, 1.0, 0.01, "hue", func(v: float) -> String: return _hue_word(v))
	_bind_shape_slider(box, "Lumps", 0)
	_bind_shape_slider(box, "Ripples", 1)
	box.add_child(_note("Dark skin warms in the sun (good in the cold). Skin that matches the ground is harder for others to sense; moving gives it away. Length and boxiness add skin: more room for sun patches, more heat lost. A long body moves more cheaply."))
	return box


func _build_brain_tools(parent: VBoxContainer) -> VBoxContainer:
	var box := _page(parent)
	_heading(box, "REFLEXES  (add a ready-made wiring)")
	var grid := GridContainer.new()
	grid.columns = 2
	box.add_child(grid)
	var cards := [
		["Wander", "Go somewhere, keep going", [["bias", "wander", 1.0], ["bias", "inertia", 2.0], ["bias", "throttle", -0.3]]],
		["Hunt", "Lean towards the living", [["bias", "biomass", 1.5], ["bias", "inertia", 0.5]]],
		["Stop to eat", "Slow down while eating", [["eating", "throttle", -1.5]]],
		["Run when bitten", "Speed up and turn from big ones", [["hurt", "throttle", 1.5], ["hurt", "bigger", -1.5]]],
		["Seek light", "Lean towards the bright", [["bias", "light", 1.5]]],
		["Scavenge", "Lean towards the dead", [["bias", "corpse", 1.5]]],
		["Avoid big ones", "Keep away from bigger bodies", [["bias", "bigger", -1.0]]],
		["Herd", "Stay near its own kind", [["bias", "kin", 1.0]]],
		["Keep comfy", "Lean towards its comfort", [["bias", "comfort", 1.0]]],
		["Roam when hungry", "Hunger drives it on", [["hunger", "throttle", 1.0], ["hunger", "wander", 1.0]]],
	]
	for c: Array in cards:
		var b := _button(grid, c[0], _add_reflex.bind(c[2]))
		b.tooltip_text = c[1]
		b.custom_minimum_size.x = 134
	var row := HBoxContainer.new()
	box.add_child(row)
	_button(row, "+ Neuron", func() -> void: brain.add_neuron())
	_button(row, "Clear brain", func() -> void:
		genome["links"] = []
		genome["hidden"] = 0
		genome["tau"] = []
		genome["bias"] = []
		brain.sel_kind = ""
		_edited(""))
	box.add_child(_note("Drag from a sense (or neuron) to an urge (or neuron) to wire it. Wheel on a picked link sets its weight; right-click removes. Urges marked 👁 need an eye."))

	link_box = VBoxContainer.new()
	box.add_child(link_box)
	_heading(link_box, "PICKED LINK")
	link_title = _label("", 13, INK)
	link_title.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	link_box.add_child(link_title)
	link_weight_label = _label("", 12, INK)
	link_box.add_child(link_weight_label)
	link_weight = _bare_slider(link_box, -layout["max_weight"], layout["max_weight"], 0.05, func(v: float) -> void:
		var links: Array = genome["links"]
		if brain.sel_kind == "link" and brain.sel_index < links.size():
			links[brain.sel_index]["weight"] = v
			_edited("weight%d" % brain.sel_index))
	_button(link_box, "Remove link (Delete)", func() -> void: brain.remove_link(brain.sel_index))

	neuron_box = VBoxContainer.new()
	box.add_child(neuron_box)
	_heading(neuron_box, "PICKED NEURON")
	neuron_title = _label("", 13, INK)
	neuron_box.add_child(neuron_title)
	neuron_tau_label = _label("", 12, INK)
	neuron_box.add_child(neuron_tau_label)
	# Tau on a log scale: 1 tick (instant) to 1,000 (a memory over days).
	neuron_tau = _bare_slider(neuron_box, 0.0, 3.0, 0.01, func(v: float) -> void:
		if brain.sel_kind == "neuron":
			genome["tau"][brain.sel_index] = pow(10.0, v)
			_edited("tau%d" % brain.sel_index))
	neuron_bias_label = _label("", 12, INK)
	neuron_box.add_child(neuron_bias_label)
	neuron_bias = _bare_slider(neuron_box, -layout["max_weight"], layout["max_weight"], 0.05, func(v: float) -> void:
		if brain.sel_kind == "neuron":
			genome["bias"][brain.sel_index] = v
			_edited("bias%d" % brain.sel_index))
	neuron_box.add_child(_note("Slow neurons (high τ) remember: they smooth what they feel and hold it for about τ ticks. A day is %d ticks." % layout.get("day_length", 600)))
	_button(neuron_box, "Remove neuron (Delete)", func() -> void: brain.remove_neuron(brain.sel_index))

	_heading(box, "TRY IT  (set its senses, see its urges)")
	var inputs: PackedStringArray = layout["inputs"]
	for i in range(1, inputs.size()):
		var row2 := HBoxContainer.new()
		box.add_child(row2)
		var l := _label(brain.INPUT_LABELS[i], 12, INK)
		l.custom_minimum_size.x = 92
		row2.add_child(l)
		var s := HSlider.new()
		s.min_value = -1.0 if inputs[i] == "cold" else 0.0
		s.max_value = 1.0
		s.step = 0.05
		s.value = probe_inputs[i]
		s.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		s.focus_mode = Control.FOCUS_NONE
		s.value_changed.connect(func(v: float) -> void:
			probe_inputs[i] = v
			_probe())
		row2.add_child(s)
		probe_sliders.append(s)
	var held := _label("Held for %d ticks" % probe_ticks, 12, INK)
	box.add_child(held)
	_bare_slider(box, 1, 600, 1, func(v: float) -> void:
		probe_ticks = int(v)
		held.text = "Held for %d ticks" % probe_ticks
		_probe()).value = probe_ticks
	return box


func _build_chem_tools(parent: VBoxContainer) -> VBoxContainer:
	var box := _page(parent)
	_heading(box, "CHEMICALS")
	var levels: PackedFloat32Array = layout["levels"]
	for c in CHEM_NAMES.size():
		box.add_child(_label("%s   %3d a unit   %s" % [CHEM_NAMES[c], int(levels[c]), CHEM_BLURB[c]], 12, INK))
	box.add_child(_note("An enzyme turns one chemical into another, either way. Running downhill (to a poorer chemical) it releases the difference: that is how a body pays its upkeep. Running uphill it stores light (a reaction from A takes A from the soil). Every enzyme costs upkeep for its rate, used or not."))
	box.add_child(_note("A body can burn a chemical if one of its enzymes has it on the richer side. It takes those up from its cell, keeps them from meals, and gives off the rest. Plants and bodies are made of D; vents make C."))
	_heading(box, "QUICK ADD")
	var quick := [["Photosynthesis  A ⇄ D", [0, 3, -1, -1, 0.015]], ["Vent eater  A ⇄ C", [0, 2, -1, -1, 0.02]],
		["Burn B  A ⇄ B", [0, 1, -1, -1, 0.02]], ["Double  A+A ⇄ D+D", [0, 3, 0, 3, 0.01]]]
	for q: Array in quick:
		_button(box, q[0], _add_enzyme.bind(q[1]))
	chem_note = _rich(12)
	box.add_child(chem_note)
	return box


func _build_life_help(parent: VBoxContainer) -> VBoxContainer:
	var box := _page(parent)
	_heading(box, "LIFE CYCLE")
	box.add_child(_note("These genes have no part to draw; they shape how it lives and breeds. Each costs something: a wide comfort range, bite, sleep machinery, and flinging seeds all add upkeep."))
	box.add_child(_note("Breeding: when its store reaches the threshold it builds a child (at birth size) out of its store and hands it a share of what is left. Many small young, or a few big ones, is up to you."))
	box.add_child(_note("Evolution starts at the first child."))
	return box


func _build_life(page: Control) -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 4)
	box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	page.add_child(box)
	var margin := 16
	box.add_theme_constant_override("separation", 5)
	var grid := GridContainer.new()
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", 28)
	grid.add_theme_constant_override("v_separation", 10)
	grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var pad := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		pad.add_theme_constant_override("margin_" + side, margin)
	pad.add_child(grid)
	box.add_child(pad)
	var cell := func(title: String, help: String) -> VBoxContainer:
		var v := VBoxContainer.new()
		v.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		v.custom_minimum_size.x = 300
		v.add_child(_label(title, 13, Color(0.75, 0.82, 0.92)))
		var h := _note(help)
		v.add_child(h)
		grid.add_child(v)
		return v
	_bind_slider(cell.call("BREEDING", "Divides once its store holds this much. More: fewer, better-provisioned young."),
		"Divides at", layout["min_threshold"], 1500.0, 5.0, "repro_threshold", func(v: float) -> String: return "%.0f energy" % v)
	_bind_slider(cell.call("CHILD'S SHARE", "Of what is left after building the child's body, the share handed over."),
		"Gives", 0.05, 0.95, 0.01, "offspring_share", func(v: float) -> String: return "%d%%" % roundi(v * 100.0))
	_bind_slider(cell.call("BORN AT", "Small young are cheap but easy to eat and catch little light until grown."),
		"Born at", layout["min_birth"], 1.0, 0.01, "birth_size", func(v: float) -> String: return "%d%% of adult mass" % roundi(v * 100.0))
	_bind_slider(cell.call("SEED THROW", "How far it can fling a child, beyond touching. Upkeep per cell of range, and each throw costs energy."),
		"Throws up to", 0.0, layout["max_throw"], 0.1, "seed_throw", func(v: float) -> String: return "%.1f cells" % v)
	_bind_slider(cell.call("COMFORT", "The temperature it is cheapest to live at."),
		"Comfortable at", -20.0, 50.0, 0.5, "thermal_optimum", func(v: float) -> String: return "%.1f °C" % v)
	_bind_slider(cell.call("TOLERANCE", "How far from comfort it can go without paying for heat. Wide ranges cost upkeep."),
		"Tolerates ±", 0.5, 30.0, 0.5, "thermal_tolerance", func(v: float) -> String: return "%.1f °C" % v)
	_bind_slider(cell.call("BITE", "Drains the living faster: up to four times the plain pull. Upkeep whether or not it bites."),
		"Bite", 0.0, layout["max_bite"], 0.05, "bite", func(v: float) -> String: return "×%.2f pull" % (1.0 + v))
	_bind_slider(cell.call("DORMANCY", "Sleeps through dark and cold, paying a tenth of its upkeep, but helpless, and slow to wake."),
		"Sleeps", 0.0, 1.0, 0.01, "dormancy", func(v: float) -> String: return "not at all" if v <= 0.0 else "%d%% deep" % roundi(v * 100.0))


func _build_tank_tools(parent: VBoxContainer) -> VBoxContainer:
	var box := _page(parent)
	_heading(box, "TEST TANK")
	box.add_child(_note("A small flat world of its own (32 × 32 cells) with the sun and air of the Habitat on the right. Your design lands at the centre by meteor and lives by the world's rules. Nothing here touches the real world."))
	var row := HBoxContainer.new()
	box.add_child(row)
	row.add_child(_label("Drop", 12, INK))
	tank_count = SpinBox.new()
	tank_count.min_value = 1
	tank_count.max_value = 60
	tank_count.value = 12
	row.add_child(tank_count)
	tank_luca = CheckBox.new()
	tank_luca.text = "with wild LUCAs"
	tank_luca.button_pressed = true
	tank_luca.focus_mode = Control.FOCUS_NONE
	tank_luca.tooltip_text = "Plants to compete with for light, or to eat"
	box.add_child(tank_luca)
	var run := HBoxContainer.new()
	box.add_child(run)
	_button(run, "▶ Run test", _start_tank)
	_button(run, "Pause", func() -> void: tank_running = not tank_running)
	_button(run, "New seed", func() -> void:
		tank_seed += 1
		_start_tank())
	_heading(box, "SPEED")
	var speeds := HBoxContainer.new()
	box.add_child(speeds)
	for i in TANK_SPEEDS.size():
		_button(speeds, ["1x", "4x", "16x", "Max"][i], func() -> void: tank_speed = i)
	tank_info = _rich(13)
	box.add_child(tank_info)
	return box


func _build_tank_view(page: VBoxContainer) -> void:
	page.add_theme_constant_override("separation", 0)
	tank_stage = SubViewportContainer.new()
	tank_stage.set_script(load("res://sprite_stage.gd"))
	tank_stage.size_flags_vertical = Control.SIZE_EXPAND_FILL
	tank_stage.size_flags_stretch_ratio = 3.0
	tank_stage.mouse_filter = Control.MOUSE_FILTER_IGNORE
	page.add_child(tank_stage)
	tank_stage.blob_mat.set_shader_parameter("by_species", false)
	tank_chart = Control.new()
	tank_chart.size_flags_vertical = Control.SIZE_EXPAND_FILL
	tank_chart.custom_minimum_size.y = 130
	tank_chart.draw.connect(_draw_tank_chart)
	page.add_child(tank_chart)


func _build_budget(box: VBoxContainer) -> void:
	_heading(box, "WILL IT LIVE?")
	verdict_label = _label("", 24, GOOD)
	box.add_child(verdict_label)
	verdict_sub = _label("", 12, FAINT)
	verdict_sub.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(verdict_sub)
	budget_bars = Control.new()
	budget_bars.custom_minimum_size = Vector2(0, 66)
	budget_bars.draw.connect(_draw_budget_bars)
	box.add_child(budget_bars)
	role_label = _label("", 13, INK)
	role_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(role_label)

	_heading(box, "HABITAT  (for the estimate and the test tank)")
	var sun_label := _label("", 12, INK)
	box.add_child(sun_label)
	var set_sun := func(v: float) -> void:
		sun = v
		sun_label.text = "Sun  %d%%  (100%% = the bright edge of the world)" % roundi(v * 100.0)
		_assess()
	_bare_slider(box, 0.0, 1.5, 0.05, set_sun).value = sun
	set_sun.call(sun)
	var air_label := _label("", 12, INK)
	box.add_child(air_label)
	var set_air := func(v: float) -> void:
		air = v
		air_label.text = "Air  %.0f °C  (the warm edge is about 24 °C, the cold edge 4 °C)" % v
		_assess()
	_bare_slider(box, -20.0, 45.0, 1.0, set_air).value = air
	set_air.call(air)

	_heading(box, "UPKEEP, LINE BY LINE  (energy a tick)")
	costs_text = _rich(12)
	box.add_child(costs_text)
	_heading(box, "ITS BODY")
	stats_text = _rich(12)
	box.add_child(stats_text)
	warn_text = _rich(12)
	box.add_child(warn_text)


func _build_library() -> void:
	library = PanelContainer.new()
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.09, 0.10, 0.13, 0.98)
	style.set_border_width_all(1)
	style.border_color = Color(1, 1, 1, 0.15)
	style.set_corner_radius_all(6)
	style.set_content_margin_all(14)
	library.add_theme_stylebox_override("panel", style)
	library.custom_minimum_size = Vector2(640, 420)
	library.visible = false
	add_child(library)
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 8)
	library.add_child(v)
	var head := HBoxContainer.new()
	v.add_child(head)
	var t := _label("SEED VAULT", 16, INK)
	t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(t)
	_button(head, "Show folder", func() -> void: OS.shell_open(ProjectSettings.globalize_path(VAULT_DIR)))
	_button(head, "Close", func() -> void: library.visible = false)
	v.add_child(_note("Designs you saved and species banked from the world (a sample of up to %d living members, so they keep their variety). Edit opens the first body here; Release drops the whole sample." % layout.get("vault_sample", 20)))
	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	v.add_child(scroll)
	library_list = VBoxContainer.new()
	library_list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	library_list.add_theme_constant_override("separation", 4)
	scroll.add_child(library_list)


# --- Small UI helpers --------------------------------------------------------------

func _label(text: String, font_size: int, color: Color) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", font_size)
	l.add_theme_color_override("font_color", color)
	return l


func _note(text: String) -> Label:
	var l := _label(text, 11, FAINT)
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.custom_minimum_size.x = 200
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


func _heading(box: Container, text: String) -> void:
	var l := _label(text, 11, FAINT)
	box.add_child(l)
	var line := ColorRect.new()
	line.color = Color(1, 1, 1, 0.08)
	line.custom_minimum_size.y = 1
	box.add_child(line)


func _button(box: Container, text: String, action: Callable, toggle := false) -> Button:
	var b := Button.new()
	b.text = text
	b.toggle_mode = toggle
	b.focus_mode = Control.FOCUS_NONE
	b.pressed.connect(action)
	box.add_child(b)
	return b


func _bare_slider(box: Container, low: float, high: float, step: float, changed: Callable) -> HSlider:
	var s := HSlider.new()
	s.min_value = low
	s.max_value = high
	s.step = step
	s.focus_mode = Control.FOCUS_NONE
	s.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	s.value_changed.connect(changed)
	box.add_child(s)
	return s


## A slider bound to one gene: it shows the gene and sets it.
func _bind_slider(box: Container, title: String, low: float, high: float, step: float, key: String, text: Callable) -> void:
	var l := _label("", 12, INK)
	box.add_child(l)
	var s := _bare_slider(box, low, high, step, func(v: float) -> void:
		genome[key] = v
		_edited(key))
	bindings.append({"slider": s, "label": l, "title": title, "get": func() -> float: return genome.get(key, 0.0), "text": text})


func _bind_shape_slider(box: Container, title: String, index: int) -> void:
	var l := _label("", 12, INK)
	box.add_child(l)
	var s := _bare_slider(box, -1.0, 1.0, 0.02, func(v: float) -> void:
		genome["shape"][index] = v
		_edited("shape%d" % index))
	bindings.append({"slider": s, "label": l, "title": title, "get": func() -> float: return genome["shape"][index],
		"text": func(v: float) -> String: return "%+.2f (looks only)" % v})


func _panel_scroll(parent: Container, width: float) -> PanelContainer:
	var panel := PanelContainer.new()
	var style := StyleBoxFlat.new()
	style.bg_color = PANEL_BG
	style.set_corner_radius_all(6)
	style.set_content_margin_all(10)
	panel.add_theme_stylebox_override("panel", style)
	panel.custom_minimum_size.x = width
	parent.add_child(panel)
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	panel.add_child(scroll)
	return panel   # pages go inside panel.get_child(0), the scroll container


func _scroll_page() -> ScrollContainer:
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	var inner := VBoxContainer.new()
	inner.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(inner)
	return scroll


func _page(parent: VBoxContainer) -> VBoxContainer:
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 6)
	parent.add_child(box)
	return box


func _swatch(c: Color) -> ImageTexture:
	var img := Image.create(12, 12, false, Image.FORMAT_RGBA8)
	img.fill(c)
	return ImageTexture.create_from_image(img)


func _status(text: String, color := FAINT) -> void:
	status_label.text = text
	status_label.add_theme_color_override("font_color", color)


# --- Tabs and tools ------------------------------------------------------------------

func _show_tab(i: int) -> void:
	tab = i
	for k in tab_buttons.size():
		tab_buttons[k].set_pressed_no_signal(k == i)
		left_pages[k].visible = k == i
		center_pages[k].visible = k == i
	if i == 0:
		_set_tool(canvas.tool if canvas.tool != "stamp" else "stamp")
	if i == 4 and not tank_running and tank_history.is_empty():
		_start_tank()


func _set_tool(t: String) -> void:
	canvas.tool = t
	for key: String in tool_buttons:
		var on := key == t or (t == "stamp" and key == "stamp%d" % canvas.stamp_type)
		(tool_buttons[key] as Button).set_pressed_no_signal(on)
	canvas.overlay.queue_redraw()


func _set_stamp(type: int) -> void:
	canvas.stamp_type = type
	_set_tool("stamp")


# --- Changing the design --------------------------------------------------------------

## Every change ends here: clamp it as the world would, keep an undo step, and
## refresh everything that shows it.
func _edited(merge_key: String) -> void:
	if merge_key == "full":
		_status("That is as many as a body can hold.", OK)
		return
	genome = world.clamp_design(genome)
	var now := Time.get_ticks_msec() / 1000.0
	var merge := merge_key != "" and merge_key == last_merge and now - last_merge_time < 1.2
	if not merge and genome != committed:
		undo_stack.append(committed)
		if undo_stack.size() > 200:
			undo_stack.pop_front()
		redo_stack.clear()
	committed = genome.duplicate(true)
	last_merge = merge_key
	last_merge_time = now
	_after_change(false)


func _after_change(full: bool) -> void:
	canvas.genome = genome
	brain.genome = genome
	canvas.refresh()
	_assess()
	_sync_sliders()
	_sync_part_panel()
	_sync_brain_panel()
	_probe()
	_build_chem(full)


func _undo() -> void:
	if undo_stack.is_empty():
		_status("Nothing to undo.")
		return
	redo_stack.append(committed)
	committed = undo_stack.pop_back()
	genome = committed.duplicate(true)
	last_merge = ""
	_after_change(true)


func _redo() -> void:
	if redo_stack.is_empty():
		_status("Nothing to redo.")
		return
	undo_stack.append(committed)
	committed = redo_stack.pop_back()
	genome = committed.duplicate(true)
	last_merge = ""
	_after_change(true)


func _sync_sliders() -> void:
	for b: Dictionary in bindings:
		var v: float = (b["get"] as Callable).call()
		(b["slider"] as HSlider).set_value_no_signal(v)
		(b["label"] as Label).text = "%s   %s" % [b["title"], (b["text"] as Callable).call(v)]


func _sync_part_panel() -> void:
	var k: int = canvas.selected
	var parts: Array = genome.get("parts", [])
	part_box.visible = k >= 0 and k < parts.size()
	if not part_box.visible:
		return
	var p: Dictionary = parts[k]
	var t: int = p["type"]
	part_title.text = PART_NAMES[t]
	part_title.add_theme_color_override("font_color", canvas.PART_COLORS[t])
	var blurb: String = PART_BLURBS[t]
	if t == canvas.FLAGELLUM or t == canvas.LEGS:
		blurb += "  Here it pushes at %d%%." % roundi(canvas.thrust_facing(t, p["theta"]) * 100.0)
	part_blurb.text = blurb
	part_size.set_value_no_signal(p["size"])
	part_size_label.text = "Size   %.2f" % p["size"]
	part_angle.set_value_no_signal(rad_to_deg(p["theta"]))
	part_angle_label.text = "Where   %s (%d°)" % [_side(p["theta"]), roundi(rad_to_deg(p["theta"]))]
	var stalky: bool = t == canvas.PHOTO or t == canvas.ABSORBER
	part_stalk.visible = stalky
	part_stalk_label.visible = stalky
	part_stalk.set_value_no_signal(p["stalk"])
	part_stalk_label.text = "Stalk   %.2f  %s" % [p["stalk"], "(on the skin)" if p["stalk"] <= 0.0 else ("a tentacle" if t == canvas.ABSORBER else "a frond")]


func _set_part(key: String, v: float) -> void:
	var k: int = canvas.selected
	var parts: Array = genome["parts"]
	if k < 0 or k >= parts.size():
		return
	var partner: int = canvas._partner(k) if canvas.mirror else -1
	parts[k][key] = v
	if partner >= 0:
		parts[partner][key] = -v if key == "theta" else v
	_edited("%s%d" % [key, k])


func _side(theta: float) -> String:
	var a := wrapf(rad_to_deg(theta), -180.0, 180.0)
	var names := ["front", "front-right", "right", "back-right", "back", "back-left", "left", "front-left"]
	# Screen down (+theta) is the body's right when it faces right.
	var i := posmod(roundi(a / 45.0), 8)
	return names[i]


func _sync_brain_panel() -> void:
	brain.has_sensor = report.get("sense_cells", 0) > 0
	var parts: Array = genome.get("parts", [])
	brain.has_motor = parts.any(func(p: Dictionary) -> bool: return int(p["type"]) in [canvas.FLAGELLUM, canvas.LEGS])
	var links: Array = genome.get("links", [])
	link_box.visible = brain.sel_kind == "link" and brain.sel_index < links.size()
	if link_box.visible:
		var l: Dictionary = links[brain.sel_index]
		link_title.text = "%s  →  %s" % [_node_label(l["from"]), _node_label(l["to"])]
		link_weight.set_value_no_signal(l["weight"])
		link_weight_label.text = "Weight   %+.2f  (%s)" % [l["weight"], "pushes" if l["weight"] >= 0.0 else "holds back"]
	neuron_box.visible = brain.sel_kind == "neuron" and brain.sel_index < int(genome.get("hidden", 0))
	if neuron_box.visible:
		var k: int = brain.sel_index
		var tau: float = genome["tau"][k]
		var bias: float = genome["bias"][k]
		neuron_title.text = "Neuron h%d" % k
		neuron_tau.set_value_no_signal(log(tau) / log(10.0))
		neuron_tau_label.text = "Memory (τ)   %.0f ticks  %s" % [tau, "(instant)" if tau < 1.5 else ("(about %.1f days)" % (tau / layout.get("day_length", 600)) if tau > 120.0 else "")]
		neuron_bias.set_value_no_signal(bias)
		neuron_bias_label.text = "Bias   %+.2f" % bias
	brain.queue_redraw()


func _node_label(node: int) -> String:
	var fo: int = layout["first_output"]
	var fh: int = layout["first_hidden"]
	if node >= fh:
		return "h%d" % (node - fh)
	if node >= fo:
		return brain.OUTPUT_LABELS[node - fo]
	return brain.INPUT_LABELS[node]


func _add_reflex(wires: Array) -> void:
	var inputs: PackedStringArray = layout["inputs"]
	var outputs: PackedStringArray = layout["outputs"]
	var fo: int = layout["first_output"]
	var links: Array = genome["links"]
	for w: Array in wires:
		var from := inputs.find(w[0])
		var to := fo + outputs.find(w[1])
		var found := false
		for l: Dictionary in links:
			if int(l["from"]) == from and int(l["to"]) == to:
				l["weight"] = w[2]
				found = true
		if not found and links.size() < int(layout["max_links"]):
			links.append({"from": from, "to": to, "weight": w[2]})
	_edited("")
	if not brain.has_motor:
		_status("Wired. It needs a tail or legs before a brain does anything.", OK)


func _probe() -> void:
	if genome.is_empty():
		return
	var got: Dictionary = world.probe_brain(genome, probe_inputs, probe_ticks)
	brain.inputs = probe_inputs
	brain.outputs = got["outputs"]
	brain.hidden_fire = got["hidden"]
	brain.queue_redraw()


# --- Chemistry --------------------------------------------------------------------------

func _add_enzyme(e: Array) -> void:
	var enzymes: Array = genome["enzymes"]
	if enzymes.size() >= int(layout["max_enzymes"]):
		_status("A body holds at most %d enzymes." % layout["max_enzymes"], OK)
		return
	enzymes.append({"x": e[0], "y": e[1], "x2": e[2], "y2": e[3], "rate": e[4]})
	_edited("")
	_build_chem(true)


var _chem_shape := ""

## Rebuild the enzyme cards when their number or reactions change (not while
## a rate slider is being dragged).
func _build_chem(force: bool) -> void:
	var enzymes: Array = genome.get("enzymes", [])
	var shape := ""
	for e: Dictionary in enzymes:
		shape += str(e.get("words", "")) + ";"
	if not force and shape == _chem_shape:
		_update_chem_labels()
		return
	_chem_shape = shape
	for c in chem_list.get_children():
		c.queue_free()
	var pad := MarginContainer.new()
	for side in ["left", "right", "top"]:
		pad.add_theme_constant_override("margin_" + side, 16)
	chem_list.add_child(pad)
	var inner := VBoxContainer.new()
	inner.add_theme_constant_override("separation", 10)
	pad.add_child(inner)
	inner.add_child(_label("ENZYMES  (%d of %d)" % [enzymes.size(), layout["max_enzymes"]], 12, FAINT))
	if enzymes.is_empty():
		inner.add_child(_label("No enzymes: it cannot burn anything, so it cannot live. Add one on the left.", 13, BAD))
	for k in enzymes.size():
		inner.add_child(_enzyme_card(k, enzymes[k]))
	_update_chem_labels()


func _enzyme_card(k: int, e: Dictionary) -> PanelContainer:
	var card := PanelContainer.new()
	var st := StyleBoxFlat.new()
	st.bg_color = Color(0.11, 0.125, 0.16)
	st.set_corner_radius_all(5)
	st.set_content_margin_all(10)
	card.add_theme_stylebox_override("panel", st)
	var v := VBoxContainer.new()
	card.add_child(v)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 6)
	v.add_child(row)
	var pick := func(key: String, allow_none: bool) -> OptionButton:
		var o := OptionButton.new()
		o.focus_mode = Control.FOCUS_NONE
		if allow_none:
			o.add_item("—", 0)
		for c in CHEM_NAMES.size():
			o.add_item(CHEM_NAMES[c], c + 1)
		var cur: int = e.get(key, -1)
		o.select(o.get_item_index(cur + 1) if cur >= 0 else (0 if allow_none else 0))
		o.item_selected.connect(func(idx: int) -> void:
			var id := o.get_item_id(idx) - 1
			var en: Dictionary = genome["enzymes"][k]
			en[key] = id
			# A two-input reaction needs both second slots, or neither.
			if key == "x2" and id >= 0 and int(en["y2"]) < 0: en["y2"] = 0
			if key == "y2" and id >= 0 and int(en["x2"]) < 0: en["x2"] = 0
			if key in ["x2", "y2"] and id < 0:
				en["x2"] = -1
				en["y2"] = -1
			_edited("")
			_build_chem(true))
		return o
	row.add_child(pick.call("x", false))
	row.add_child(_label("+", 14, FAINT))
	row.add_child(pick.call("x2", true))
	row.add_child(_label("  ⇄  ", 16, INK))
	row.add_child(pick.call("y", false))
	row.add_child(_label("+", 14, FAINT))
	row.add_child(pick.call("y2", true))
	var gap := Control.new()
	gap.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(gap)
	_button(row, "Remove", func() -> void:
		(genome["enzymes"] as Array).remove_at(k)
		_edited("")
		_build_chem(true))
	var info := _label("", 12, FAINT)
	info.name = "Info"
	info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	v.add_child(info)
	var rate_row := HBoxContainer.new()
	v.add_child(rate_row)
	var rate_label := _label("", 12, INK)
	rate_label.name = "Rate"
	rate_label.custom_minimum_size.x = 210
	rate_row.add_child(rate_label)
	# Rate on a log scale, MIN_ENZYME_RATE .. MAX_ENZYME_RATE.
	var lo: float = log(float(layout["min_rate"])) / log(10.0)
	var hi: float = log(float(layout["max_rate"])) / log(10.0)
	var s := _bare_slider(rate_row, lo, hi, 0.01, func(val: float) -> void:
		genome["enzymes"][k]["rate"] = pow(10.0, val)
		_edited("rate%d" % k))
	s.set_value_no_signal(log(float(e["rate"])) / log(10.0))
	card.set_meta("index", k)
	return card


func _update_chem_labels() -> void:
	var levels: PackedFloat32Array = layout["levels"]
	var enzymes: Array = genome.get("enzymes", [])
	var cards := chem_list.find_children("*", "PanelContainer", true, false)
	for card: Node in cards:
		if not card.has_meta("index"):
			continue
		var k: int = card.get_meta("index")
		if k >= enzymes.size():
			continue
		var e: Dictionary = enzymes[k]
		var lo: float = levels[int(e["x"])] + (levels[int(e["x2"])] if int(e["x2"]) >= 0 else 0.0)
		var hi: float = levels[int(e["y"])] + (levels[int(e["y2"])] if int(e["y2"]) >= 0 else 0.0)
		var step := hi - lo
		var rate: float = e["rate"]
		var info: Label = card.find_child("Info", true, false)
		var rl: Label = card.find_child("Rate", true, false)
		var words := str(e.get("words", "")).replace("-", " ⇄ ")
		if step > 0.0:
			info.text = "%s: burning releases %d a unit, up to %.2f energy a tick; in the light it can store as much." % [words, int(step), rate * step]
		else:
			info.text = "%s: equal on both sides, so it releases and stores nothing and never runs; it only costs upkeep." % words
		var cost := 0.0
		for c: Dictionary in report.get("costs", []):
			if str(c["kind"]) == "enzyme" and int(c["index"]) == k:
				cost = c["upkeep"]
		rl.text = "Rate %.4f a tick   upkeep %.4f" % [rate, cost]
	var burns: String = report.get("burns", "")
	chem_note.text = "[b]Burns:[/b] %s\n[b]Most it can release:[/b] %.2f a tick (it spends %.2f)\n[b]Most light it can store:[/b] %.2f a tick" % [
		burns if burns != "" else "[color=#f66]nothing[/color]", report.get("burn_power", 0.0),
		report.get("upkeep", 0.0) + report.get("thermal", 0.0), report.get("charge_power", 0.0)]


# --- The energy budget -------------------------------------------------------------------

func _assess() -> void:
	if genome.is_empty() or verdict_label == null:
		return
	report = world.assess_design(genome, sun, air)
	var upkeep: float = report["upkeep"]
	var heat: float = report["thermal"]
	var income: float = report["light_income"]
	var spend := upkeep + heat
	var net := income - spend
	var parts: Array = genome["parts"]
	var has := func(t: int) -> bool: return parts.any(func(p: Dictionary) -> bool: return int(p["type"]) == t)
	var mouth: bool = has.call(canvas.ABSORBER)
	var photo: bool = has.call(canvas.PHOTO)
	var burns: String = report["burns"]
	var doomed: bool = burns == "" or float(report["burn_power"]) < spend
	var day: float = layout.get("day_length", 600)

	var verdict := ""
	var color := GOOD
	var sub := ""
	if doomed:
		verdict = "WON'T LIVE"
		color = BAD
		sub = "Its enzymes can't release what it spends. See Chemistry."
	elif net > 0.12 * spend:
		verdict = "THRIVES"
		color = GOOD
	elif net > 0.0:
		verdict = "GETS BY"
		color = OK
	elif mouth:
		verdict = "MUST EAT"
		color = Color(1.0, 0.65, 0.3)
		sub = "The sun can't keep it: it needs %.2f a tick from food (about %.1f LUCA bodies a day)." % [-net, -net * day / 244.0]
	elif not photo and burns.contains("C"):
		verdict = "NEEDS A VENT"
		color = Color(1.0, 0.65, 0.3)
		sub = "It lives on C, which only vents make. Release it near one."
	else:
		verdict = "STARVES"
		color = BAD
		sub = "It spends %.2f a tick more than the sun gives it here." % -net
	if net > 0.0 and not doomed:
		var threshold: float = report["repro_threshold"]
		var child: float = report["child_cost"]
		var refill: float = child + float(genome["offspring_share"]) * maxf(threshold - child, 0.0)
		var days := refill / (net * day)
		sub = "Net %+.2f a tick: a child about every %s." % [net, ("%.1f days" % days) if days < 50.0 else "%d days" % roundi(days)]
	verdict_label.text = verdict
	verdict_label.add_theme_color_override("font_color", color)
	verdict_sub.text = sub
	budget_bars.set_meta("income", income)
	budget_bars.set_meta("upkeep", upkeep)
	budget_bars.set_meta("heat", heat)
	budget_bars.queue_redraw()
	role_label.text = _role(mouth, photo, burns)

	# The bill.
	var lines: PackedStringArray = []
	var top := 0.0
	for c: Dictionary in report["costs"]:
		top = maxf(top, c["upkeep"])
	for c: Dictionary in report["costs"]:
		var u: float = c["upkeep"]
		var bar := "█".repeat(clampi(roundi(u / maxf(top, 1e-6) * 14.0), 1, 14))
		lines.append("[color=#%s]%s[/color] %s  [color=#9aa3b2]%.3f[/color]" % [_cost_color(c).to_html(false), bar, _cost_name(c), u])
	if heat > 0.0005:
		lines.append("[color=#%s]%s[/color] Heat or cold here  [color=#9aa3b2]%.3f[/color]" % [BAD.to_html(false), "█".repeat(clampi(roundi(heat / maxf(top, 1e-6) * 14.0), 1, 14)), heat])
	lines.append("[color=#9aa3b2]At age 10,000 ticks: %.2f a tick (wear)[/color]" % report["upkeep_old"])
	costs_text.text = "\n".join(lines)

	# Its body in numbers.
	var speed: float = report["max_speed"]
	var sense: int = report["sense_cells"]
	var stats: PackedStringArray = [
		"Mass [b]%.2f[/b]   store [b]%.0f[/b]   body worth [b]%.0f[/b]" % [report["mass"], report["capacity"], report["body_energy"]],
		"Speed [b]%s[/b]   sees [b]%s[/b]" % ["%.3f a tick" % speed if speed > 0.0 else "none", "%d cells" % sense if sense > 0 else "nothing"],
		"Pull [b]%.2f[/b]   membrane [b]%.2f[/b]   poison [b]%.2f[/b]" % [report["pull"], report["membrane"], report["toxicity"]],
		"Sun catch [b]%.2f[/b]   stands [b]%.2f[/b] tall   child costs [b]%.0f[/b]" % [report["light_capture"], report["canopy"], report["child_cost"]],
		"Burns [b]%s[/b]   parts [b]%d/%d[/b]   links [b]%d/%d[/b]" % [burns if burns != "" else "nothing", parts.size(), layout["max_parts"],
			(genome["links"] as Array).size(), layout["max_links"]],
	]
	stats_text.text = "\n".join(stats)
	var warns: PackedStringArray = report["warnings"]
	var wl: PackedStringArray = []
	for w in warns:
		wl.append("[color=#ffb35c]▲ %s[/color]" % w)
	warn_text.text = "\n".join(wl)
	if chem_note != null:
		_update_chem_labels()


func _role(mouth: bool, photo: bool, burns: String) -> String:
	var mover: bool = report.get("mover", false)
	var role := ""
	if burns == "":
		role = "A stillborn: it can burn nothing."
	elif mouth and mover:
		role = "Plays as: a roaming hunter or grazer."
	elif mouth and photo:
		role = "Plays as: a flytrap, a plant that also drains its neighbours."
	elif mouth:
		role = "Plays as: an ambush feeder, sitting and draining whatever comes near."
	elif photo and mover:
		role = "Plays as: a drifting plant."
	elif photo:
		role = "Plays as: a plant."
	elif burns.contains("C"):
		role = "Plays as: a vent dweller."
	else:
		role = "Plays as: a skin feeder, living on loose chemicals."
	var tags: PackedStringArray = []
	if float(report["toxicity"]) > 0.0: tags.append("poisonous")
	if float(report["membrane"]) > 0.6: tags.append("armoured")
	if float(genome["dormancy"]) > 0.0: tags.append("sleeps")
	if int(report["sense_cells"]) > 0: tags.append("sighted")
	if float(genome["birth_size"]) < 0.3: tags.append("many small young")
	return role + ("  (" + ", ".join(tags) + ")" if not tags.is_empty() else "")


func _cost_name(c: Dictionary) -> String:
	var i: int = c["index"]
	match str(c["kind"]):
		"body": return "Body"
		"part":
			var p: Dictionary = genome["parts"][i]
			var stalk := " on a stalk" if float(p["stalk"]) > 0.0 else ""
			return "%s (%s%s)" % [PART_NAMES[int(p["type"])], _side(p["theta"]), stalk]
		"enzyme": return "Enzyme %s" % str(genome["enzymes"][i].get("words", "")).replace("-", "⇄")
		"brain": return "Brain"
		"tolerance": return "Wide comfort range"
		"bite": return "Bite"
		"dormancy": return "Sleep machinery"
		"throw": return "Seed throwing"
		"shape": return "Extra skin (shape)"
	return str(c["kind"])


func _cost_color(c: Dictionary) -> Color:
	if str(c["kind"]) == "part":
		return canvas.PART_COLORS[int(genome["parts"][int(c["index"])]["type"])]
	if str(c["kind"]) == "body":
		return Color(0.93, 0.84, 0.66)
	if str(c["kind"]) == "enzyme":
		return Color(0.55, 0.75, 1.0)
	return Color(0.7, 0.72, 0.78)


func _draw_budget_bars() -> void:
	var income: float = budget_bars.get_meta("income", 0.0)
	var upkeep: float = budget_bars.get_meta("upkeep", 0.0)
	var heat: float = budget_bars.get_meta("heat", 0.0)
	var top := maxf(maxf(income, upkeep + heat), 0.01) * 1.1
	var w := budget_bars.size.x - 96.0
	var font := get_theme_default_font()
	var row := func(y: float, label: String, segs: Array, total: float) -> void:
		budget_bars.draw_string(font, Vector2(0, y + 15), label, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, FAINT)
		var x := 58.0
		budget_bars.draw_rect(Rect2(x, y + 2, w, 18), Color(1, 1, 1, 0.05))
		for s: Array in segs:
			var len: float = w * float(s[0]) / top
			budget_bars.draw_rect(Rect2(x, y + 2, len, 18), s[1])
			x += len
		budget_bars.draw_string(font, Vector2(58 + w + 4, y + 15), "%.2f" % total, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, INK)
	row.call(4.0, "Sun in", [[income, Color(0.95, 0.80, 0.30)]], income)
	row.call(34.0, "Spends", [[upkeep, Color(0.55, 0.70, 0.95)], [heat, BAD]], upkeep + heat)


# --- The seed vault ----------------------------------------------------------------------

func _file_name(title: String) -> String:
	var out := ""
	for ch in title.strip_edges():
		out += ch if (ch.is_valid_identifier() or ch.is_valid_int() or ch in "-_") else ("_" if ch == " " else "")
	return out if out != "" else "creature"


func _save() -> void:
	var title := design_name.strip_edges()
	if title == "" or title == "Unnamed":
		_status("Give it a name first (top left).", OK)
		name_edit.grab_focus()
		return
	var path := "%s/%s.creature" % [VAULT_DIR, _file_name(title)]
	if _write(path, world.write_creature_file([genome], title, origin)):
		_status("Saved %s to the seed vault." % title, GOOD)


func _write(path: String, text: String) -> bool:
	DirAccess.make_dir_recursive_absolute(VAULT_DIR)
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f == null:
		_status("Could not write %s." % ProjectSettings.globalize_path(path), BAD)
		return false
	f.store_string(text)
	f.close()
	return true


## Bank a living species from the world: a sample of its members, as a file.
## Returns a line for the chronicle.
func bank_species(species_id: int) -> String:
	var text: String = world.bank_species(species_id)
	if text == "":
		return "There is no such species to bank."
	var info: Dictionary = world.read_creature_file(text)
	var title: String = info["name"]
	var path := "%s/%s.creature" % [VAULT_DIR, _file_name(title)]
	var n := 2
	while FileAccess.file_exists(path):
		path = "%s/%s_%d.creature" % [VAULT_DIR, _file_name(title), n]
		n += 1
	if not _write(path, text):
		return "Could not write to the seed vault."
	return "Banked %d %s in the seed vault (%s)." % [(info["bodies"] as Array).size(), title, path.get_file()]


func _open_library() -> void:
	for c in library_list.get_children():
		c.queue_free()
	DirAccess.make_dir_recursive_absolute(VAULT_DIR)
	var files := DirAccess.get_files_at(VAULT_DIR)
	var any := false
	for file in files:
		if not file.ends_with(".creature"):
			continue
		any = true
		var path := "%s/%s" % [VAULT_DIR, file]
		var text := FileAccess.get_file_as_string(path)
		var info: Dictionary = world.read_creature_file(text)
		var row := HBoxContainer.new()
		row.add_theme_constant_override("separation", 8)
		library_list.add_child(row)
		var words := VBoxContainer.new()
		words.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(words)
		if not info["ok"]:
			words.add_child(_label(file, 13, BAD))
			words.add_child(_note("Can't read it: %s" % info["error"]))
		else:
			var n: int = (info["bodies"] as Array).size()
			words.add_child(_label(str(info["name"]) if str(info["name"]) != "" else file.get_basename(), 14, INK))
			words.add_child(_note("%s · %s" % ["1 body" if n == 1 else "a sample of %d" % n, info["origin"]]))
			_button(row, "Edit", func() -> void:
				library.visible = false
				load_genome(info["bodies"][0], info["name"], "edited from %s" % file)
				_status("Opened %s%s." % [info["name"], "" if n == 1 else " (the first of its %d bodies)" % n]))
			var rel := _button(row, "Release", func() -> void:
				library.visible = false
				_release_text(text, info["name"]))
			rel.add_theme_color_override("font_color", GUEST)
		_button(row, "Delete", func() -> void:
			DirAccess.remove_absolute(path)
			_open_library())
	if not any:
		library_list.add_child(_note("The vault is empty. Save a design here, or bank a species from the world (inspector or species card)."))
	library.visible = true
	library.reset_size()
	library.position = ((size - library.size) * 0.5).floor()


func _release() -> void:
	var title := design_name.strip_edges()
	if title == "" or title == "Unnamed":
		title = "Designed"
	_release_text(world.write_creature_file([genome], title, origin), title)


func _release_text(text: String, title: String) -> void:
	var count := int(count_spin.value)
	visible = false
	tank_running = false
	world.trial_stop()
	release_requested.emit(text, title, count)


# --- The test tank --------------------------------------------------------------------------

func _start_tank() -> void:
	var text: String = world.write_creature_file([genome], design_name if design_name != "" else "Designed", origin)
	if not world.trial_start(text, int(tank_count.value), tank_luca.button_pressed, sun, air, tank_seed):
		return
	tank_history.clear()
	tank_running = true
	var info: Dictionary = world.trial_info()
	tank_extent = info.get("extent", 32.0)
	tank_capacity = 0
	_record_tank(info)
	_draw_tank()


func _process(_delta: float) -> void:
	if not visible or tab != 4:
		return
	if tank_running:
		world.trial_step(TANK_SPEEDS[tank_speed], 8.0)
		var info: Dictionary = world.trial_info()
		_record_tank(info)
		if int(info.get("guests", 0)) == 0 and int(info.get("tick", 0)) > 0:
			tank_running = false
	_draw_tank()


func _record_tank(info: Dictionary) -> void:
	if info.is_empty():
		return
	var tick: int = info["tick"]
	if tank_history.is_empty() or tick - int(tank_history[-1][0]) >= 25:
		tank_history.append([tick, int(info["guests"]), int(info["natives"])])
	var day: float = layout.get("day_length", 600)
	var guests: int = info["guests"]
	var fed := [float(info["fed_light"]), float(info["fed_living"]), float(info["fed_dead"]), float(info["fed_loose"])]
	var total: float = fed[0] + fed[1] + fed[2] + fed[3]
	var diet := ""
	if total > 0.0:
		diet = "Diet: sun %d%%, the living %d%%, the dead %d%%, loose %d%%" % [roundi(fed[0] / total * 100), roundi(fed[1] / total * 100), roundi(fed[2] / total * 100), roundi(fed[3] / total * 100)]
	var title := design_name if design_name != "" else "design"
	var verdict := ""
	if guests == 0:
		verdict = "[color=#f66][b]Your %s died out[/b][/color] on day %.1f." % [title, tick / day]
	elif int(info["generations"]) >= 3:
		verdict = "[color=#7e7][b]Your %s are breeding true[/b][/color]: %d generations deep." % [title, int(info["generations"])]
	elif int(info["births"]) > 0:
		verdict = "[color=#fd6][b]First young born.[/b][/color]"
	else:
		verdict = "[color=#9aa3b2]Waiting for the first child…[/color]"
	tank_info.text = "%s\n\nDay [b]%.1f[/b]   tick %d\nAlive [color=#%s][b]%d[/b][/color]   wild [color=#%s]%d[/color]\nBorn [b]%d[/b]   generations [b]%d[/b]\nFed %d%% full\n%s" % [
		verdict, tick / day, tick, GUEST.to_html(false), guests, NATIVE.to_html(false), int(info["natives"]),
		int(info["births"]), int(info["generations"]), roundi(float(info["mean_energy"]) * 100.0), diet]


func _draw_tank() -> void:
	if tank_stage == null or not tank_stage.is_visible_in_tree():
		return
	var info: Dictionary = world.trial_info()
	if info.is_empty():
		return
	var n: int = int(info["guests"]) + int(info["natives"])
	if n > tank_capacity:
		tank_capacity = maxi(256, nearest_po2(n + 1))
	var room := minf(tank_stage.size.x, tank_stage.size.y) - 16.0
	var zoom := maxf(0.5, floorf(room / (tank_extent / tank_stage.PX_SIZE) * 2.0) / 2.0)
	tank_stage.look(Vector2(tank_extent, tank_extent) * 0.5, zoom)
	tank_stage.set_ground(true, Rect2(0, 0, tank_extent, tank_extent))
	var width := 1024
	tank_stage.show_sprites(world.trial_instance_buffer(tank_capacity), world.trial_part_buffer(tank_capacity), n, width)
	tank_stage.blob_mat.set_shader_parameter("by_species", true)
	tank_chart.queue_redraw()


func _draw_tank_chart() -> void:
	var r := Rect2(Vector2(14, 10), tank_chart.size - Vector2(28, 30))
	var font := get_theme_default_font()
	tank_chart.draw_rect(r, Color(1, 1, 1, 0.03))
	if tank_history.size() < 2:
		tank_chart.draw_string(font, r.position + Vector2(8, 20), "Population over time appears here.", HORIZONTAL_ALIGNMENT_LEFT, -1, 12, FAINT)
		return
	var last_tick: float = maxf(float(tank_history[-1][0]), 600.0)
	var top := 1.0
	for h: Array in tank_history:
		top = maxf(top, maxf(h[1], h[2]))
	var day: float = layout.get("day_length", 600)
	var d := day
	while d <= last_tick:
		var x := r.position.x + r.size.x * d / last_tick
		tank_chart.draw_line(Vector2(x, r.position.y), Vector2(x, r.end.y), Color(1, 1, 1, 0.05))
		d += day * maxf(1.0, floorf(last_tick / day / 12.0))
	for series in [[2, NATIVE], [1, GUEST]]:
		var pts := PackedVector2Array()
		for h: Array in tank_history:
			pts.append(Vector2(r.position.x + r.size.x * float(h[0]) / last_tick, r.end.y - r.size.y * float(h[series[0]]) / top))
		tank_chart.draw_polyline(pts, series[1], 2.0, true)
	tank_chart.draw_string(font, Vector2(r.position.x + 4, r.position.y + 14), "%d" % roundi(top), HORIZONTAL_ALIGNMENT_LEFT, -1, 11, FAINT)
	tank_chart.draw_string(font, Vector2(r.position.x, r.end.y + 16), "your design", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, GUEST)
	tank_chart.draw_string(font, Vector2(r.position.x + 90, r.end.y + 16), "wild LUCAs", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, NATIVE)
	tank_chart.draw_string(font, Vector2(r.end.x - 70, r.end.y + 16), "day %.1f" % (last_tick / day), HORIZONTAL_ALIGNMENT_LEFT, -1, 11, FAINT)


# --- Keys ------------------------------------------------------------------------------------

func _input(event: InputEvent) -> void:
	if not visible or not (event is InputEventKey) or not event.pressed or event.echo:
		return
	var focus := get_viewport().gui_get_focus_owner()
	if focus is LineEdit or focus is TextEdit:
		if event.keycode == KEY_ESCAPE:
			focus.release_focus()
			get_viewport().set_input_as_handled()
		return
	var cmd: bool = event.ctrl_pressed or event.meta_pressed
	var handled := true
	if event.keycode == KEY_ESCAPE:
		close()
	elif cmd and event.keycode == KEY_Z:
		if event.shift_pressed: _redo()
		else: _undo()
	elif cmd and event.keycode == KEY_Y:
		_redo()
	elif cmd and event.keycode == KEY_S:
		_save()
	elif event.keycode in [KEY_DELETE, KEY_BACKSPACE]:
		if tab == 0 and canvas.selected >= 0:
			canvas.remove_part(canvas.selected)
		elif tab == 1 and brain.sel_kind == "link":
			brain.remove_link(brain.sel_index)
		elif tab == 1 and brain.sel_kind == "neuron":
			brain.remove_neuron(brain.sel_index)
	elif tab == 0 and event.keycode == KEY_V:
		_set_tool("select")
	elif tab == 0 and event.keycode == KEY_P:
		_set_tool("pen")
	elif tab == 0 and event.keycode == KEY_M:
		mirror_check.button_pressed = not mirror_check.button_pressed
	elif tab == 0 and event.keycode >= KEY_1 and event.keycode <= KEY_8:
		_set_stamp(event.keycode - KEY_1)
	else:
		handled = false
	if handled:
		get_viewport().set_input_as_handled()


# A rough name for a skin hue (core/colour.hpp keeps every hue earthy).
func _hue_word(v: float) -> String:
	var words := ["rust", "tan", "ochre", "olive", "sage", "sage", "grey-green", "slate", "slate", "dusky", "mauve", "rose"]
	return words[int(round(fposmod(v, 1.0) * 12.0)) % 12]
