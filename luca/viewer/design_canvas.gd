extends Control
## The creature designer's body canvas: the design drawn big by the world's
## own sprite shader, with tools on top.
##
##   Select  click a part to pick it; drag it round the rim; drag a mouth or a
##           sun patch outward to put it on a stalk; wheel = its size;
##           right-click or Delete removes it
##   Pen     pull the outline: the nose or tail for length, the sides for
##           width, a corner for how boxy it is; wheel = overall size
##   Stamp   click the rim to add the chosen part there (mirrored if Mirror is on)
##
## Everything edits the genome Dictionary shared with designer.gd and is
## clamped in C++ (LucaWorld.clamp_design); the canvas never decides a rule.
## Geometry here mirrors blob.gdshader and core/body.hpp so the handles sit on
## the drawn outline and the arcs match what the body really covers.

signal edited(merge_key: String)            # the genome changed (merge_key lets undo coalesce a drag)
signal selection_changed(index: int)

const PX_SIZE := 0.125
const PX_PER_RADIUS := 4.5
const FULL := 0.6                           # fullness at which blob.gdshader draws the radius as-is
enum { PHOTO, ABSORBER, FLAGELLUM, LEGS, SENSOR, ARMOUR, STORAGE, POISON }
const PART_COLORS := [Color(0.36, 0.72, 0.22), Color(0.86, 0.22, 0.22), Color(0.85, 0.72, 0.50), Color(0.70, 0.50, 0.30),
		Color(0.95, 0.95, 0.95), Color(0.62, 0.65, 0.72), Color(0.97, 0.88, 0.55), Color(0.80, 0.35, 0.98)]
const DEFAULT_SIZE := [0.8, 0.5, 1.0, 1.0, 0.5, 0.75, 1.0, 0.5]
const COVER_ARC := PI * 0.5
const MOUTH_ARC_BASE := 0.5
const MOUTH_ARC_PER_SIZE := 0.8

var world: Object
var genome: Dictionary = {}
var layout: Dictionary = {}
var stage: SubViewportContainer
var overlay: Control
var tool := "select"                        # "select", "pen" or "stamp"
var stamp_type := PHOTO
var mirror := true
var selected := -1
var hover := Vector2(INF, INF)              # mouse, in sprite pixels from the centre
var drag: Dictionary = {}
var zoom := 6.0


func _ready() -> void:
	clip_contents = true
	focus_mode = Control.FOCUS_CLICK
	stage = SubViewportContainer.new()
	stage.set_script(load("res://sprite_stage.gd"))
	stage.set_anchors_preset(Control.PRESET_FULL_RECT)
	stage.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(stage)
	stage.set_ground(true)
	overlay = Control.new()
	overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
	overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	overlay.draw.connect(_draw_overlay)
	add_child(overlay)
	resized.connect(func() -> void:
		stage.look(Vector2.ZERO, zoom)
		overlay.queue_redraw())


# --- Showing the design ----------------------------------------------------------

func refresh() -> void:
	if world == null or genome.is_empty():
		return
	var s: Dictionary = world.design_sprite(genome, FULL)
	stage.show_sprites(s["instances"], s["parts"], 1, 12)
	overlay.queue_redraw()


## Pick a whole-pixel zoom that fits the body and everything it holds out.
func fit() -> void:
	var reach := _reach_px()
	var room := minf(size.x, size.y)
	if room <= 0.0:
		return
	zoom = clampf(floorf(room * 0.40 / reach), 2.0, 14.0)
	stage.look(Vector2(PX_SIZE * 0.5, PX_SIZE * 0.5), zoom)   # sprites snap to the middle of a pixel
	overlay.queue_redraw()


func _reach_px() -> float:
	var a := _axes()
	var reach := maxf(a.x, a.y) + 10.0
	for p: Dictionary in genome.get("parts", []):
		var stalk: float = p["stalk"] / PX_SIZE
		var t: int = p["type"]
		var extra := stalk + (4.0 if t == ABSORBER or t == PHOTO else 0.0)
		if t == FLAGELLUM:
			extra = 2.0 + minf(p["size"], 1.5) * 4.0
		if t == LEGS:
			extra = 1.0 + minf(p["size"], 1.5) * 2.0
		reach = maxf(reach, maxf(a.x, a.y) + extra + 4.0)
	return reach


# --- Geometry (sprite pixels, body frame = screen frame: front is +x) -------------

func _axes() -> Vector2:
	var r: float = genome.get("radius", 1.0) * PX_PER_RADIUS
	var k := 1.0 + 2.0 * clampf(genome.get("elongation", 0.0), 0.0, 1.0)
	return Vector2(r * pow(k, 2.0 / 3.0), r * pow(k, -1.0 / 3.0))


func _m() -> float:
	return 2.0 + 4.0 * clampf(genome.get("squareness", 0.0), 0.0, 1.0)


## Distance from the centre to the outline at angle theta.
func _rim(theta: float) -> float:
	var a := _axes()
	var m := _m()
	var u := Vector2(absf(cos(theta)) / a.x, absf(sin(theta)) / a.y)
	var rho := pow(pow(u.x, m) + pow(u.y, m), 1.0 / m)
	return 1.0 / maxf(rho, 1e-4)


func _to_screen(p: Vector2) -> Vector2:
	return size * 0.5 + (p + Vector2(0.5, 0.5)) * zoom


func _to_px(screen: Vector2) -> Vector2:
	return (screen - size * 0.5) / zoom - Vector2(0.5, 0.5)


## Where part k is drawn: its base on the rim, and the tip of its stalk.
func _part_base(p: Dictionary) -> Vector2:
	if int(p["type"]) == STORAGE:
		return Vector2.ZERO
	var th: float = p["theta"]
	return Vector2(cos(th), sin(th)) * _rim(th)


func _part_marker(p: Dictionary) -> Vector2:
	var base := _part_base(p)
	var th: float = p["theta"]
	var stalk: float = p["stalk"] / PX_SIZE
	if stalk > 0.5:
		return base + Vector2(cos(th), sin(th)) * stalk
	return base


func _half_arc(p: Dictionary) -> float:
	var t: int = p["type"]
	var s: float = p["size"]
	if t == ABSORBER:
		return minf(PI, MOUTH_ARC_BASE + MOUTH_ARC_PER_SIZE * s)
	if t in [PHOTO, ARMOUR, POISON]:
		return minf(PI, COVER_ARC * s)
	return 0.0


## How well a motor at this angle pushes (core/body.cpp, thrust_facing).
static func thrust_facing(type: int, theta: float) -> float:
	if type == FLAGELLUM:
		return 0.25 + 0.75 * 0.5 * (1.0 - cos(theta))
	if type == LEGS:
		return 0.5 + 0.5 * absf(sin(theta))
	return 0.0


# --- Drawing the tools --------------------------------------------------------------

func _draw_overlay() -> void:
	if genome.is_empty():
		return
	var ink := Color(1, 1, 1, 0.85)
	var faint := Color(1, 1, 1, 0.25)
	var font := get_theme_default_font()
	# Which way is the front.
	var nose := _to_screen(Vector2(_rim(0.0) + 6.0, 0.0))
	overlay.draw_string(font, nose + Vector2(14, 5), "front", HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color(1, 1, 1, 0.45))
	overlay.draw_line(nose, nose + Vector2(10, 0), Color(1, 1, 1, 0.45), 2.0)
	overlay.draw_line(nose + Vector2(10, 0), nose + Vector2(5, -4), Color(1, 1, 1, 0.45), 2.0)
	overlay.draw_line(nose + Vector2(10, 0), nose + Vector2(5, 4), Color(1, 1, 1, 0.45), 2.0)

	if tool == "pen":
		_draw_outline(faint, 0.0)
		for h in _handles():
			var c := _to_screen(h["at"])
			var hot: bool = not drag.is_empty() and drag.get("handle", "") == h["kind"]
			overlay.draw_rect(Rect2(c - Vector2(5, 5), Vector2(10, 10)), Color(1.0, 0.85, 0.35) if hot else ink, true)
			overlay.draw_rect(Rect2(c - Vector2(5, 5), Vector2(10, 10)), Color(0, 0, 0, 0.6), false, 1.0)

	var parts: Array = genome.get("parts", [])
	for k in parts.size():
		var p: Dictionary = parts[k]
		var col: Color = PART_COLORS[int(p["type"])]
		var at := _to_screen(_part_marker(p))
		if k == selected:
			var half := _half_arc(p)
			if half > 0.0:
				_draw_arc_on_rim(p["theta"], half, Color(col, 0.75), 3.0)
			if int(p["type"]) == LEGS:
				overlay.draw_circle(_to_screen(Vector2(cos(-p["theta"]), sin(-p["theta"])) * _rim(-p["theta"])), 5.0, Color(col, 0.5), false, 2.0)
			overlay.draw_circle(at, 8.0, Color(1, 1, 1, 0.95), false, 2.0)
		overlay.draw_circle(at, 4.0, col)
		overlay.draw_circle(at, 4.0, Color(0, 0, 0, 0.7), false, 1.0)

	# The stamp: a ghost where the part would go.
	if tool == "stamp" and hover.x < INF:
		var th := atan2(hover.y, hover.x)
		var ghost := {"type": stamp_type, "size": DEFAULT_SIZE[stamp_type], "theta": th, "stalk": 0.0}
		var col: Color = PART_COLORS[stamp_type]
		var spots := [th]
		if _mirrors(stamp_type, th):
			spots.append(-th)
		for t: float in spots:
			ghost["theta"] = t
			var half := _half_arc(ghost)
			if half > 0.0:
				_draw_arc_on_rim(t, half, Color(col, 0.45), 3.0)
			overlay.draw_circle(_to_screen(_part_base(ghost)), 5.0, Color(col, 0.8))
		if stamp_type in [FLAGELLUM, LEGS]:
			var push := thrust_facing(stamp_type, th)
			overlay.draw_string(font, _to_screen(hover) + Vector2(12, -10), "push %d%%" % roundi(push * 100.0),
				HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color(1, 1, 1, 0.8))


func _draw_outline(col: Color, out: float) -> void:
	var pts := PackedVector2Array()
	for i in 73:
		var th := TAU * i / 72.0
		pts.append(_to_screen(Vector2(cos(th), sin(th)) * (_rim(th) + out)))
	overlay.draw_polyline(pts, col, 1.0)


func _draw_arc_on_rim(theta: float, half: float, col: Color, width: float) -> void:
	var pts := PackedVector2Array()
	var n := maxi(4, int(half * 24.0))
	for i in n + 1:
		var th: float = theta - half + 2.0 * half * i / n
		pts.append(_to_screen(Vector2(cos(th), sin(th)) * (_rim(th) + 2.5)))
	overlay.draw_polyline(pts, col, width)


func _handles() -> Array:
	var a := _axes()
	var t := pow(2.0, -1.0 / _m())
	return [
		{"kind": "length", "at": Vector2(a.x, 0)}, {"kind": "length", "at": Vector2(-a.x, 0)},
		{"kind": "width", "at": Vector2(0, a.y)}, {"kind": "width", "at": Vector2(0, -a.y)},
		{"kind": "corner", "at": Vector2(a.x * t, a.y * t)}, {"kind": "corner", "at": Vector2(-a.x * t, a.y * t)},
		{"kind": "corner", "at": Vector2(a.x * t, -a.y * t)}, {"kind": "corner", "at": Vector2(-a.x * t, -a.y * t)},
	]


func _mirrors(type: int, theta: float) -> bool:
	# Legs are already a pair, and fat sits in the middle.
	return mirror and type != LEGS and type != STORAGE and absf(sin(theta)) > 0.2


# --- Input --------------------------------------------------------------------------

func _gui_input(event: InputEvent) -> void:
	if genome.is_empty():
		return
	if event is InputEventMouseMotion:
		hover = _to_px(event.position)
		if not drag.is_empty():
			_drag_to(hover)
		overlay.queue_redraw()
	elif event is InputEventMouseButton and event.pressed:
		var at := _to_px(event.position)
		match event.button_index:
			MOUSE_BUTTON_LEFT:
				grab_focus()
				_press(at)
			MOUSE_BUTTON_RIGHT:
				var k := _part_under(event.position)
				if k >= 0:
					remove_part(k)
			MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN:
				var up: bool = event.button_index == MOUSE_BUTTON_WHEEL_UP
				if selected >= 0 and tool == "select":
					resize_part(selected, 1.08 if up else 1.0 / 1.08)
				else:
					genome["radius"] = clampf(genome["radius"] * (1.04 if up else 1.0 / 1.04), layout.get("min_radius", 0.3), layout.get("max_radius", 5.0))
					edited.emit("radius")
		accept_event()
	elif event is InputEventMouseButton and not event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		if not drag.is_empty():
			drag = {}
			var reach := _reach_px() * zoom
			if reach > minf(size.x, size.y) * 0.48 or reach < minf(size.x, size.y) * 0.18:
				fit()
			overlay.queue_redraw()


func _press(at: Vector2) -> void:
	if tool == "pen":
		var best := ""
		var best_d := 12.0 / zoom
		for h in _handles():
			var d: float = (h["at"] as Vector2).distance_to(at)
			if d < best_d:
				best_d = d
				best = h["kind"]
		if best == "":
			# Anywhere on the outline: the nearest handle's direction.
			var th := atan2(at.y, at.x)
			if absf(at.length() - _rim(th)) * zoom < 14.0:
				var c := absf(cos(th))
				var s := absf(sin(th))
				best = "length" if c > 0.92 else ("width" if s > 0.92 else "corner")
		if best != "":
			drag = {"handle": best}
		return
	if tool == "stamp":
		var th := atan2(at.y, at.x)
		add_part(stamp_type, th)
		return
	# Select.
	var k := _part_under(_to_screen(at))
	_select(k)
	if k >= 0:
		drag = {"part": k, "partner": _partner(k)}


func _part_under(screen: Vector2) -> int:
	var parts: Array = genome.get("parts", [])
	var best := -1
	var best_d := 11.0
	for k in parts.size():
		var d := _to_screen(_part_marker(parts[k])).distance_to(screen)
		if d < best_d:
			best_d = d
			best = k
	return best


## The part placed as part k's mirror image, if any (same type and size, -theta).
func _partner(k: int) -> int:
	var parts: Array = genome["parts"]
	var p: Dictionary = parts[k]
	if not _mirrors(p["type"], p["theta"]):
		return -1
	for j in parts.size():
		var q: Dictionary = parts[j]
		if j != k and q["type"] == p["type"] and absf(q["size"] - p["size"]) < 1e-3 and absf(angle_difference(q["theta"], -p["theta"])) < 0.05:
			return j
	return -1


func _drag_to(at: Vector2) -> void:
	if drag.has("handle"):
		var a := _axes()
		match drag["handle"]:
			"length": _set_axes(maxf(absf(at.x), 1.5), a.y)
			"width": _set_axes(a.x, maxf(absf(at.y), 1.5))
			"corner":
				var diag := Vector2(a.x, a.y)
				var t := clampf(Vector2(absf(at.x), absf(at.y)).dot(diag.normalized()) / diag.length(), pow(2.0, -0.5), pow(2.0, -1.0 / 6.0))
				var m := -log(2.0) / log(t)
				genome["squareness"] = clampf((m - 2.0) / 4.0, 0.0, 1.0)
		edited.emit("pen")
		return
	if drag.has("part"):
		var k: int = drag["part"]
		var parts: Array = genome["parts"]
		if k >= parts.size():
			return
		var p: Dictionary = parts[k]
		var th := atan2(at.y, at.x)
		if int(p["type"]) == STORAGE:
			return
		p["theta"] = th
		if int(p["type"]) in [PHOTO, ABSORBER]:
			var out := at.length() - _rim(th)
			p["stalk"] = clampf((out - 3.0) * PX_SIZE, 0.0, layout.get("max_stalk", 1.0)) if out > 3.0 else 0.0
		var j: int = drag.get("partner", -1)
		if j >= 0 and j < parts.size():
			parts[j]["theta"] = -th
			parts[j]["stalk"] = p["stalk"]
		edited.emit("move")


## Length and width of the outline (sprite pixels) -> radius and elongation,
## keeping the volume rule of blob.gdshader: axes r k^(2/3) and r k^(-1/3).
func _set_axes(length: float, width: float) -> void:
	var k := clampf(length / width, 1.0, 3.0)
	var r := pow(length * width * width, 1.0 / 3.0)
	if length < width:
		r = pow(width * width * width, 1.0 / 3.0) if width > length * 1.0 else r
		k = 1.0
	genome["elongation"] = (k - 1.0) / 2.0
	genome["radius"] = clampf(r / PX_PER_RADIUS, layout.get("min_radius", 0.3), layout.get("max_radius", 5.0))


# --- Editing parts (also called from the designer's panel) ------------------------

func add_part(type: int, theta: float) -> void:
	var parts: Array = genome["parts"]
	var max_parts: int = layout.get("max_parts", 12)
	if parts.size() >= max_parts:
		edited.emit("full")
		return
	if type == STORAGE:
		theta = 0.0
	parts.append({"type": type, "size": DEFAULT_SIZE[type], "theta": theta, "stalk": 0.0})
	var k := parts.size() - 1
	if _mirrors(type, theta) and parts.size() < max_parts:
		parts.append({"type": type, "size": DEFAULT_SIZE[type], "theta": -theta, "stalk": 0.0})
	_select(k)
	edited.emit("")


func remove_part(k: int) -> void:
	var parts: Array = genome["parts"]
	if k < 0 or k >= parts.size():
		return
	var j := _partner(k)
	parts.remove_at(k)
	if j >= 0:
		parts.remove_at(j if j < k else j - 1)
	_select(-1)
	edited.emit("")


func resize_part(k: int, factor: float) -> void:
	var parts: Array = genome["parts"]
	if k < 0 or k >= parts.size():
		return
	var j := _partner(k)
	var s := clampf(parts[k]["size"] * factor, layout.get("min_part", 0.05), layout.get("max_part", 4.0))
	parts[k]["size"] = s
	if j >= 0:
		parts[j]["size"] = s
	edited.emit("size%d" % k)


func _select(k: int) -> void:
	if k != selected:
		selected = k
		selection_changed.emit(k)
	overlay.queue_redraw()


func select(k: int) -> void:
	_select(k)
