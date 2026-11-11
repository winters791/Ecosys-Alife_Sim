// A tail (the flagellum): a tapering whip from the back of the body that
// wags, faster and wider the faster the creature is going.
shader_type spatial;
render_mode skip_vertex_transform, diffuse_lambert_wrap;
#include "creature.gdshaderinc"

varying flat vec4 info;
varying float along;

void vertex() {
	info = COLOR;
	vec4 c = INSTANCE_CUSTOM;
	along = VERTEX.y + 0.5;                      // 0 at the root .. 1 at the tip
	vec3 v = vec3(VERTEX.x, along, VERTEX.z);
	v.xz *= 1.0 - 0.75 * along;                  // taper
	vec3 world = (MODEL_MATRIX * vec4(v, 1.0)).xyz;
	vec3 side = normalize(MODEL_MATRIX[0].xyz);
	float going = c.y * motion * (1.0 - info.a);
	float wag = sin(TIME * (3.0 + 11.0 * going) + c.x * 6.2831853 - along * 3.5);
	world += side * wag * along * c.w * (0.18 + 0.4 * going) * (1.0 - info.a);
	world.y += creature_lift(c, info.a) - 0.22 * info.a * c.w - 0.3 * info.a * along * c.w;
	VERTEX = (VIEW_MATRIX * vec4(world, 1.0)).xyz;
	NORMAL = normalize((VIEW_MATRIX * vec4(MODEL_NORMAL_MATRIX * NORMAL, 0.0)).xyz);
}

void fragment() {
	ALBEDO = creature_skin(info) * mix(0.9, 0.65, along);
	ROUGHNESS = 0.6;
	RIM = 0.25;
}
