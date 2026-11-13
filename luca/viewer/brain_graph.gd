extends Control
## The creature designer's brain editor: the evolved CTRNN laid out as a graph.
##
## Senses on the left, hidden neurons in the middle, urges on the right.
##   Drag from a sense or a neuron to an urge or a neuron: a new link (+1).
##   Click a link to pick it; wheel = its weight; right-click or Delete removes it.
##   Click a neuron to pick it (its time constant and bias are in the panel);
##   right-click removes it.
## Links are green when they push (+) and red when they pull back (-), thicker
## the stronger. Node colours show what the brain answers to the senses set in
## the "Try it" panel (LucaWorld.probe_brain runs the real network).

signal edited(merge_key: String)
signal selection_changed(kind: String, index: int)   # "link", "neuron" or ""

const INPUT_LABELS := ["Always on", "Hunger", "Light here", "Crowded", "Too cold", "Dead here", "Own speed", "Eating", "Being eaten", "Noise",
		"Eyes: light", "Eyes: light L-R", "Eyes: warmth", "Eyes: warmth L-R", "Eyes: the dead", "Eyes: dead L-R", "Eyes: soil", "Eyes: soil L-R",
		"Eyes: loose B", "Eyes: B L-R", "Eyes: loose C", "Eyes: C L-R", "Eyes: loose D", "Eyes: D L-R", "Eyes: the living", "Eyes: living L-R"]
const OUTPUT_LABELS := ["Seek light", "Seek the living", "Seek the dead", "Seek comfort", "Seek rich soil", "Seek kin",
		"Seek bigger", "Seek movers", "Wander", "Keep going", "Speed", "Thrust", "Turn", "Mouth"]
const OUTPUT_NEEDS_EYES := 8   # the first eight urges lean along what a sensor feels
const BG := Color(0.06, 0.068, 0.09)
const BOX := Color(0.13, 0.15, 0.19)
const INK := Color(0.92, 0.94, 0.97)
const FAINT := Color(0.58, 0.63, 0.71)
const PLUS := Color(0.40, 0.85, 0.45)
const MINUS := Color(0.95, 0.40, 0.38)

var genome: Dictionary = {}
var layout: Dictionary = {}
var inputs := PackedFloat32Array()      # the senses being tried, NUM_INPUTS values
var outputs := PackedFloat32Array()     # what the brain answered
var hidden_fire := PackedFloat32Array()
var has_sensor := false
var has_motor := false
var sel_kind := ""
var sel_index := -1
var drag_from := -1
var drag_at := Vector2.ZERO
var drag_start := Vector2.ZERO
var drag_far := false                    # dragged well away: back onto the same neuron makes a self-loop
var hover_node := -1
var node_pos: Dictionary = {}           # node number -> Rect2 (boxes) or centre (neurons, as a Rect2 of size 0)


func _ready() -> void:
	clip_contents = true
	focus_mode = Control.FOCUS_CLICK
	resized.connect(queue_redraw)


func _n_in() -> int: return (layout.get("inputs", []) as PackedStringArray).size()
func _n_out() -> int: return (layout.get("outputs", []) as PackedStringArray).size()
func _out_start() -> int: return layout.get("out_start", 0)   # urges before this are not shown (the sensor brain reads only thrust and turn)
func _first_out() -> int: return layout.get("first_output", 9)
func _first_hidden() -> int: return layout.get("first_hidden", 20)
func _hidden() -> int: return genome.get("hidden", 0)


func _layout_nodes() -> void:
	node_pos.clear()
	var n_in := _n_in()
	var n_out := _n_out()
	var top := 34.0
	var room := size.y - top - 12.0
	var w_in := 118.0
	var w_out := 128.0
	var row_in := room / maxf(n_in, 1)
	for i in n_in:
		node_pos[i] = Rect2(12.0, top + row_in * i + row_in * 0.5 - 13.0, w_in, 26.0)
	var shown := n_out - _out_start()
	var row_out := room / maxf(shown, 1)
	for o in range(_out_start(), n_out):
		var slot := o - _out_start()
		node_pos[_first_out() + o] = Rect2(size.x - 12.0 - w_out, top + row_out * slot + row_out * 0.5 - 13.0, w_out, 26.0)
	# Hidden neurons in one or two columns in the middle.
	var h := _hidden()
	var cols := 1 if h <= 4 else 2
	var per_col := ceili(float(h) / cols)
	var mid := size.x * 0.5
	for k in h:
		var c := k / maxi(per_col, 1)
		var r := k % maxi(per_col, 1)
		var x := mid + (c - (cols - 1) * 0.5) * 120.0
		var y := top + room * (r + 0.5) / maxf(per_col, 1)
		node_pos[_first_hidden() + k] = Rect2(Vector2(x, y), Vector2.ZERO)


func _is_neuron(node: int) -> bool:
	return node >= _first_hidden()


## Where a link leaves a node (right side) or arrives (left side).
func _port(node: int, leaving: bool) -> Vector2:
	var r: Rect2 = node_pos.get(node, Rect2())
	if _is_neuron(node):
		return r.position + Vector2(18.0 if leaving else -18.0, 0.0)
	return Vector2(r.end.x if leaving else r.position.x, r.position.y + r.size.y * 0.5)


func _curve(from: int, to: int) -> PackedVector2Array:
	var a := _port(from, true)
	var b := _port(to, false)
	var pts := PackedVector2Array()
	if from == to:   # a neuron feeding itself: a loop over its head
		var c: Vector2 = (node_pos[from] as Rect2).position
		for i in 25:
			var t := PI * 0.15 + TAU * 0.7 * i / 24.0
			pts.append(c + Vector2(cos(t + PI * 0.5), -absf(sin(t + PI * 0.5))) * 22.0 + Vector2(0, -14))
		return pts
	var bend := maxf(40.0, absf(b.x - a.x) * 0.45)
	var c1 := a + Vector2(bend, 0)
	var c2 := b - Vector2(bend, 0)
	for i in 21:
		var t := i / 20.0
		var u := 1.0 - t
		pts.append(u * u * u * a + 3.0 * u * u * t * c1 + 3.0 * u * t * t * c2 + t * t * t * b)
	return pts


func _value_of(node: int) -> float:
	if node < _n_in():
		return inputs[node] if node < inputs.size() else 0.0
	if _is_neuron(node):
		var k := node - _first_hidden()
		return hidden_fire[k] if k < hidden_fire.size() else 0.0
	var o := node - _first_out()
	return outputs[o] if o < outputs.size() else 0.0


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), BG)
	if genome.is_empty():
		return
	_layout_nodes()
	var font := get_theme_default_font()
	draw_string(font, Vector2(14, 20), "SENSES", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, FAINT)
	draw_string(font, Vector2(size.x * 0.5 - 40, 20), "HIDDEN NEURONS", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, FAINT)
	draw_string(font, Vector2(size.x - 140, 20), "URGES", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, FAINT)
	if not has_motor:
		draw_string(font, Vector2(size.x * 0.5 - 150, size.y - 14),
			"No tail or legs: a brain only drives a motor, so this one does nothing yet.", HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color(1.0, 0.7, 0.35))

	# Links first, under the nodes.
	var links: Array = genome.get("links", [])
	var mw: float = layout.get("max_weight", 3.0)
	for i in links.size():
		var l: Dictionary = links[i]
		if not node_pos.has(int(l["from"])) or not node_pos.has(int(l["to"])):
			continue
		var w: float = l["weight"]
		var col := PLUS if w >= 0.0 else MINUS
		col.a = 0.35 + 0.6 * minf(absf(w) / mw, 1.0)
		var width := 1.0 + 3.0 * minf(absf(w) / mw, 1.0)
		var pts := _curve(l["from"], l["to"])
		if sel_kind == "link" and sel_index == i:
			draw_polyline(pts, Color(1, 1, 1, 0.9), width + 4.0, true)
		draw_polyline(pts, col, width, true)
		# The weight, at the middle of the curve.
		var mid := pts[pts.size() / 2]
		if sel_kind == "link" and sel_index == i or absf(w) >= 0.05:
			draw_string(font, mid + Vector2(-12, -5), "%+.1f" % w, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color(col, 1.0))

	# The link being drawn.
	if drag_from >= 0:
		draw_line(_port(drag_from, true), drag_at, Color(1, 1, 1, 0.7), 2.0, true)

	# Nodes.
	for i in _n_in():
		_draw_box(i, INPUT_LABELS[i] if i < INPUT_LABELS.size() else str(i), false)
	for o in range(_out_start(), _n_out()):
		var label: String = OUTPUT_LABELS[o] if o < OUTPUT_LABELS.size() else str(o)
		_draw_box(_first_out() + o, label, o < OUTPUT_NEEDS_EYES and not has_sensor)
	var taus: Array = genome.get("tau", [])
	for k in _hidden():
		var node := _first_hidden() + k
		var c: Vector2 = (node_pos[node] as Rect2).position
		var v := _value_of(node)
		var fill := BOX.lerp(PLUS if v >= 0.0 else MINUS, clampf(absf(v), 0.0, 1.0) * 0.8)
		draw_circle(c, 18.0, fill)
		var ring := Color(1, 1, 1, 0.95) if (sel_kind == "neuron" and sel_index == k) else (Color(1, 1, 1, 0.5) if hover_node == node else FAINT)
		draw_circle(c, 18.0, ring, false, 2.0 if ring.a > 0.9 else 1.0, true)
		draw_string(font, c + Vector2(-8, 4), "h%d" % k, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, INK)
		var tau: float = taus[k] if k < taus.size() else 1.0
		draw_string(font, c + Vector2(-26, 32), "τ %s" % ("%.0f" % tau if tau >= 10.0 else "%.1f" % tau), HORIZONTAL_ALIGNMENT_LEFT, 52, 10, FAINT)


func _draw_box(node: int, label: String, dim: bool) -> void:
	var r: Rect2 = node_pos[node]
	var font := get_theme_default_font()
	var v := _value_of(node)
	var fill := BOX.lerp(PLUS if v >= 0.0 else MINUS, clampf(absf(v) / 3.0 if node >= _first_out() else absf(v), 0.0, 1.0) * 0.7)
	draw_rect(r, fill)
	var edge := Color(1, 1, 1, 0.55) if hover_node == node else Color(1, 1, 1, 0.12)
	draw_rect(r, edge, false, 1.0)
	var col := FAINT if dim else INK
	draw_string(font, r.position + Vector2(8, 17), label, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 34, 12, col)
	# The value, right-aligned.
	var text := ("%.2f" % v) if node < _first_out() else ("%+.1f" % v)
	draw_string(font, Vector2(r.end.x - 30, r.position.y + 17), text, HORIZONTAL_ALIGNMENT_RIGHT, 26, 10, FAINT)
	if dim:
		draw_string(font, Vector2(r.position.x - 14, r.position.y + 17), "👁", HORIZONTAL_ALIGNMENT_LEFT, -1, 10, Color(1.0, 0.7, 0.35))


# --- Input ------------------------------------------------------------------------------

func _node_at(p: Vector2) -> int:
	for node: int in node_pos:
		var r: Rect2 = node_pos[node]
		if _is_neuron(node):
			if r.position.distance_to(p) < 20.0:
				return node
		elif r.grow(3.0).has_point(p):
			return node
	return -1


func _link_at(p: Vector2) -> int:
	var links: Array = genome.get("links", [])
	var best := -1
	var best_d := 7.0
	for i in links.size():
		var l: Dictionary = links[i]
		if not node_pos.has(int(l["from"])) or not node_pos.has(int(l["to"])):
			continue
		var pts := _curve(l["from"], l["to"])
		for j in pts.size() - 1:
			var d := Geometry2D.get_closest_point_to_segment(p, pts[j], pts[j + 1]).distance_to(p)
			if d < best_d:
				best_d = d
				best = i
	return best


func _is_source(node: int) -> bool:
	return node >= 0 and (node < _n_in() or _is_neuron(node))


func _is_target(node: int) -> bool:
	return node >= _first_out()


func _gui_input(event: InputEvent) -> void:
	if genome.is_empty():
		return
	if event is InputEventMouseMotion:
		var node := _node_at(event.position)
		if node != hover_node:
			hover_node = node
			queue_redraw()
		if drag_from >= 0:
			drag_at = event.position
			drag_far = drag_far or drag_at.distance_to(drag_start) > 30.0
			queue_redraw()
	elif event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
			grab_focus()
			var node := _node_at(event.position)
			if _is_source(node):
				drag_from = node
				drag_at = event.position
				drag_start = event.position
				drag_far = false
			if _is_neuron(node):
				_select("neuron", node - _first_hidden())
			elif node < 0:
				var link := _link_at(event.position)
				_select("link" if link >= 0 else "", link)
			accept_event()
		elif event.button_index == MOUSE_BUTTON_LEFT and not event.pressed and drag_from >= 0:
			var to := _node_at(event.position)
			# A press and release on one neuron is a click (pick it), not a
			# link to itself: drag out and back in for a self-loop.
			if _is_target(to) and (to != drag_from or drag_far):
				connect_nodes(drag_from, to, 1.0)
			drag_from = -1
			queue_redraw()
			accept_event()
		elif event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
			var node := _node_at(event.position)
			if _is_neuron(node):
				remove_neuron(node - _first_hidden())
			else:
				var link := _link_at(event.position)
				if link >= 0:
					remove_link(link)
			accept_event()
		elif event.pressed and event.button_index in [MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN] and sel_kind == "link":
			nudge_weight(sel_index, 0.1 if event.button_index == MOUSE_BUTTON_WHEEL_UP else -0.1)
			accept_event()


func _select(kind: String, index: int) -> void:
	sel_kind = kind
	sel_index = index
	selection_changed.emit(kind, index)
	queue_redraw()


# --- Editing (also called from the designer's panel) ----------------------------------

func connect_nodes(from: int, to: int, weight: float) -> void:
	var links: Array = genome["links"]
	for i in links.size():
		if int(links[i]["from"]) == from and int(links[i]["to"]) == to:
			_select("link", i)
			return
	if links.size() >= int(layout.get("max_links", 48)):
		edited.emit("full")
		return
	links.append({"from": from, "to": to, "weight": weight})
	_select("link", links.size() - 1)
	edited.emit("")


func remove_link(i: int) -> void:
	var links: Array = genome["links"]
	if i < 0 or i >= links.size():
		return
	links.remove_at(i)
	_select("", -1)
	edited.emit("")


func nudge_weight(i: int, by: float) -> void:
	var links: Array = genome["links"]
	if i < 0 or i >= links.size():
		return
	var mw: float = layout.get("max_weight", 3.0)
	links[i]["weight"] = clampf(snappedf(links[i]["weight"] + by, 0.05), -mw, mw)
	edited.emit("weight%d" % i)
	queue_redraw()


func add_neuron() -> void:
	var h := _hidden()
	if h >= int(layout.get("max_hidden", 8)):
		edited.emit("full")
		return
	genome["hidden"] = h + 1
	(genome["tau"] as Array).append(10.0)
	(genome["bias"] as Array).append(0.0)
	_select("neuron", h)
	edited.emit("")


func remove_neuron(k: int) -> void:
	var h := _hidden()
	if k < 0 or k >= h:
		return
	var gone := _first_hidden() + k
	var kept: Array = []
	for l: Dictionary in genome["links"]:
		if int(l["from"]) == gone or int(l["to"]) == gone:
			continue
		if int(l["from"]) > gone:
			l["from"] = int(l["from"]) - 1
		if int(l["to"]) > gone:
			l["to"] = int(l["to"]) - 1
		kept.append(l)
	genome["links"] = kept
	(genome["tau"] as Array).remove_at(k)
	(genome["bias"] as Array).remove_at(k)
	genome["hidden"] = h - 1
	_select("", -1)
	edited.emit("")
