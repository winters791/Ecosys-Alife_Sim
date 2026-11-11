// Pixel creatures. Every creature is one flat quad, and its sprite is drawn
// here, pixel by pixel, from its body data. Nothing is stored per creature
// except those numbers, so ten thousand of them are still one draw call, and
// a body that evolves a new part simply starts being drawn with it.
//
// A sprite needs no rotation or scale, so the instance transform's basis
// carries data instead (see LucaWorld::get_instance_buffer):
//   MODEL_MATRIX[0].xyz = (core radius, absorber, flagellum)
//   MODEL_MATRIX[1].xyz = (legs, sensor, armour)
//   MODEL_MATRIX[2].xyz = (storage, heading x, heading z)
//   MODEL_MATRIX[3].xyz = position (x, z); y carries the species hue, 0..1
//   COLOR               = (pigment, photo cover, energy fullness, speed)
//   INSTANCE_CUSTOM     = (phase, shape gene 0, shape gene 1, poison)
//
// What each part looks like:
//   photo      green pixels spreading over the skin
//   absorber   a dark red mouth on the leading edge (teeth when it is big)
//   flagellum  a wiggling tail behind
//   legs       stubs along both sides that step as it moves
//   sensor     an eye (two when the sensor is large)
//   armour     thick grey plates around the rim
//   storage    a pale yellow fat spot in the middle
//   poison     bright violet warts scattered over the skin
shader_type spatial;
render_mode unshaded, skip_vertex_transform, cull_disabled;

uniform float px_size = 0.125;       // world units per sprite pixel (shared with the terrain)
uniform float px_per_radius = 4.5;   // sprite pixels per unit of core radius
uniform float extent = 128.0;
uniform vec3 tint = vec3(1.0);       // day / night
uniform float poison_test = 0.0;      // testing only: draw every body as if it carried this much poison
uniform bool by_species = false;     // paint skin in the species colour instead of the pigment

varying vec2 px;                     // this fragment, in sprite pixels from the centre
varying flat vec4 traits;            // pigment, photo, fullness, speed
varying flat vec4 body;              // radius in pixels, absorber, flagellum, legs
varying flat vec4 extra;             // sensor, armour, storage, phase
varying flat vec2 shape;
varying flat float poison;
varying flat vec2 heading;
varying flat float species_hue;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void vertex() {
	vec3 a = MODEL_MATRIX[0].xyz;
	vec3 b = MODEL_MATRIX[1].xyz;
	vec3 c = MODEL_MATRIX[2].xyz;
	vec3 origin = MODEL_MATRIX[3].xyz;

	traits = COLOR;
	species_hue = origin.y;
	shape = clamp(INSTANCE_CUSTOM.yz, -1.0, 1.0);
	poison = INSTANCE_CUSTOM.w + poison_test;
	// Fat is visible: a full blob is plump, a starving one has shrunk.
	float radius = max(a.x, 0.05) * px_per_radius * mix(0.85, 1.1, traits.b);
	body = vec4(radius, a.y, a.z, b.x);
	extra = vec4(b.y, b.z, c.x, INSTANCE_CUSTOM.x * 6.2831853);
	heading = length(c.yz) > 0.01 ? normalize(c.yz) : vec2(0.0, 1.0);

	// The quad is big enough for the body plus a tail or legs sticking out.
	float half_px = ceil(radius) + 7.0;
	px = VERTEX.xz * 2.0 * half_px;
	// Snap to the world's pixel grid so sprites never sit between pixels.
	vec2 centre = (floor(origin.xz / px_size) + 0.5) * px_size;
	// Things further down the screen are drawn on top.
	float layer = 0.5 + 0.4 * origin.z / extent;
	vec3 world = vec3(centre.x + px.x * px_size, layer, centre.y + px.y * px_size);
	VERTEX = (VIEW_MATRIX * vec4(world, 1.0)).xyz;
	NORMAL = vec3(0.0, 0.0, 1.0);
}

void fragment() {
	vec2 p = floor(px) + 0.5;            // centre of this sprite pixel
	vec2 h = heading;
	vec2 s = vec2(-h.y, h.x);
	float along = dot(p, h);
	float side = dot(p, s);
	float phase = extra.w;
	float speed = traits.a;

	// Body outline: a disc with lumps from the shape genes, breathing a little.
	float ang = atan(p.y, p.x);
	float lump = 1.0 + 0.16 * shape.x * cos(2.0 * ang + phase)
	                 + 0.12 * shape.y * cos(3.0 * ang + phase * 1.7);
	float rad = body.x * lump + 0.3 * sin(TIME * 1.6 + phase);
	float d = length(p);

	// Pigment: pale cream to dark brown.
	vec3 skin = mix(vec3(0.93, 0.84, 0.66), vec3(0.20, 0.13, 0.10), traits.r);
	if (by_species) {
		// One flat colour per species (the same hue the species list shows).
		vec3 k = clamp(abs(fract(species_hue + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
		skin = mix(vec3(1.0), k, 0.7) * 0.92;
	}
	vec3 rim = skin * 0.55;
	vec3 col = vec3(0.0);
	bool hit = false;

	// Flagellum: a tail behind, wiggling faster the faster it can go.
	if (body.z > 0.0) {
		float len = 2.0 + min(body.z, 1.5) * 4.0;
		float t = -along - rad * 0.7;
		if (t > 0.0 && t < len) {
			float wig = sin(t * 1.1 - TIME * (4.0 + 14.0 * speed) + phase) * min(1.5, t * 0.5);
			if (abs(side - wig) < 0.6) { col = rim; hit = true; }
		}
	}

	// Legs: stubs along both sides, alternate ones stepping.
	if (body.w > 0.0 && !hit) {
		float len = 1.0 + min(body.w, 1.5) * 2.0;
		float out_px = abs(side) - rad * 0.8;
		float slot = floor(along / 2.0);
		bool is_leg = mod(slot, 2.0) < 0.5 && abs(along) < rad * 0.8;
		float step_px = sin(TIME * (2.0 + 12.0 * speed) + phase + slot * 3.14159 + (side > 0.0 ? 0.0 : 3.14159)) > 0.0 ? 1.0 : 0.0;
		if (is_leg && out_px > 0.0 && out_px < len + step_px) { col = rim; hit = true; }
	}

	if (d < rad) {
		hit = true;
		col = skin;

		// Storage: a pale fat spot in the middle.
		if (extra.z > 0.0 && d < min(rad - 1.0, 0.8 + extra.z * 2.0)) col = vec3(0.98, 0.90, 0.55);

		// Photosynthetic patches: green pixels, more of them with more cover.
		// In the species view they are a darker shade of the species colour,
		// so the body still reads as one colour from a distance.
		if (hash(p + floor(phase * 10.0)) < traits.g * 0.95)
			col = by_species ? skin * 0.78 : mix(vec3(0.16, 0.45, 0.16), vec3(0.36, 0.72, 0.22), traits.b);

		// Poison: violet warts, a few even on a faintly poisonous body. They sit
		// on a coarse grid so they read as spots, not noise, and never flicker.
		if (poison > 0.0) {
			vec2 wart = floor(p * 0.5);
			if (hash(wart + floor(phase * 7.0) + 31.0) < 0.10 + min(poison, 2.0) * 0.16)
				col = mod(wart.x + wart.y, 2.0) < 1.0 ? vec3(0.78, 0.30, 0.95) : vec3(0.52, 0.16, 0.72);
		}

		// Rim, or armour plates if it has them.
		float plate = extra.y > 0.0 ? 1.0 + min(extra.y * 2.5, 2.0) : 0.0;
		if (d > rad - 1.0) col = rim;
		if (plate > 0.0 && d > rad - plate)
			col = mod(floor(ang * 2.2 + phase), 2.0) < 1.0 ? vec3(0.55, 0.57, 0.62) : vec3(0.38, 0.40, 0.46);

		// Absorber: a mouth on the leading edge.
		if (body.y > 0.0) {
			float depth = 1.0 + min(body.y, 1.5) * 1.6;
			float width = 0.6 + min(body.y, 1.5) * 1.3;
			if (along > rad - depth - 0.5 && abs(side) < width) {
				col = vec3(0.55, 0.08, 0.10);
				// Teeth once the mouth is big enough to have them.
				if (body.y > 0.6 && along > rad - 1.5 && mod(floor(side + 0.5), 2.0) < 1.0) col = vec3(0.96, 0.94, 0.88);
			}
		}

		// Sensor: one eye, or two when it is large.
		if (extra.x > 0.0) {
			float apart = extra.x > 0.35 ? max(1.0, rad * 0.45) : 0.0;
			vec2 e1 = floor(h * rad * 0.25 + s * apart) + 0.5;
			vec2 e2 = floor(h * rad * 0.25 - s * apart) + 0.5;
			if (distance(p, e1) < 0.5 || distance(p, e2) < 0.5) col = vec3(0.05, 0.05, 0.08);
			vec2 w1 = e1 - floor(h + 0.5), w2 = e2 - floor(h + 0.5);
			if (rad > 3.5 && (distance(p, w1) < 0.5 || distance(p, w2) < 0.5)) col = vec3(0.97);
		}
	}

	if (!hit) discard;
	ALBEDO = col * tint;
}
