// A creature's body: one soft sphere per creature, stretched by its shape
// genes, breathing, bobbing as it walks and squashing as it hops.
shader_type spatial;
render_mode skip_vertex_transform, diffuse_lambert_wrap;
#include "creature.gdshaderinc"

uniform float poison_test = 0.0;     // testing only: draw every body as if it carried this much poison

varying vec3 local;
varying flat vec4 info;              // hue, pigment, fullness, sleep
varying flat float poison;

float hash3(vec3 p) {
	return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

void vertex() {
	info = COLOR;
	poison = INSTANCE_CUSTOM.w + poison_test;
	vec4 c = vec4(INSTANCE_CUSTOM.xyz, length(MODEL_MATRIX[0].xyz));
	local = VERTEX;

	// Stretch upward and thin sideways by the same breath, so it keeps its volume.
	float s = creature_stretch(c, info.a);
	vec3 v = VERTEX * vec3(1.0 / sqrt(s), s, 1.0 / sqrt(s));
	vec3 world = (MODEL_MATRIX * vec4(v, 1.0)).xyz;
	float half_height = length(MODEL_MATRIX[1].xyz);
	world.y += creature_lift(c, info.a) - (1.0 - s) * half_height;   // feet stay on the ground
	VERTEX = (VIEW_MATRIX * vec4(world, 1.0)).xyz;
	NORMAL = normalize((VIEW_MATRIX * vec4(MODEL_NORMAL_MATRIX * (NORMAL * vec3(sqrt(s), 1.0 / s, sqrt(s))), 0.0)).xyz);
}

void fragment() {
	vec3 skin = creature_skin(info);
	// A paler belly, and a hungry body is a little washed out.
	skin = mix(skin, vec3(1.0), 0.22 * smoothstep(0.1, -0.8, local.y));
	skin = mix(skin * 0.9 + 0.06, skin, 0.5 + 0.5 * info.b);

	// Poison: violet spots, more of them the more poison it carries.
	if (poison > 0.0) {
		vec3 cell = floor(local * 3.2 + 0.5);
		float d = length(local * 3.2 - cell);
		if (hash3(cell) < 0.18 + 0.2 * min(poison, 2.0) && d < 0.42)
			skin = mix(vec3(0.62, 0.22, 0.88), vec3(0.80, 0.45, 1.0), smoothstep(0.42, 0.0, d));
	}
	ALBEDO = skin;
	ROUGHNESS = 0.55;
	SPECULAR = 0.35;
	RIM = 0.18;
	RIM_TINT = 0.7;
}
