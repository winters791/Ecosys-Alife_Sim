// One shared blob mesh, deformed per instance (design 7.4). The sim sends raw
// traits; everything about how a creature LOOKS is decided here.
//
//   COLOR (per instance)  = (pigment, photo cover, energy fullness, speed)
//   INSTANCE_CUSTOM       = (random phase, shape gene 0, shape gene 1, shape gene 2)
shader_type spatial;

varying vec4 traits;
varying vec3 local_pos;
varying float phase;

void vertex() {
	traits = COLOR;
	phase = INSTANCE_CUSTOM.x * 6.2831853;
	vec3 shape = clamp(INSTANCE_CUSTOM.yzw, -1.0, 1.0);

	vec3 v = VERTEX;
	local_pos = v;

	// Shape genes: stretch, and two kinds of lumps.
	v.y *= 1.0 + 0.35 * shape.x;
	float around = atan(v.z, v.x);
	v += normalize(VERTEX) * 0.14 * shape.y * sin(3.0 * around + phase);
	v += normalize(VERTEX) * 0.10 * shape.z * sin(5.0 * VERTEX.y + phase);

	// Fat is visible: a full blob is plump, a starving one is shrunken.
	v *= mix(0.80, 1.12, traits.b);

	// Breathing, and a little sag so it sits on the ground like jelly.
	float breath = sin(TIME * 2.2 + phase);
	v.xz *= 1.0 + 0.045 * breath;
	v.y *= 0.86 - 0.045 * breath;

	VERTEX = v;
}

void fragment() {
	// Pigment: pale cream to dark brown.
	vec3 skin = mix(vec3(0.80, 0.70, 0.55), vec3(0.06, 0.04, 0.03), traits.r);

	// Photosynthetic patches: green spots that spread with photo cover.
	vec3 p = local_pos * 6.5;
	float spots = sin(p.x + phase) * sin(p.y + phase * 1.7) * sin(p.z - phase) * 0.5 + 0.5;
	float cover = clamp(traits.g, 0.0, 1.0);
	float green = smoothstep(1.0 - cover, 1.08 - cover, spots + 0.08) * step(0.001, cover);
	vec3 leaf = mix(vec3(0.05, 0.22, 0.07), vec3(0.13, 0.42, 0.10), traits.b);

	ALBEDO = mix(skin, leaf, green * 0.9);
	ROUGHNESS = 0.45;
	SPECULAR = 0.4;
	RIM = 0.12;
	RIM_TINT = 0.8;
}
