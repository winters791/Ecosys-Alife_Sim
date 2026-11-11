extends Control
## The tree of life, drawn as a tree.
##
## Time runs upward from the ground. The trunk is the oldest species; every
## species that branched from it leaves the wood at the height (tick) it was
## born and climbs until now, or until it died out. A living branch ends in a
## crown of leaves in its species colour, bigger for a bigger population; a
## dead one is a bare grey stub. A branch is as thick as the life it carries.
##
## Hover a branch to read about it. Click it to zoom in: it becomes the trunk
## and only its descendants are shown. Click the trunk, or right-click
## anywhere, to step back out.
##
## Everything is drawn in chunky pixels (ART screen pixels each) from
## LucaWorld.get_species_tree(); nothing here feeds back into the world.

const ART := 3                # screen pixels per art pixel
const GROUND := 9             # art pixels of soil under the tree
const TOP_SPACE := 20         # art pixels of sky kept clear above the canopy
const SIDE := 14              # art pixels of margin left and right
const MAX_SLOT := 30.0        # widest gap between neighbouring branches, art pixels
const GROW_TIME := 0.45       # seconds a tree takes to grow in after a zoom

const SKY := [Color(0.07, 0.08, 0.14), Color(0.09, 0.11, 0.19), Color(0.12, 0.15, 0.25)]
const SOIL := Color(0.24, 0.17, 0.12)
const SOIL_DARK := Color(0.17, 0.12, 0.09)
const GRASS := Color(0.36, 0.62, 0.27)
const BARK := Color(0.45, 0.31, 0.20)
const DEAD := Color(0.36, 0.35, 0.39)
const INK := Color(0.93, 0.94, 0.96)
const FAINT := Color(0.62, 0.67, 0.75)

var species: Array = []       # as given by get_species_tree(), in drawing order
var now := 1
var root_id := -1             # the species shown as the trunk; -1 = the oldest
var hover := -1               # index into `species`, or -1
var clock := 0.0
var grown := 1.0              # 0..1 while a freshly shown tree grows in
var redraw_clock := 0.0

# Layout, rebuilt whenever the data or the zoom changes. Indexed like `species`.
var root := -1
var kids: Array[PackedInt32Array] = []
var shown := PackedByteArray()             # 1 if under the current trunk
var slot := PackedFloat32Array()           # horizontal position, in slots
var slots := 1.0                           # width of the whole tree, in slots
var carried := PackedInt32Array()          # living creatures on this branch and all above it
var t0 := 0                                # tick at the ground


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	clip_contents = true


func show_tree(tree: Array, tick: int) -> void:
	species = tree
	now = maxi(tick, 1)
	_layout()
	queue_redraw()


func _process(delta: float) -> void:
	if not is_visible_in_tree():
		return
	clock += delta
	grown = minf(grown + delta / GROW_TIME, 1.0)
	redraw_clock += delta
	if redraw_clock >= 0.08:   # leaves sway at about twelve frames a second
		redraw_clock = 0.0
		queue_redraw()


# --- Layout ------------------------------------------------------------------------

func _layout() -> void:
	var n := species.size()
	kids.clear()
	for i in n:
		kids.append(PackedInt32Array())
	root = -1
	for i in n:
		var p: int = species[i]["row_parent"]
		if p >= 0:
			kids[p].append(i)
		if species[i]["id"] == root_id:
			root = i
	if root < 0:
		root = 0 if n > 0 else -1
		root_id = -1
	shown.resize(n)
	shown.fill(0)
	slot.resize(n)
	carried.resize(n)
	if root < 0:
		return

	# Parents come before their children in the array, so a backwards pass
	# sums each branch's own life and everything growing from it.
	var width := PackedFloat32Array()
	width.resize(n)
	for i in range(n - 1, -1, -1):
		var w := 1.0
		var c: int = species[i]["population"]
		for k: int in kids[i]:
			w += width[k]
			c += carried[k]
		width[i] = w
		carried[i] = c

	# Each branch owns a run of slots. Its children are dealt to whichever
	# side is lighter; on each side the earliest born sits furthest out, so a
	# later branch never has to cross an older sibling.
	t0 = species[root]["born"]
	slots = width[root]
	var stack: Array = [[root, 0.0]]
	while not stack.is_empty():
		var item: Array = stack.pop_back()
		var i: int = item[0]
		var left_edge: float = item[1]
		shown[i] = 1
		var left: Array[int] = []
		var right: Array[int] = []
		var lw := 0.0
		var rw := 0.0
		for k: int in kids[i]:
			if lw <= rw:
				left.append(k)
				lw += width[k]
			else:
				right.append(k)
				rw += width[k]
		var x := left_edge
		for k in left:
			stack.append([k, x])
			x += width[k]
		slot[i] = x + 0.5
		x = left_edge + width[i]
		for k in right:
			x -= width[k]
			stack.append([k, x])
	if hover >= n or (hover >= 0 and shown[hover] == 0):
		hover = -1


## Art-pixel grid size.
func _cols() -> int:
	return int(size.x) / ART

func _rows() -> int:
	return int(size.y) / ART

func _ground_y() -> int:
	return _rows() - GROUND

func _x(i: int) -> int:
	var usable := float(_cols() - 2 * SIDE)
	var step := minf(usable / slots, MAX_SLOT)
	return int(round(_cols() * 0.5 + (slot[i] - slots * 0.5) * step))

func _y(tick: int) -> int:
	var span := float(maxi(now - t0, 1))
	var height := float(_ground_y() - TOP_SPACE) * grown
	return _ground_y() - int(round(height * clampf(float(tick - t0) / span, 0.0, 1.0)))

func _alive(i: int) -> bool:
	return species[i]["extinct"] < 0

func _end_tick(i: int) -> int:
	return now if _alive(i) else int(species[i]["extinct"])

func _thickness(i: int) -> int:
	return clampi(1 + int(log(1.0 + carried[i]) / log(5.0)), 1, 5)

func _wood(i: int) -> Color:
	if carried[i] == 0:
		return DEAD
	var c := BARK.lerp(_leaf(i), 0.35 if _alive(i) else 0.0)
	return c.lightened(0.35) if i == hover else c

func _leaf(i: int) -> Color:
	return Color.from_hsv(species[i]["hue"], 0.62, 0.92)

## Where a branch turns from reaching outward to climbing: (x, y) in art pixels.
func _elbow(i: int) -> Vector2i:
	var parent: int = species[i]["row_parent"]
	var yb := _y(species[i]["born"])
	var ye := _y(_end_tick(i))
	var reach := absi(_x(i) - _x(parent))
	return Vector2i(_x(i), yb - mini(mini(reach / 2, 36), maxi(yb - ye, 0)))


# --- Drawing -----------------------------------------------------------------------

func _px(x: int, y: int, w: int, h: int, color: Color) -> void:
	draw_rect(Rect2(x * ART, y * ART, w * ART, h * ART), color)

func _hash(a: int, b: int) -> float:
	var h := (a * 374761393 + b * 668265263) & 0x7fffffff
	h = ((h ^ (h >> 13)) * 1274126177) & 0x7fffffff
	return float(h & 0xffff) / 65536.0


func _draw() -> void:
	var cols := _cols()
	var rows := _rows()
	var ground := _ground_y()

	# Sky in three flat bands with a dithered seam, and a few fixed stars.
	var band := ground / 3
	for b in 3:
		_px(0, b * band, cols, band + (ground - 3 * band if b == 2 else 0), SKY[b])
		if b > 0:
			for x in range(b % 2, cols, 2):
				_px(x, b * band - 1, 1, 1, SKY[b])
	for s in 70:
		var sx := int(_hash(s, 11) * cols)
		var sy := int(_hash(s, 29) * (ground - 30))
		if _hash(s, int(clock * 0.7) + 5) > 0.12:   # an occasional twinkle
			_px(sx, sy, 1, 1, Color(0.80, 0.84, 0.95, 0.55))

	# The ground.
	_px(0, ground, cols, GROUND, SOIL)
	for x in cols:
		if _hash(x, 3) < 0.35:
			_px(x, ground + 2 + int(_hash(x, 7) * (GROUND - 3)), 1, 1, SOIL_DARK)
		_px(x, ground - (1 if _hash(x, 5) < 0.3 else 0), 1, 2, GRASS)

	if root < 0:
		draw_string(get_theme_default_font(), Vector2(16, 30), "Nothing has taken hold yet.", HORIZONTAL_ALIGNMENT_LEFT, -1, 14, FAINT)
		return

	# Height marks: when things happened.
	var font := get_theme_default_font()
	for k in range(1, 5):
		var tick := t0 + int((now - t0) * k / 4.0)
		var y := _y(tick)
		for x in range(2, cols - 2, 4):
			_px(x, y, 1, 1, Color(1, 1, 1, 0.07))
		draw_string(font, Vector2(6, y * ART - 3), "tick " + _short(tick), HORIZONTAL_ALIGNMENT_LEFT, -1, 10, Color(0.55, 0.60, 0.70, 0.8))

	# Wood: thin branches first, so thick ones sit in front.
	var order: Array[int] = []
	for i in species.size():
		if shown[i] == 1:
			order.append(i)
	order.sort_custom(func(a: int, b: int) -> bool: return carried[a] < carried[b] if carried[a] != carried[b] else a > b)
	for i in order:
		_draw_branch(i)
	# Leaves over all the wood, small crowns last so they are not buried.
	order.reverse()
	for i in order:
		if _alive(i):
			_draw_crown(i)
	_draw_labels(font, order)
	_draw_card(font)


func _draw_branch(i: int) -> void:
	var wood := _wood(i)
	var shade := wood.darkened(0.3)
	var t := _thickness(i)
	var x := _x(i)
	var top := _y(_end_tick(i))
	if i == root:
		# The trunk, with a flared foot and two roots.
		var ground := _ground_y()
		_px(x - t / 2, top, t, ground - top, wood)
		_px(x - t / 2 + t - 1, top, 1, ground - top, shade)
		_px(x - t / 2 - 1, ground - 3, t + 2, 3, wood)
		_px(x - t / 2 - 2, ground - 1, t + 4, 2, shade)
		return
	var parent: int = species[i]["row_parent"]
	var x0 := _x(parent)
	var y0 := _y(species[i]["born"])
	var elbow := _elbow(i)
	# Reaching outward: a stepped diagonal from the parent's wood to the elbow.
	var steps := absi(elbow.x - x0)
	var dir := signi(elbow.x - x0)
	for s in steps + 1:
		var y := y0 - int(round(float(y0 - elbow.y) * s / maxf(steps, 1.0)))
		_px(x0 + dir * s, y - t / 2, 1, t, wood)
		_px(x0 + dir * s, y - t / 2 + t - 1, 1, 1, shade)
	# Climbing.
	if elbow.y > top:
		_px(x - t / 2, top, t, elbow.y - top + 1, wood)
		if t > 1:
			_px(x - t / 2 + t - 1, top, 1, elbow.y - top + 1, shade)
	if not _alive(i):
		_px(x, top - 1, 1, 1, wood.lightened(0.25))   # a snapped tip


func _draw_crown(i: int) -> void:
	var leaf := _leaf(i)
	if i == hover:
		leaf = leaf.lightened(0.3)
	var light := leaf.lightened(0.28)
	var dark := leaf.darkened(0.32)
	var r := clampi(2 + int(sqrt(float(species[i]["population"])) / 3.0), 2, 10)
	var cx := _x(i)
	var cy := _y(now) - r / 2
	var id: int = species[i]["id"]
	var sway := int(round(sin(clock * 1.4 + id * 1.7)))
	for dy in range(-r, r + 1):
		var half := int(sqrt(float(r * r - dy * dy)))
		half += int(_hash(id, dy + 40) * 2.0) - (1 if r > 2 else 0)   # a ragged, leafy edge
		if half < 0:
			continue
		var ox := cx + (sway if dy < 0 else 0)   # only the top of the crown moves
		_px(ox - half, cy + dy, 2 * half + 1, 1, leaf)
		if dy < 0 and half > 1:
			_px(ox - half + 1, cy + dy, half, 1, light)
		if dy > 0:
			_px(ox, cy + dy, half + 1, 1, dark)
	# A few loose leaves, so no two crowns are the same.
	for k in 4:
		var lx := cx + int((_hash(id, k) - 0.5) * 2.2 * r) + sway
		var ly := cy + int((_hash(id, k + 9) - 0.6) * 1.8 * r)
		_px(lx, ly, 1, 1, light if k % 2 == 0 else dark)


func _draw_labels(font: Font, order: Array[int]) -> void:
	# Name the biggest living crowns, the trunk, and whatever is hovered.
	var named: Array[int] = []
	var living: Array[int] = []
	for i in order:
		if _alive(i):
			living.append(i)
	living.sort_custom(func(a: int, b: int) -> bool: return species[a]["population"] > species[b]["population"])
	for i in living.slice(0, 7):
		named.append(i)
	if hover >= 0 and not named.has(hover):
		named.append(hover)
	var taken: Array[Rect2] = []
	for i in named:
		var text: String = species[i]["name"]
		var w := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, 12).x
		var y := (_y(_end_tick(i)) - 14) * ART if _alive(i) else (_y(_end_tick(i)) - 3) * ART
		var box := Rect2(clampf(_x(i) * ART - w * 0.5, 4.0, size.x - w - 4.0), y - 11, w, 14)
		var clash := false
		for other in taken:
			clash = clash or other.intersects(box.grow(2))
		if clash and i != hover:
			continue
		taken.append(box)
		draw_string(font, box.position + Vector2(1, 12), text, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color(0, 0, 0, 0.7))
		draw_string(font, box.position + Vector2(0, 11), text, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, INK if i == hover else _leaf(i).lightened(0.45))


## The card in the corner: the hovered branch, or the trunk.
func _draw_card(font: Font) -> void:
	var i := hover if hover >= 0 else root
	var s: Dictionary = species[i]
	var parent: int = s["row_parent"]
	var lines: Array[String] = []
	if _alive(i):
		lines.append("%s alive   (most ever: %s)" % [_commas(s["population"]), _commas(s["peak"])])
	else:
		lines.append("Extinct at tick %s, after %s ticks" % [_commas(s["extinct"]), _commas(s["extinct"] - s["born"])])
	if parent >= 0:
		lines.append("Branched from %s at tick %s" % [species[parent]["name"], _commas(s["born"])])
	else:
		lines.append("The first of its line, from tick %s" % _commas(s["born"]))
	var branches: int = kids[i].size()
	lines.append("%d %s grew from it   %s creatures on this limb" % [branches, "branch" if branches == 1 else "branches", _commas(carried[i])])
	if hover < 0:
		lines.append("Click a branch to zoom in." if root_id < 0 else "Click a branch to zoom in. Right-click to go back.")
	elif i == root:
		lines.append("This is the trunk." if parent < 0 else "Click to step back out to %s." % species[parent]["name"])
	else:
		lines.append("Click to make this the trunk.")

	var w := font.get_string_size(s["name"], HORIZONTAL_ALIGNMENT_LEFT, -1, 16).x
	for line in lines:
		w = maxf(w, font.get_string_size(line, HORIZONTAL_ALIGNMENT_LEFT, -1, 12).x)
	w += 26.0
	var h := 30.0 + 16.0 * lines.size()
	# Low in the corner: the oldest part of a tree is the barest.
	var at := Vector2(10, (_ground_y() - 3) * ART - h)
	draw_rect(Rect2(at, Vector2(w, h)), Color(0.05, 0.06, 0.09, 0.88))
	draw_rect(Rect2(at, Vector2(4, h)), _leaf(i) if _alive(i) else DEAD)
	draw_string(font, at + Vector2(14, 20), s["name"], HORIZONTAL_ALIGNMENT_LEFT, -1, 16, INK)
	for k in lines.size():
		draw_string(font, at + Vector2(14, 40 + 16 * k), lines[k], HORIZONTAL_ALIGNMENT_LEFT, -1, 12, FAINT)

	# Where we are: the line of descent down to the trunk on show.
	var trail: Array[String] = []
	var up := root
	while up >= 0:
		trail.push_front(species[up]["name"])
		up = species[up]["row_parent"]
	if trail.size() > 1:
		var text := "  ›  ".join(trail)
		var tw := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, 12).x
		draw_rect(Rect2(size.x - tw - 22, (_ground_y() - 3) * ART - 24, tw + 14, 22), Color(0.05, 0.06, 0.09, 0.88))
		draw_string(font, Vector2(size.x - tw - 15, (_ground_y() - 3) * ART - 8), text, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, FAINT)


# --- Input -------------------------------------------------------------------------

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		var found := _pick(event.position)
		if found != hover:
			hover = found
			queue_redraw()
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_LEFT:
			var found := _pick(event.position)
			if found < 0:
				return
			if found == root:
				_zoom_out()
			else:
				_zoom_to(found)
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			_zoom_out()
		accept_event()


func _notification(what: int) -> void:
	if what == NOTIFICATION_MOUSE_EXIT and hover >= 0:
		hover = -1
		queue_redraw()


func _zoom_to(i: int) -> void:
	root_id = species[i]["id"]
	hover = -1
	grown = 0.0
	_layout()
	queue_redraw()


func _zoom_out() -> void:
	if root < 0 or species[root]["row_parent"] < 0:
		return
	_zoom_to(species[root]["row_parent"])


## The branch under the mouse (screen pixels), or -1.
func _pick(at: Vector2) -> int:
	if root < 0:
		return -1
	var p := at / float(ART)
	var best := -1
	var best_d := 4.0   # art pixels: how close counts as "on" a branch
	for i in species.size():
		if shown[i] == 0:
			continue
		var x := float(_x(i))
		var top := float(_y(_end_tick(i)))
		var d := 1e9
		if i == root:
			d = _to_segment(p, Vector2(x, top), Vector2(x, _ground_y()))
		else:
			var parent: int = species[i]["row_parent"]
			var elbow := Vector2(_elbow(i))
			d = minf(_to_segment(p, Vector2(_x(parent), _y(species[i]["born"])), elbow),
					_to_segment(p, elbow, Vector2(x, top)))
		d -= _thickness(i) * 0.5
		if _alive(i):
			var r := clampi(2 + int(sqrt(float(species[i]["population"])) / 3.0), 2, 10)
			d = minf(d, p.distance_to(Vector2(x, _y(now) - r / 2)) - r)
		if d < best_d:
			best_d = d
			best = i
	return best


func _to_segment(p: Vector2, a: Vector2, b: Vector2) -> float:
	var ab := b - a
	var t := 0.0 if ab.length_squared() < 1e-6 else clampf((p - a).dot(ab) / ab.length_squared(), 0.0, 1.0)
	return p.distance_to(a + ab * t)


func _short(tick: int) -> String:
	if tick >= 1000000:
		return "%.1fM" % (tick / 1000000.0)
	if tick >= 1000:
		return "%dk" % (tick / 1000)
	return str(tick)


func _commas(value: int) -> String:
	var s := str(value)
	var i := s.length() - 3
	while i > 0:
		s = s.insert(i, ",")
		i -= 3
	return s
