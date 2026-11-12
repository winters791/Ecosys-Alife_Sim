extends Control
## The tree of life, as a scrollable cladogram.
##
## One row per species. Time runs left to right: a species' line starts at the
## tick it branched off and runs to now, or to the tick it died out. A short
## stem drops from the parent's line to each branch. Children are listed
## newest first under their parent, so no stem ever crosses another line.
## Names, and how many are alive, sit in a column on the right.
##
##   Mouse    hover a row to read about it; click to select it (its line of
##            descent lights up, the rest dims); double-click to open its
##            branch on its own; click the triangle to fold its branches;
##            wheel, trackpad or drag to scroll; right-click to step back out.
##   Keys     Up/Down move, Left/Right fold or go up/down a generation,
##            Enter opens a branch, Backspace steps out, E hides dead ends,
##            Esc closes.
##
## Drawn in plain pixel rectangles from LucaWorld.get_species_tree(); nothing
## here feeds back into the world.

signal close_requested
signal selection_changed(species_id: int)   # -1 when nothing is selected

const ROW := 18          # pixels per species row
const HEAD := 30         # breadcrumbs and buttons
const AXIS := 20         # tick labels
const FOOT := 62         # the info bar
const PAD := 12
const NAMES := 236       # width of the name column
const GAP := 18          # between the plot and the names

const BG := Color(0.065, 0.072, 0.095)
const STRIPE := Color(0.085, 0.094, 0.122)
const BAR := Color(0.045, 0.05, 0.068)
const GRID := Color(1, 1, 1, 0.05)
const DEAD := Color(0.42, 0.43, 0.48)
const INK := Color(0.93, 0.94, 0.96)
const FAINT := Color(0.58, 0.63, 0.71)
const DIM := 0.72        # how far an unlit row fades into the background

var species: Array = []  # as given by get_species_tree(): depth-first, parents first
var now := 1

# Per species, indexed like `species`; rebuilt when new data arrives.
var index_of := {}                         # species id -> index
var kids: Array[PackedInt32Array] = []     # children, oldest first
var clade_count := PackedInt32Array()      # species in its clade, itself included
var clade_alive := PackedInt32Array()      # creatures alive in its clade
var lives_on := PackedByteArray()          # 1 if it or anything after it is alive

# View state, kept across updates by species id.
var focus_id := -1                         # branch shown on its own; -1 = all life
var selected_id := -1
var collapsed := {}                        # species id -> true
var hide_extinct := false
var scroll := 0.0

# Layout.
var rows := PackedInt32Array()             # species index on each row, top down
var row_of := PackedInt32Array()           # species index -> row, or -1
var lit := PackedByteArray()               # 1 if on the selected line of descent
var t0 := 0
var hover := -1
var mouse := Vector2(-1, -1)
var buttons: Array = []                    # [Rect2, Callable], rebuilt each draw
var press_at := Vector2.ZERO
var press_scroll := 0.0
var pressing := false
var dragged := false


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	focus_mode = Control.FOCUS_ALL
	clip_contents = true


func show_tree(tree: Array, tick: int) -> void:
	species = tree
	now = maxi(tick, 1)
	_index()
	_layout()
	hover = _row_at(mouse)
	queue_redraw()


# --- Data --------------------------------------------------------------------------

func _index() -> void:
	var n := species.size()
	index_of.clear()
	kids.clear()
	for i in n:
		kids.append(PackedInt32Array())
	for i in n:
		index_of[int(species[i]["id"])] = i
		var p: int = species[i]["row_parent"]
		if p >= 0:
			kids[p].append(i)
	clade_count.resize(n)
	clade_alive.resize(n)
	lives_on.resize(n)
	# Parents come before children, so one backwards pass sums each clade.
	for i in range(n - 1, -1, -1):
		var count := 1
		var alive: int = species[i]["population"] if _alive(i) else 0
		var on := 1 if _alive(i) else 0
		for k: int in kids[i]:
			count += clade_count[k]
			alive += clade_alive[k]
			on = on | lives_on[k]
		clade_count[i] = count
		clade_alive[i] = alive
		lives_on[i] = on
	if not index_of.has(focus_id):
		focus_id = -1
	if not index_of.has(selected_id):
		selected_id = -1


func _layout() -> void:
	var n := species.size()
	rows.clear()
	row_of.resize(n)
	row_of.fill(-1)
	var roots: Array[int] = []
	if focus_id >= 0:
		roots.append(index_of[focus_id])
	else:
		for i in n:
			if species[i]["row_parent"] < 0:
				roots.append(i)
	t0 = now
	for r in roots:
		t0 = mini(t0, species[r]["born"])
	# Depth first. Children go on the stack oldest first, so the newest comes
	# off first and sits right under its parent.
	var stack: Array[int] = []
	for k in range(roots.size() - 1, -1, -1):
		stack.append(roots[k])
	while not stack.is_empty():
		var i: int = stack.pop_back()
		row_of[i] = rows.size()
		rows.append(i)
		if collapsed.has(int(species[i]["id"])):
			continue
		for k: int in kids[i]:
			if _listed(k):
				stack.append(k)
	if selected_id >= 0 and row_of[index_of[selected_id]] < 0:
		selected_id = -1
	_light()
	_clamp_scroll()


func _light() -> void:
	lit.resize(species.size())
	if selected_id < 0:
		lit.fill(1)
		return
	lit.fill(0)
	var s: int = index_of[selected_id]
	# Depth-first order keeps a clade in one run of indices.
	for i in range(s, s + clade_count[s]):
		lit[i] = 1
	var up: int = species[s]["row_parent"]
	while up >= 0:
		lit[up] = 1
		up = species[up]["row_parent"]


func _listed(i: int) -> bool:
	return not hide_extinct or lives_on[i] == 1


func _shown_kids(i: int) -> int:
	var c := 0
	for k: int in kids[i]:
		if _listed(k):
			c += 1
	return c


func _alive(i: int) -> bool:
	return species[i]["extinct"] < 0


func _end_tick(i: int) -> int:
	return now if _alive(i) else int(species[i]["extinct"])


func _color(i: int) -> Color:
	var c := Color.from_hsv(species[i]["hue"], 0.62, 0.92) if _alive(i) else DEAD
	return c if lit[i] == 1 else c.lerp(BG, DIM)


# --- Geometry ----------------------------------------------------------------------

func _plot_left() -> float:
	return PAD + 2.0

func _plot_right() -> float:
	return size.x - NAMES - GAP

func _names_left() -> float:
	return size.x - NAMES

func _rows_top() -> float:
	return HEAD + AXIS

func _rows_bottom() -> float:
	return size.y - FOOT

func _tx(tick: int) -> float:
	var f := clampf(float(tick - t0) / float(maxi(now - t0, 1)), 0.0, 1.0)
	return roundf(_plot_left() + (_plot_right() - _plot_left()) * f)

func _ry(row: int) -> float:
	return _rows_top() + row * ROW - scroll

func _max_scroll() -> float:
	return maxf(0.0, rows.size() * ROW - (_rows_bottom() - _rows_top()) + 6.0)

func _clamp_scroll() -> void:
	scroll = clampf(scroll, 0.0, _max_scroll())

func _row_at(p: Vector2) -> int:
	if p.y < _rows_top() or p.y >= _rows_bottom() or p.x < 0 or p.x >= size.x:
		return -1
	var r := int(floor((p.y - _rows_top() + scroll) / ROW))
	return rows[r] if r >= 0 and r < rows.size() else -1

func _fold_rect(row: int) -> Rect2:
	return Rect2(_names_left() - 4, _ry(row), 18, ROW)


# --- Drawing -----------------------------------------------------------------------

func _draw() -> void:
	var font := get_theme_default_font()
	buttons.clear()
	draw_rect(Rect2(Vector2.ZERO, size), BG)
	if species.is_empty():
		draw_string(font, Vector2(PAD, HEAD + 24), "Nothing has taken hold yet: a species shows here once 10 are alive at once.", HORIZONTAL_ALIGNMENT_LEFT, -1, 13, FAINT)
		return

	var top := _rows_top()
	var bottom := _rows_bottom()
	var first := maxi(0, int(floor(scroll / ROW)))
	var last := mini(rows.size() - 1, int(ceil((scroll + bottom - top) / ROW)))

	# Row backgrounds: stripes so a line can be followed to its name.
	for r in range(first, last + 1):
		var i: int = rows[r]
		var y := _ry(r)
		var band := STRIPE if r % 2 == 1 else BG
		if species[i]["id"] == selected_id:
			band = Color(0.17, 0.2, 0.27)
		elif i == hover:
			band = Color(0.12, 0.14, 0.19)
		if band != BG:
			draw_rect(Rect2(0, y, size.x, ROW), band)

	# Time grid.
	var step := _nice_step(now - t0)
	var tick := int(ceil(float(t0) / step)) * step
	while tick <= now:
		draw_rect(Rect2(_tx(tick), top, 1, bottom - top), GRID)
		tick += step

	# Stems: from the parent's line down to each child. A stem may span the
	# view with neither end on it, so look at every row, not just visible ones.
	for r in rows.size():
		var i: int = rows[r]
		var p: int = species[i]["row_parent"]
		if p < 0 or row_of[p] < 0:
			continue
		var y0 := _ry(row_of[p]) + ROW * 0.5
		var y1 := _ry(r) + ROW * 0.5
		if y1 < top or y0 > bottom:
			continue
		var a := maxf(y0, top)
		var b := minf(y1, bottom)
		draw_rect(Rect2(_tx(species[i]["born"]), a, 2, b - a + 1), _color(i).darkened(0.15))

	# Lines, caps, leaders and names.
	for r in range(first, last + 1):
		_draw_row(font, r)

	# Scrollbar.
	if _max_scroll() > 0.0:
		var track := bottom - top
		var h := maxf(24.0, track * track / (rows.size() * ROW))
		var y := top + (track - h) * scroll / _max_scroll()
		draw_rect(Rect2(size.x - 4, y, 3, h), Color(1, 1, 1, 0.22))

	# Header, axis and footer go over anything that scrolled under them.
	draw_rect(Rect2(0, 0, size.x, top), BG)
	draw_rect(Rect2(0, top - 1, size.x, 1), Color(1, 1, 1, 0.08))
	_draw_header(font)
	tick = int(ceil(float(t0) / step)) * step
	while tick <= now:
		var x := _tx(tick)
		var text := "tick " + _short(tick) if tick == 0 else _short(tick)
		var w := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, 11).x
		if x + w * 0.5 < _plot_right() - 30:
			draw_string(font, Vector2(clampf(x - w * 0.5, 2, size.x), top - 6), text, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, FAINT)
			draw_rect(Rect2(x, top - 3, 1, 3), FAINT)
		tick += step
	var now_x := _tx(now)
	draw_string(font, Vector2(now_x - 22, top - 6), "now", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, INK)
	draw_rect(Rect2(now_x, top - 3, 1, 3), INK)
	draw_string(font, Vector2(_names_left() + 14, top - 6), "SPECIES", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, FAINT)
	draw_string(font, Vector2(size.x - 52, top - 6), "ALIVE", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, FAINT)
	_draw_footer(font)


func _draw_row(font: Font, r: int) -> void:
	var i: int = rows[r]
	var y := _ry(r)
	var cy := y + ROW * 0.5
	var col := _color(i)
	var x0 := _tx(species[i]["born"])
	var x1 := maxf(_tx(_end_tick(i)), x0 + 2)
	var alive := _alive(i)
	var th := 4.0 if alive else 2.0

	draw_rect(Rect2(x0, cy - th * 0.5, x1 - x0, th), col)
	draw_rect(Rect2(x0 - 1, cy - 2, 4, 4), col)
	if alive:
		draw_rect(Rect2(x1 - 2, cy - 4, 6, 8), col)
		draw_rect(Rect2(x1 - 2, cy - 4, 6, 2), col.lightened(0.3))
	else:
		draw_rect(Rect2(x1, cy - 3, 2, 6), col)
	# A dotted leader from the end of the line to its name.
	var lead := Color(1, 1, 1, 0.16 if lit[i] == 1 else 0.05)
	var x := x1 + 8
	while x < _names_left() - 8:
		draw_rect(Rect2(x, cy, 1, 1), lead)
		x += 4

	# The name column.
	var nl := _names_left()
	if _shown_kids(i) > 0:
		_triangle(Vector2(nl, cy), collapsed.has(int(species[i]["id"])), FAINT if lit[i] == 1 else FAINT.lerp(BG, DIM))
	draw_rect(Rect2(nl + 14, cy - 4, 8, 8), col)
	var ink := (INK if alive else FAINT)
	if lit[i] == 0:
		ink = ink.lerp(BG, DIM)
	var name: String = species[i]["name"]
	draw_string(font, Vector2(nl + 28, cy + 5), name, HORIZONTAL_ALIGNMENT_LEFT, -1, 13, ink)
	if collapsed.has(int(species[i]["id"])) and clade_count[i] > 1:
		var nw := font.get_string_size(name, HORIZONTAL_ALIGNMENT_LEFT, -1, 13).x
		draw_string(font, Vector2(nl + 34 + nw, cy + 5), "+%d" % (clade_count[i] - 1), HORIZONTAL_ALIGNMENT_LEFT, -1, 11, ink.lerp(BG, 0.3))
	var count := _commas(species[i]["population"]) if alive else "died " + _short(species[i]["extinct"])
	var cw := font.get_string_size(count, HORIZONTAL_ALIGNMENT_LEFT, -1, 12).x
	draw_string(font, Vector2(size.x - 10 - cw, cy + 5), count, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, ink if alive else ink.lerp(BG, 0.25))


## A small pixel triangle: pointing right when folded, down when open.
func _triangle(at: Vector2, folded: bool, color: Color) -> void:
	for k in 4:
		if folded:
			draw_rect(Rect2(at.x + k, at.y - 3 + k, 1, 7 - 2 * k), color)
		else:
			draw_rect(Rect2(at.x - 3 + k + 3, at.y - 2 + k, 7 - 2 * k, 1), color)


func _draw_header(font: Font) -> void:
	# Breadcrumbs: where the view is in the whole tree. Each one is a way back.
	var trail: Array[int] = []
	if focus_id >= 0:
		var up: int = index_of[focus_id]
		while up >= 0:
			trail.push_front(up)
			up = species[up]["row_parent"]
	var x := float(PAD)
	var y := 20.0
	x = _button(font, x, y, "All life", focus_id < 0, func() -> void: _open(-1))
	for k in trail.size():
		var i := trail[k]
		draw_string(font, Vector2(x + 2, y), "›", HORIZONTAL_ALIGNMENT_LEFT, -1, 13, FAINT)
		x += 14
		var id: int = species[i]["id"]
		x = _button(font, x, y, species[i]["name"], k == trail.size() - 1, func() -> void: _open(id))

	# Buttons on the right.
	var labels := ["Unfold all", "Fold all", "Dead ends: %s  (E)" % ("hidden" if hide_extinct else "shown")]
	var calls := [_unfold_all, _fold_all, _toggle_extinct]
	var right := size.x - PAD
	for k in labels.size():
		var w := font.get_string_size(labels[k], HORIZONTAL_ALIGNMENT_LEFT, -1, 12).x + 16
		right -= w
		_button(font, right, y, labels[k], false, calls[k])
		right -= 6


## A flat pill with a label; returns the x just past it.
func _button(font: Font, x: float, baseline: float, text: String, on: bool, call: Callable) -> float:
	var w := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, 12).x + 16
	var rect := Rect2(x, baseline - 15, w, 21)
	var hot := rect.has_point(mouse)
	var fill := Color(0.2, 0.24, 0.32) if on else (Color(0.16, 0.18, 0.24) if hot else Color(0.11, 0.125, 0.165))
	draw_rect(rect, fill)
	draw_string(font, Vector2(x + 8, baseline), text, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, INK if on or hot else FAINT)
	buttons.append([rect, call])
	return x + w + 4


func _draw_footer(font: Font) -> void:
	var top := _rows_bottom()
	draw_rect(Rect2(0, top, size.x, FOOT), BAR)
	draw_rect(Rect2(0, top, size.x, 1), Color(1, 1, 1, 0.08))
	var hint := "Click: select    Double-click or Enter: open branch    Right-click or Backspace: back    Arrows: move and fold    Esc: close"
	var i: int = hover if hover >= 0 else (index_of[selected_id] if selected_id >= 0 else -1)
	if i < 0:
		var alive := 0
		for k in species.size():
			if _alive(k):
				alive += 1
		draw_string(font, Vector2(PAD, top + 24), "%d species have taken hold, %d still alive. Hover a row to read about it." % [species.size(), alive], HORIZONTAL_ALIGNMENT_LEFT, -1, 14, INK)
		draw_string(font, Vector2(PAD, top + 46), hint, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, FAINT)
		return
	var s: Dictionary = species[i]
	var col := Color.from_hsv(s["hue"], 0.62, 0.92) if _alive(i) else DEAD
	draw_rect(Rect2(PAD, top + 12, 10, 10), col)
	draw_string(font, Vector2(PAD + 18, top + 22), s["name"], HORIZONTAL_ALIGNMENT_LEFT, -1, 15, INK)
	var nw := font.get_string_size(s["name"], HORIZONTAL_ALIGNMENT_LEFT, -1, 15).x
	var status := ""
	if _alive(i):
		status = "%s alive now, most ever %s" % [_commas(s["population"]), _commas(s["peak"])]
	else:
		status = "extinct at tick %s after %s ticks, most ever %s" % [_commas(s["extinct"]), _commas(s["extinct"] - s["born"]), _commas(s["peak"])]
	draw_string(font, Vector2(PAD + 30 + nw, top + 22), status, HORIZONTAL_ALIGNMENT_LEFT, -1, 13, FAINT)
	var p: int = s["row_parent"]
	var origin := "Branched from %s at tick %s" % [species[p]["name"], _commas(s["born"])] if p >= 0 else "The first of its line, from tick %s" % _commas(s["born"])
	var below := ""
	if clade_count[i] > 1:
		below = "    %d direct %s, %d species in its clade, %s creatures alive in it" % [kids[i].size(), "branch" if kids[i].size() == 1 else "branches", clade_count[i], _commas(clade_alive[i])]
	draw_string(font, Vector2(PAD, top + 46), origin + below, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, FAINT)


# --- Input -------------------------------------------------------------------------

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		mouse = event.position
		if pressing and (dragged or absf(mouse.y - press_at.y) > 4.0):
			dragged = true
			scroll = press_scroll - (mouse.y - press_at.y)
			_clamp_scroll()
		hover = _row_at(mouse)
		queue_redraw()
	elif event is InputEventMouseButton:
		_mouse_button(event)
	elif event is InputEventPanGesture:
		_scroll_by(event.delta.y * 14.0)
		accept_event()
	elif event is InputEventKey and event.pressed:
		if _key(event.keycode):
			accept_event()


func _mouse_button(event: InputEventMouseButton) -> void:
	match event.button_index:
		MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN:
			if event.pressed:
				var f := event.factor if event.factor > 0.0 else 1.0
				_scroll_by((-1.0 if event.button_index == MOUSE_BUTTON_WHEEL_UP else 1.0) * ROW * 3.0 * f)
		MOUSE_BUTTON_LEFT:
			if event.pressed:
				grab_focus()
				pressing = true
				dragged = false
				press_at = event.position
				press_scroll = scroll
				if event.double_click:
					var i := _row_at(event.position)
					if i >= 0:
						_open(species[i]["id"])
			else:
				pressing = false
				if not dragged:
					_click(event.position)
		MOUSE_BUTTON_RIGHT:
			if event.pressed:
				_back()
	accept_event()


func _click(at: Vector2) -> void:
	for b: Array in buttons:
		if (b[0] as Rect2).has_point(at):
			(b[1] as Callable).call()
			return
	var i := _row_at(at)
	if i < 0:
		if at.y >= _rows_top() and at.y < _rows_bottom():
			_select(-1)
		return
	if _shown_kids(i) > 0 and _fold_rect(row_of[i]).has_point(at):
		_fold(i, not collapsed.has(int(species[i]["id"])))
		return
	_select(species[i]["id"])


## Returns true if the key was used here.
func _key(code: Key) -> bool:
	var row := row_of[index_of[selected_id]] if selected_id >= 0 else -1
	var page := maxi(1, int((_rows_bottom() - _rows_top()) / ROW) - 1)
	match code:
		KEY_DOWN: _select_row(row + 1 if row >= 0 else 0)
		KEY_UP: _select_row(row - 1 if row >= 0 else 0)
		KEY_PAGEDOWN: _select_row(row + page if row >= 0 else page)
		KEY_PAGEUP: _select_row(row - page)
		KEY_HOME: _select_row(0)
		KEY_END: _select_row(rows.size() - 1)
		KEY_LEFT:
			if row < 0:
				return true
			var i: int = rows[row]
			if _shown_kids(i) > 0 and not collapsed.has(int(species[i]["id"])):
				_fold(i, true)
			elif species[i]["row_parent"] >= 0 and row_of[species[i]["row_parent"]] >= 0:
				_select_row(row_of[species[i]["row_parent"]])
		KEY_RIGHT:
			if row < 0:
				return true
			var i: int = rows[row]
			if collapsed.has(int(species[i]["id"])):
				_fold(i, false)
			elif _shown_kids(i) > 0:
				_select_row(row + 1)
		KEY_ENTER, KEY_KP_ENTER:
			if selected_id >= 0:
				_open(selected_id)
		KEY_BACKSPACE: _back()
		KEY_E: _toggle_extinct()
		KEY_ESCAPE: close_requested.emit()
		_: return false
	return true


func _notification(what: int) -> void:
	if what == NOTIFICATION_MOUSE_EXIT:
		mouse = Vector2(-1, -1)
		hover = -1
		pressing = false
		queue_redraw()
	elif what == NOTIFICATION_RESIZED:
		_clamp_scroll()


# --- Actions -----------------------------------------------------------------------

func _scroll_by(amount: float) -> void:
	scroll += amount
	_clamp_scroll()
	hover = _row_at(mouse)
	queue_redraw()


func _select(id: int, center := false) -> void:
	var changed := id != selected_id
	selected_id = id
	_light()
	if changed:
		selection_changed.emit(id)
	if id >= 0:
		_reveal(row_of[index_of[id]], center)
	queue_redraw()


func _select_row(row: int) -> void:
	if rows.is_empty():
		return
	_select(species[rows[clampi(row, 0, rows.size() - 1)]]["id"])


## Scroll just enough to show a row, or put it in the middle of the view.
func _reveal(row: int, center := false) -> void:
	if row < 0:
		return
	var y := row * ROW
	var view := _rows_bottom() - _rows_top()
	if center:
		scroll = y - (view - ROW) * 0.5
	elif y < scroll:
		scroll = y
	elif y + ROW > scroll + view:
		scroll = y + ROW - view
	_clamp_scroll()


func _fold(i: int, fold: bool) -> void:
	if fold:
		collapsed[int(species[i]["id"])] = true
	else:
		collapsed.erase(int(species[i]["id"]))
	_layout()
	queue_redraw()


func _fold_all() -> void:
	# Fold everything below the top level, so the tree reads as its main limbs.
	collapsed.clear()
	var top: int = index_of[focus_id] if focus_id >= 0 else -1
	for i in species.size():
		if kids[i].size() > 0 and i != top and (top >= 0 or species[i]["row_parent"] >= 0):
			collapsed[int(species[i]["id"])] = true
	_layout()
	queue_redraw()


func _unfold_all() -> void:
	collapsed.clear()
	_layout()
	if selected_id >= 0:
		_reveal(row_of[index_of[selected_id]])
	queue_redraw()


func _toggle_extinct() -> void:
	hide_extinct = not hide_extinct
	_layout()
	if selected_id >= 0:
		_reveal(row_of[index_of[selected_id]])
	queue_redraw()


## Show one branch on its own (-1 = all life).
func _open(id: int) -> void:
	focus_id = id
	if id >= 0:
		collapsed.erase(id)
	scroll = 0.0
	_layout()
	if selected_id >= 0 and row_of[index_of[selected_id]] >= 0:
		_reveal(row_of[index_of[selected_id]])
	hover = _row_at(mouse)
	queue_redraw()


## Step out to the branch this one grew from, keeping it in view.
func _back() -> void:
	if focus_id < 0:
		return
	var was := focus_id
	var p: int = species[index_of[focus_id]]["row_parent"]
	_open(species[p]["id"] if p >= 0 else -1)
	_select(was, true)


## For main.gd: open the tree on one species (by index), as its own branch.
func _zoom_to(i: int) -> void:
	_open(species[i]["id"])
	_select(species[i]["id"])


## For main.gd: select a species by name, unfolding and widening the view as
## needed so it can be seen.
func select_name(name: String) -> void:
	for i in species.size():
		if species[i]["name"] == name:
			select_id(species[i]["id"])
			return


## For main.gd: select a species by id, the same way. Returns false if the
## tree does not list it (it never had 10 alive at once).
func select_id(id: int) -> bool:
	if not index_of.has(id):
		return false
	var i: int = index_of[id]
	if hide_extinct and lives_on[i] == 0:
		hide_extinct = false
	var up: int = species[i]["row_parent"]
	while up >= 0:
		collapsed.erase(int(species[up]["id"]))
		up = species[up]["row_parent"]
	if focus_id >= 0:
		var f: int = index_of[focus_id]
		if i < f or i >= f + clade_count[f]:
			focus_id = -1
	_layout()
	_select(species[i]["id"], true)
	return true


# --- Text --------------------------------------------------------------------------

## A round tick interval giving about eight marks across `span`.
func _nice_step(span: int) -> int:
	var raw := maxf(float(span) / 8.0, 1.0)
	var mag := pow(10.0, floor(log(raw) / log(10.0)))
	for m: float in [1.0, 2.0, 5.0, 10.0]:
		if m * mag >= raw:
			return maxi(int(m * mag), 1)
	return maxi(int(10.0 * mag), 1)


func _short(tick: int) -> String:
	if tick >= 1000000:
		return "%.1fM" % (tick / 1000000.0)
	if tick >= 1000:
		return "%dk" % (tick / 1000) if tick % 1000 == 0 else "%.1fk" % (tick / 1000.0)
	return str(tick)


func _commas(value: int) -> String:
	var s := str(value)
	var i := s.length() - 3
	while i > 0:
		s = s.insert(i, ",")
		i -= 3
	return s
