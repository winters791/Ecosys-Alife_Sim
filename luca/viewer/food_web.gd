extends Control
## The food web (design doc 4.3), drawn from the energy that actually moved
## (LucaWorld.get_food_web; core/foodweb.hpp). Nothing is labelled predator or
## plant: a species stands as far right as its trophic level, worked out from
## what it ate over the last few thousand ticks, and every arrow is a flow of
## energy, as thick as the flow.
##
##   Columns  the sources on the left (sun, vents, the dead, loose chemicals),
##            then species by trophic level, 1 (lives on the sun or on the
##            dead) rightwards.
##   Arrows   red: drained from the living. Olive: leavings taken up. Gold:
##            sunlight. Orange: vents. Grey: the dead. Brown: loose chemicals
##            nobody is tagged with.
##   Mouse    hover a species to see what it eats and who eats it; click to
##            select it (the card beside opens on it); wheel to show more or
##            fewer species.
##   Keys     Esc or G closes.
##
## Plain pixel rectangles and straight lines; nothing here feeds back into the world.

signal close_requested
signal selection_changed(species_id: int)

const HEAD := 30
const FOOT := 62
const PAD := 16
const SOURCE_W := 120     # the sources' column
const NODE_MIN := 6
const NODE_MAX := 26

const BG := Color(0.065, 0.072, 0.095)
const BAR := Color(0.045, 0.05, 0.068)
const GRID := Color(1, 1, 1, 0.05)
const INK := Color(0.93, 0.94, 0.96)
const FAINT := Color(0.58, 0.63, 0.71)
const KIND_COLOR := {
	"living": Color(0.80, 0.25, 0.22),
	"leavings": Color(0.60, 0.62, 0.30),
	"sun": Color(0.92, 0.78, 0.30),
	"vent": Color(0.90, 0.48, 0.20),
	"dead": Color(0.62, 0.60, 0.58),
	"loose": Color(0.55, 0.42, 0.28),
}
const SOURCE_NAMES := {"sun": "Sun", "vent": "Vents", "dead": "The dead", "loose": "Loose chemicals"}
const SOURCE_ORDER := ["sun", "vent", "dead", "loose"]

var nodes: Array = []       # from get_food_web, most intake first
var edges: Array = []
var shown := 30             # how many species are drawn (wheel changes it)
var selected_id := -1
var hover_id := -1          # a species id, or -1
var mouse := Vector2.ZERO

# Layout, rebuilt on every update.
var pos := {}               # species id -> Vector2 (node centre)
var radius := {}            # species id -> half-size in pixels
var source_pos := {}        # source kind -> Vector2
var max_level := 2.0
var intake_of := {}         # species id -> energy a tick it takes in

const MINOR := 0.03         # flows under this share of what the eater takes in are drawn only for the species in focus


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	focus_mode = Control.FOCUS_ALL
	clip_contents = true


func show_web(web: Dictionary) -> void:
	nodes = web.get("nodes", [])
	edges = web.get("edges", [])
	_layout()
	queue_redraw()


func select_id(id: int) -> void:
	selected_id = id
	queue_redraw()


func _layout() -> void:
	pos.clear()
	radius.clear()
	source_pos.clear()
	intake_of.clear()
	for n: Dictionary in nodes:
		intake_of[int(n["id"])] = float(n["intake"])
	var list := nodes.slice(0, shown)
	max_level = 2.0
	var max_alive := 1
	for n: Dictionary in list:
		max_level = maxf(max_level, float(n["level"]))
		max_alive = maxi(max_alive, int(n["alive"]))

	var top := float(HEAD + 24)
	var bottom := size.y - FOOT - 12.0
	var left := float(PAD + SOURCE_W)
	var right := size.x - PAD - 120.0      # room for names

	# Sources that fed anything shown, stacked in the left column.
	var used := {}
	for e: Dictionary in edges:
		if int(e["from"]) < 0:
			used[e["kind"]] = true
	var k := 0
	for kind: String in SOURCE_ORDER:
		if used.has(kind):
			source_pos[kind] = Vector2(PAD + 10, top + 20.0 + k * 54.0)
			k += 1

	# Species: a column for each band of a quarter of a trophic level; within a
	# band, the biggest eaters at the top.
	var bands := {}
	for n: Dictionary in list:
		var band := int(floor((float(n["level"]) - 1.0) * 4.0))
		if not bands.has(band):
			bands[band] = []
		bands[band].append(n)
	for band: int in bands:
		var col: Array = bands[band]
		var x := left + clampf((band / 4.0 + 0.125) / maxf(max_level - 1.0, 0.25), 0.0, 1.0) * (right - left)
		var step := (bottom - top) / maxf(col.size(), 1)
		for j in col.size():
			var n: Dictionary = col[j]
			var id := int(n["id"])
			# Stagger alternate rows a little so names in a crowded column don't collide.
			pos[id] = Vector2(x + (12.0 if j % 2 == 1 else 0.0), top + step * (j + 0.5))
			radius[id] = roundf(lerpf(NODE_MIN, NODE_MAX, sqrt(float(n["alive"]) / max_alive)) * 0.5)


func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED:
		_layout()


func _draw() -> void:
	var font := get_theme_default_font()
	draw_rect(Rect2(Vector2.ZERO, size), BG)
	draw_rect(Rect2(0, 0, size.x, HEAD), BAR)
	draw_string(font, Vector2(PAD, 20), "Showing %d of %d species (wheel for more or fewer).   Energy over the last few thousand ticks." % [mini(shown, nodes.size()), nodes.size()], HORIZONTAL_ALIGNMENT_LEFT, -1, 12, FAINT)
	if nodes.is_empty():
		draw_string(font, Vector2(PAD, HEAD + 40), "No flows yet: the web fills in as life eats.", HORIZONTAL_ALIGNMENT_LEFT, -1, 13, FAINT)
		_draw_footer(font)
		return

	# Trophic level gridlines.
	var left := float(PAD + SOURCE_W)
	var right := size.x - PAD - 120.0
	for lv in range(1, int(ceil(max_level)) + 1):
		var x := left + (lv - 1.0) / maxf(max_level - 1.0, 0.25) * (right - left)
		draw_line(Vector2(x, HEAD + 8), Vector2(x, size.y - FOOT), GRID, 1.0)
		draw_string(font, Vector2(x + 3, HEAD + 18), "level %d" % lv, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, FAINT)

	# Arrows: biggest flows last so they sit on top. Thickness by the share of the largest.
	var biggest := 0.0
	for e: Dictionary in edges:
		if _drawn(e):
			biggest = maxf(biggest, float(e["energy"]))
	var focus := hover_id if hover_id >= 0 else selected_id
	for idx in range(edges.size() - 1, -1, -1):
		var e: Dictionary = edges[idx]
		if not _drawn(e):
			continue
		var touches := focus >= 0 and (int(e["to"]) == focus or int(e["from"]) == focus)
		if not touches and float(e["energy"]) < MINOR * float(intake_of.get(int(e["to"]), 0.0)):
			continue
		var a := _from_point(e)
		var b: Vector2 = pos[int(e["to"])]
		var w := maxf(1.0, roundf(sqrt(float(e["energy"]) / maxf(biggest, 1e-9)) * 8.0))
		var col: Color = KIND_COLOR.get(e["kind"], FAINT)
		col.a = 0.8 if (focus < 0 or touches) else 0.1
		if int(e["from"]) == int(e["to"]):
			# Eating its own kind: a small square loop above the node.
			var r: float = radius[int(e["to"])]
			draw_rect(Rect2(b + Vector2(-r, -r - 10), Vector2(2 * r, 8)), col, false, w)
			continue
		var end := b - (b - a).normalized() * (float(radius[int(e["to"])]) + 2.0)
		draw_line(a, end, col, w)
		# Arrowhead: a short bar across the line at its end, pixel-plain.
		var d := (end - a).normalized()
		var side := Vector2(-d.y, d.x) * (w + 3.0)
		draw_line(end - d * 4.0 + side, end - d * 4.0 - side, col, maxf(1.0, w * 0.5))

	# Sources.
	for kind: String in source_pos:
		var p: Vector2 = source_pos[kind]
		draw_rect(Rect2(p - Vector2(6, 6), Vector2(12, 12)), KIND_COLOR[kind])
		draw_string(font, p + Vector2(12, 5), SOURCE_NAMES[kind], HORIZONTAL_ALIGNMENT_LEFT, -1, 12, INK)

	# Species.
	for n: Dictionary in nodes.slice(0, shown):
		var id := int(n["id"])
		var p: Vector2 = pos[id]
		var r: float = radius[id]
		var col := Color.from_hsv(float(n["hue"]), 0.62, 0.92)
		var dim := focus >= 0 and id != focus and not _linked(id, focus)
		if dim:
			col = col.lerp(BG, 0.7)
		draw_rect(Rect2(p - Vector2(r, r), Vector2(2 * r, 2 * r)), col)
		if id == selected_id:
			draw_rect(Rect2(p - Vector2(r + 3, r + 3), Vector2(2 * r + 6, 2 * r + 6)), INK, false, 1.0)
		draw_string(font, p + Vector2(r + 5, 5), str(n["name"]), HORIZONTAL_ALIGNMENT_LEFT, -1, 12, FAINT if dim else INK)

	_draw_footer(font)


func _draw_footer(font: Font) -> void:
	draw_rect(Rect2(0, size.y - FOOT, size.x, FOOT), BAR)
	var lines := _describe(hover_id if hover_id >= 0 else selected_id)
	for j in lines.size():
		draw_string(font, Vector2(PAD, size.y - FOOT + 20 + j * 18), lines[j], HORIZONTAL_ALIGNMENT_LEFT, size.x - 2 * PAD, 12, INK if j == 0 else FAINT)


## What a species eats and what eats it, in words, for the footer.
func _describe(id: int) -> PackedStringArray:
	var out := PackedStringArray()
	if id < 0:
		out.append("Hover a species to see what it lives on. Arrows: red drained alive, olive leavings, gold sun, orange vents, grey the dead, brown loose chemicals.")
		return out
	var node := _node(id)
	if node.is_empty():
		return out
	out.append("%s: trophic level %.1f, %d alive, takes in %.1f energy a tick" % [node["name"], node["level"], node["alive"], node["intake"]])
	var eats := []
	var eaten_by := []
	for e: Dictionary in edges:
		if int(e["to"]) == id:
			eats.append(e)
		elif int(e["from"]) == id and int(e["to"]) != id:
			eaten_by.append(e)
	var total := maxf(float(node["intake"]), 1e-9)
	var parts := PackedStringArray()
	for e: Dictionary in eats.slice(0, 5):
		parts.append("%d%% %s" % [roundi(100.0 * float(e["energy"]) / total), _from_words(e)])
	out.append("Lives on: " + (", ".join(parts) if not parts.is_empty() else "nothing yet"))
	parts = PackedStringArray()
	for e: Dictionary in eaten_by.slice(0, 5):
		var taker := _node(int(e["to"]))
		parts.append("%s (%s)" % [taker.get("name", "?"), "drains it alive" if e["kind"] == "living" else "takes up its leavings"])
	out.append("Feeds: " + (", ".join(parts) if not parts.is_empty() else "nobody, as far as the flows show"))
	return out


func _from_words(e: Dictionary) -> String:
	var from := int(e["from"])
	if from < 0:
		return {"sun": "sunlight", "vent": "vents", "dead": "the dead", "loose": "loose chemicals"}.get(e["kind"], "?")
	var name: String = _node(from).get("name", "?")
	if from == int(e["to"]):
		return "its own kind" + (" (leavings)" if e["kind"] == "leavings" else " (alive)")
	return name + (" alive" if e["kind"] == "living" else "'s leavings")


func _node(id: int) -> Dictionary:
	for n: Dictionary in nodes:
		if int(n["id"]) == id:
			return n
	return {}


func _drawn(e: Dictionary) -> bool:
	if not pos.has(int(e["to"])):
		return false
	var from := int(e["from"])
	return pos.has(from) if from >= 0 else source_pos.has(e["kind"])


func _from_point(e: Dictionary) -> Vector2:
	var from := int(e["from"])
	if from < 0:
		return source_pos[e["kind"]] + Vector2(6, 0)
	return pos[from]


func _linked(a: int, b: int) -> bool:
	for e: Dictionary in edges:
		if (int(e["from"]) == a and int(e["to"]) == b) or (int(e["from"]) == b and int(e["to"]) == a):
			return true
	return false


func _species_at(p: Vector2) -> int:
	for id: int in pos:
		var r: float = radius[id] + 3.0
		if absf(p.x - pos[id].x) <= r and absf(p.y - pos[id].y) <= r:
			return id
	return -1


func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		mouse = event.position
		var h := _species_at(mouse)
		if h != hover_id:
			hover_id = h
			queue_redraw()
	elif event is InputEventMouseButton and event.pressed:
		match event.button_index:
			MOUSE_BUTTON_LEFT:
				grab_focus()
				var id := _species_at(event.position)
				if id >= 0:
					selected_id = id
					selection_changed.emit(id)
					queue_redraw()
			MOUSE_BUTTON_WHEEL_UP:
				shown = mini(shown + 5, maxi(nodes.size(), 5))
				_layout()
				queue_redraw()
			MOUSE_BUTTON_WHEEL_DOWN:
				shown = maxi(shown - 5, 5)
				_layout()
				queue_redraw()
	elif event is InputEventKey and event.pressed:
		if event.keycode == KEY_ESCAPE or event.keycode == KEY_G:
			close_requested.emit()
			accept_event()
