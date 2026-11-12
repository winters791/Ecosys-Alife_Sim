// Pixel creatures. Every creature is one flat quad, and its sprite is drawn
// here, pixel by pixel, from its body data. Nothing is stored per creature
// except those numbers, so ten thousand of them are still one draw call, and
// a body that evolves a new part simply starts being drawn with it.
//
// A sprite needs no rotation or scale, so the instance transform's basis
// carries data instead (see LucaWorld::get_instance_buffer):
//   MODEL_MATRIX[0].xyz = (core radius, elongation, squareness)
//   MODEL_MATRIX[1].xyz = (storage, sleep, number of parts)
//   MODEL_MATRIX[2].xyz = (skin hue, heading x, heading z)
//   MODEL_MATRIX[3].xyz = position (x, z); y carries the species hue, 0..1
//   COLOR               = (pigment, photo cover, energy fullness, speed)
//   INSTANCE_CUSTOM     = (phase, lump gene 0, lump gene 1, poison)
// and every part comes from parts_tex: 12 texels per creature, in instance
// order, each (type + 1, size, angle from the front, stalk length).
//
// The body: a superellipse, round at elongation 0 and squareness 0, up to
// three times as long as wide, and up to a box with rounded corners. Each part
// is drawn where it sits on the outline (its angle from the front, which is
// the way the creature faces), and out on a stalk if it has one:
//   photo      red leaf pixels over the arc of skin it covers; a frond on a stalk
//   absorber   a dark red mouth (teeth when big); a toothed ring on a tentacle
// Skin is coloured by the hue and pigment genes exactly as core/colour.hpp
// works it out (skin_colour), since hunters sense bodies by that colour
// against the ground. Mouths and leaves are always red, whatever the skin.
//   flagellum  a wiggling tail
//   legs       a pair of stubs, mirrored on both sides, that step
//   sensor     an eye (two when large); on an eyestalk if stalked
//   armour     grey plates over the arc of rim it covers
//   storage    a pale fat spot in the middle
//   poison     violet warts over the arc it covers
// The arc a part covers must match the rules in core/body.hpp.
shader_type spatial;
render_mode unshaded, skip_vertex_transform, cull_disabled;

uniform float px_size = 0.125;       // world units per sprite pixel (shared with the terrain)
uniform float px_per_radius = 4.5;   // sprite pixels per unit of core radius
uniform float extent = 128.0;
uniform vec3 tint = vec3(1.0);       // day / night
uniform float poison_test = 0.0;      // testing only: draw every body as if it carried this much poison
uniform bool by_species = false;     // paint skin in the species colour instead of the pigment
uniform float stone = 0.0;           // 1 = drawn as a fossil (the species card, for the extinct)
uniform sampler2D parts_tex : filter_nearest, repeat_disable;
uniform int parts_width = 1024;

const int PARTS = 12;
// Arcs, as in core/body.hpp: half-width in radians per unit of part size.
const float COVER_ARC = 1.5707963;   // photo, armour, poison: size 1 covers half the outline
const float MOUTH_ARC_BASE = 0.5;
const float MOUTH_ARC_PER_SIZE = 0.8;
const float MOUTH_ARC_MAX = PI;

varying vec2 px;                     // this fragment, in sprite pixels from the centre
varying flat vec4 traits;            // pigment, photo, fullness, speed
varying flat vec4 body;              // radius in pixels, elongation, squareness, phase
varying flat vec4 extra;             // storage, sleep, part count, poison
varying flat vec2 lumps;
varying flat vec2 heading;
varying flat float species_hue;
varying flat float skin_hue;
varying flat int first_texel;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Signed smallest difference between two angles.
float angdiff(float a, float b) {
	return mod(a - b + PI, 2.0 * PI) - PI;
}

void vertex() {
	vec3 a = MODEL_MATRIX[0].xyz;
	vec3 b = MODEL_MATRIX[1].xyz;
	vec3 c = MODEL_MATRIX[2].xyz;
	vec3 origin = MODEL_MATRIX[3].xyz;

	traits = COLOR;
	species_hue = origin.y;
	skin_hue = c.x;
	lumps = clamp(INSTANCE_CUSTOM.yz, -1.0, 1.0);
	// Fat is visible: a full blob is plump, a starving one has shrunk.
	float radius = max(a.x, 0.05) * px_per_radius * mix(0.85, 1.1, traits.b);
	body = vec4(radius, clamp(a.y, 0.0, 1.0), clamp(a.z, 0.0, 1.0), INSTANCE_CUSTOM.x * 6.2831853);
	extra = vec4(b.x, b.y, b.z, INSTANCE_CUSTOM.w + poison_test);
	heading = length(c.yz) > 0.01 ? normalize(c.yz) : vec2(0.0, 1.0);
	first_texel = INSTANCE_ID * PARTS;

	// The quad is big enough for a long body plus tails, legs and stalks.
	float k = 1.0 + 2.0 * body.y;
	float half_px = ceil(radius * pow(k, 0.6667)) + 16.0;
	px = VERTEX.xz * 2.0 * half_px;
	// Snap to the world's pixel grid so sprites never sit between pixels.
	vec2 centre = (floor(origin.xz / px_size) + 0.5) * px_size;
	// Things further down the screen are drawn on top.
	float layer = 0.5 + 0.4 * origin.z / extent;
	vec3 world = vec3(centre.x + px.x * px_size, layer, centre.y + px.y * px_size);
	VERTEX = (VIEW_MATRIX * vec4(world, 1.0)).xyz;
	NORMAL = vec3(0.0, 0.0, 1.0);
}

// How far out a point is, in units of the outline: 1 on the outline.
// q is in the body's frame: x along (front +), y across (left +).
float outline_rho(vec2 q, vec2 axes, float m) {
	vec2 u = abs(q) / axes;
	return pow(pow(u.x, m) + pow(u.y, m), 1.0 / m);
}

// Distance from p to the segment a-b.
float seg_dist(vec2 p, vec2 a, vec2 b) {
	vec2 ab = b - a;
	float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 1e-4), 0.0, 1.0);
	return length(p - a - ab * t);
}

void fragment() {
	vec2 p = floor(px) + 0.5;            // centre of this sprite pixel
	vec2 h = heading;
	vec2 s = vec2(-h.y, h.x);            // the creature's left
	vec2 q = vec2(dot(p, h), dot(p, s)); // this pixel in the body's frame
	float phase = body.w;
	float speed = traits.a;
	float sleep = extra.y;

	// Body outline: a superellipse with the same volume as the round body,
	// lumps from two of the shape genes, breathing a little.
	float k = 1.0 + 2.0 * body.y;
	float r = body.x + 0.3 * sin(TIME * 1.6 + phase) * (1.0 - sleep);
	vec2 axes = r * vec2(pow(k, 0.6667), pow(k, -0.3333));
	float m = 2.0 + 4.0 * body.z;
	float ang = atan(q.y, q.x);          // 0 at the front, + to the left
	float lump = 1.0 + 0.10 * lumps.x * cos(2.0 * ang + phase) + 0.08 * lumps.y * cos(3.0 * ang + phase * 1.7);
	float rho = outline_rho(q, axes, m) / lump;
	float d = length(q);
	float edge = rho > 1e-3 ? d * (1.0 / rho - 1.0) : r;   // pixels inside the outline (negative outside)

	// Skin: a hue from the hue gene, as bright as the pigment allows
	// (core/colour.hpp skin_colour: chroma 0.28, pulled 40% towards tan so it stays
	// earthy, brightness 0.92 pale to 0.20 dark).
	vec3 hk = clamp(abs(fract(skin_hue + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
	vec3 skin = mix(mix(vec3(1.0), hk, 0.28), vec3(1.0, 0.82, 0.60), 0.40) * mix(0.92, 0.20, traits.r);
	if (by_species) {
		vec3 sk = clamp(abs(fract(species_hue + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
		skin = mix(vec3(1.0), sk, 0.7) * 0.92;
	}
	vec3 rim = skin * 0.55;
	// Leaves are always red: scarlet when the body is full, deep crimson when hungry.
	vec3 leaf = mix(vec3(0.76, 0.14, 0.14), vec3(0.96, 0.30, 0.22), traits.b);
	vec3 mouth_col = vec3(0.55, 0.08, 0.10);
	vec3 tooth = vec3(0.96, 0.94, 0.88);

	bool inside = edge > 0.0;
	vec3 col = skin;
	bool hit = inside;
	if (inside && edge < 1.0) col = rim;

	// Storage: a pale fat spot in the middle.
	if (inside && extra.x > 0.0 && d < min(r - 1.0, 0.8 + extra.x * 2.0)) col = vec3(0.98, 0.90, 0.55);

	int count = int(extra.z + 0.5);
	vec3 over = vec3(0.0);   // things drawn outside the body (tails, legs, stalks)
	bool over_hit = false;
	vec3 top = vec3(0.0);    // things drawn on top of the skin (plates, mouths, eyes)
	bool top_hit = false;
	float legs_total = 0.0;

	for (int i = 0; i < PARTS; i++) {
		if (i >= count) break;
		int t = first_texel + i;
		vec4 part = texelFetch(parts_tex, ivec2(t % parts_width, t / parts_width), 0);
		int type = int(part.x + 0.5);
		if (type == 0) continue;
		float size = part.y;
		float theta = part.z;
		float stalk = part.w / px_size;          // stalk length in sprite pixels
		vec2 dir = vec2(cos(theta), sin(theta));
		float base_r = 1.0 / max(outline_rho(dir, axes, m), 1e-3);   // outline distance at this angle
		vec2 base = dir * base_r;
		float da = abs(angdiff(ang, theta));
		// A stalk sways a little, more the longer it is.
		vec2 tip = base + dir * stalk + vec2(-dir.y, dir.x) * sin(TIME * 1.3 + phase + float(i)) * stalk * 0.12 * (1.0 - sleep);
		bool stalked = stalk > 0.5 && (type == 1 || type == 2);   // only mouths and sun patches grow stalks (genome.hpp)
		if (stalked && seg_dist(q, base, tip) < 0.6) { over = rim; over_hit = true; }

		if (type == 1 || type == 8) {
			// Photo / poison: cover an arc of skin, or a blob at the end of a stalk.
			float arc = min(PI, COVER_ARC * size);
			bool on = stalked ? distance(q, tip) < 1.0 + size * 1.5 : (inside && da < arc);
			if (on) {
				if (type == 1) {
					if (stalked || hash(p + floor(phase * 10.0)) < traits.g * 0.95 + 0.25) { top = leaf; top_hit = true; }
				} else {
					vec2 wart = floor(p * 0.5);
					if (hash(wart + floor(phase * 7.0) + 31.0) < 0.10 + min(size, 2.0) * 0.16) {
						top = mod(wart.x + wart.y, 2.0) < 1.0 ? vec3(0.78, 0.30, 0.95) : vec3(0.52, 0.16, 0.72);
						top_hit = true;
					}
				}
			}
		} else if (type == 2) {
			// Absorber: a mouth at its place on the rim, or a toothed ring on a tentacle.
			float arc = min(MOUTH_ARC_MAX, MOUTH_ARC_BASE + MOUTH_ARC_PER_SIZE * size);
			if (stalked) {
				float rr = distance(q, tip);
				float ring = 1.0 + min(size, 1.5) * 1.2;
				if (rr < ring + 0.6) {
					top = rr < ring - 0.4 ? mouth_col : (mod(floor(atan(q.y - tip.y, q.x - tip.x) * 2.5 + phase), 2.0) < 1.0 && size > 0.3 ? tooth : rim);
					top_hit = true;
				}
			} else {
				float depth = 1.0 + min(size, 1.5) * 1.6;
				if (inside && da < arc * 0.6 && edge < depth) {
					top = mouth_col;
					if (size > 0.6 && edge < 1.2 && mod(floor(ang * base_r * 1.0 + 0.5), 2.0) < 1.0) top = tooth;
					top_hit = true;
				}
			}
		} else if (type == 3) {
			// Flagellum: a tail out from where it is attached, wiggling faster the faster it can go.
			float len = 2.0 + min(size, 1.5) * 4.0 + stalk;
			vec2 rel = q - base;
			float t_along = dot(rel, dir);
			float t_side = dot(rel, vec2(-dir.y, dir.x));
			if (t_along > -0.5 && t_along < len) {
				float wig = sin(t_along * 1.1 - TIME * (4.0 + 14.0 * speed) * (1.0 - sleep) + phase) * min(1.5, max(t_along, 0.0) * 0.5);
				if (abs(t_side - wig) < 0.6) { over = rim; over_hit = true; }
			}
		} else if (type == 4) {
			// Legs: a stub at this angle and its mirror on the other side, stepping in turn.
			legs_total += size;
			float len = 1.0 + min(size, 1.5) * 2.0;
			for (int side = 0; side < 2; side++) {
				float th = side == 0 ? theta : -theta;
				vec2 dd = vec2(cos(th), sin(th));
				float br = 1.0 / max(outline_rho(dd, axes, m), 1e-3);
				float step_px = sin(TIME * (2.0 + 12.0 * speed) * (1.0 - sleep) + phase + float(i) * 2.1 + float(side) * PI) > 0.0 ? 1.0 : 0.0;
				vec2 sweep = vec2(-dd.y, dd.x) * (step_px - 0.5);
				if (seg_dist(q, dd * (br - 0.5) + sweep * 0.3, dd * (br + len) + sweep) < 0.6) { over = rim; over_hit = true; }
			}
		} else if (type == 5) {
			// Sensor: an eye toward its side of the body (two when large), or on an eyestalk.
			vec2 e1, e2;
			if (stalked) {
				e1 = floor(tip) + 0.5;
				e2 = e1;
			} else {
				float spread = size > 0.35 ? 0.35 : 0.0;
				vec2 d1 = vec2(cos(theta + spread), sin(theta + spread));
				vec2 d2 = vec2(cos(theta - spread), sin(theta - spread));
				e1 = floor(d1 * base_r * 0.6) + 0.5;
				e2 = floor(d2 * base_r * 0.6) + 0.5;
			}
			bool shut = sleep > 0.3;
			if (distance(q, e1) < 0.5 || distance(q, e2) < 0.5) { top = shut ? rim : vec3(0.05, 0.05, 0.08); top_hit = true; }
			vec2 w1 = e1 - floor(dir + 0.5), w2 = e2 - floor(dir + 0.5);
			if (!shut && (r > 3.5 || stalked) && (distance(q, w1) < 0.5 || distance(q, w2) < 0.5)) { top = vec3(0.97); top_hit = true; }
		} else if (type == 6) {
			// Armour: plates over the arc of rim it covers.
			float arc = min(PI, COVER_ARC * size);
			float plate = 1.0 + min(size * 1.5, 2.0);
			if (inside && da < arc && edge < plate) {
				top = mod(floor(ang * 2.2 * r / 4.5 + phase), 2.0) < 1.0 ? vec3(0.55, 0.57, 0.62) : vec3(0.38, 0.40, 0.46);
				top_hit = true;
			}
		}
	}

	// Poison spread over the whole body (testing only).
	if (inside && poison_test > 0.0) {
		vec2 wart = floor(p * 0.5);
		if (hash(wart + floor(phase * 7.0) + 31.0) < 0.10 + min(poison_test, 2.0) * 0.16) { top = vec3(0.78, 0.30, 0.95); top_hit = true; }
	}

	if (inside && top_hit) col = top;
	if (!inside && over_hit) { col = over; hit = true; }
	if (!inside && top_hit) { col = top; hit = true; }   // mouths and eyes out on stalks
	if (!hit) discard;

	// Asleep: the colours drain a little.
	col = mix(col, vec3(dot(col, vec3(0.3, 0.5, 0.2))) * 0.85, 0.5 * sleep);
	// A fossil: the body in stone, in three flat tones.
	float lum = dot(col, vec3(0.3, 0.5, 0.2));
	vec3 rock = lum < 0.3 ? vec3(0.30, 0.28, 0.25) : (lum < 0.55 ? vec3(0.52, 0.49, 0.44) : vec3(0.70, 0.67, 0.60));
	col = mix(col, rock, stone);
	ALBEDO = col * tint;
}
