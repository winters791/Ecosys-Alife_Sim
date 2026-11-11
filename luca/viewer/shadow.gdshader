// A soft round shadow on the ground under each creature. It shrinks and
// fades as the creature leaves the ground, which is what makes a hop read.
shader_type spatial;
render_mode skip_vertex_transform, unshaded, blend_mix, depth_draw_never, cull_disabled, shadows_disabled;
#include "creature.gdshaderinc"

varying vec2 disc;
varying flat float strength;

void vertex() {
	disc = VERTEX.xz;
	float lift = creature_lift(INSTANCE_CUSTOM, COLOR.a);
	float high = lift / max(INSTANCE_CUSTOM.w, 0.01);
	strength = 0.5 / (1.0 + 0.9 * high);
	vec3 v = VERTEX * vec3(1.0 / (1.0 + 0.25 * high), 1.0, 1.0 / (1.0 + 0.25 * high));
	VERTEX = (VIEW_MATRIX * (MODEL_MATRIX * vec4(v, 1.0))).xyz;
}

void fragment() {
	float r = length(disc);
	ALBEDO = vec3(0.10, 0.16, 0.14);
	ALPHA = strength * smoothstep(1.0, 0.35, r);
}
