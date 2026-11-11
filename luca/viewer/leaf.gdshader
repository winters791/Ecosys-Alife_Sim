// A leaf (a photosynthetic patch): a flat green blade sprouting from the top
// of the body. It sways, and folds down when the creature sleeps.
shader_type spatial;
render_mode skip_vertex_transform, diffuse_lambert_wrap, cull_disabled;
#include "creature.gdshaderinc"

varying flat vec4 info;
varying float along;
varying float across;

void vertex() {
	info = COLOR;
	vec4 c = INSTANCE_CUSTOM;
	along = VERTEX.y + 0.5;                      // 0 at the stem .. 1 at the tip
	across = VERTEX.x * 2.0;
	// A ball squeezed into a blade: wide in the middle, pointed at the tip, thin.
	vec3 v = vec3(VERTEX.x * 2.0 * sin(3.14159265 * pow(along, 0.8)), along, VERTEX.z * 0.25);
	v.z += 0.35 * along * along;                 // curls outward
	vec3 world = (MODEL_MATRIX * vec4(v, 1.0)).xyz;
	vec3 root = MODEL_MATRIX[3].xyz;
	// Sway in the air; droop when asleep.
	float sway = sin(TIME * 1.6 + c.x * 6.2831853 + root.x * 0.7 + root.z * 0.5);
	world.xz += sway * along * along * 0.12 * c.w * motion;
	world.y -= info.a * along * along * 0.7 * length(MODEL_MATRIX[1].xyz);
	world.y += creature_lift(c, info.a) - 0.44 * info.a * c.w;
	VERTEX = (VIEW_MATRIX * vec4(world, 1.0)).xyz;
	NORMAL = normalize((VIEW_MATRIX * vec4(MODEL_NORMAL_MATRIX * NORMAL, 0.0)).xyz);
}

void fragment() {
	// More cover, deeper green; the rib down the middle is lighter.
	vec3 green = mix(vec3(0.62, 0.86, 0.42), vec3(0.30, 0.70, 0.30), info.b);
	green = mix(green, vec3(0.80, 0.95, 0.55), 0.6 * smoothstep(0.18, 0.0, abs(across)));
	green *= mix(0.8, 1.05, along);
	ALBEDO = mix(green, vec3(dot(green, vec3(0.33))) * 0.8, 0.4 * info.a);
	ROUGHNESS = 0.5;
	RIM = 0.3;
	if (!FRONT_FACING) NORMAL = -NORMAL;
}
