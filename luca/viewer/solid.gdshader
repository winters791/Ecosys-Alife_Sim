// Plain coloured pieces that just ride on the body: mouths and horns.
shader_type spatial;
render_mode skip_vertex_transform, diffuse_lambert_wrap;
#include "creature.gdshaderinc"

uniform vec3 base_color : source_color = vec3(0.5);
uniform vec3 tip_color : source_color = vec3(0.5);
uniform float pivot = 0.0;           // 0.5 for a cone that grows from its base
uniform float sink = 0.22;           // how far it settles when the body sleeps, in body radii
uniform float shine = 0.3;

varying float along;

void vertex() {
	along = VERTEX.y + 0.5;
	vec3 v = VERTEX + vec3(0.0, pivot, 0.0);
	vec3 world = (MODEL_MATRIX * vec4(v, 1.0)).xyz;
	world.y += creature_lift(INSTANCE_CUSTOM, COLOR.a) - sink * COLOR.a * INSTANCE_CUSTOM.w;
	VERTEX = (VIEW_MATRIX * vec4(world, 1.0)).xyz;
	NORMAL = normalize((VIEW_MATRIX * vec4(MODEL_NORMAL_MATRIX * NORMAL, 0.0)).xyz);
}

void fragment() {
	ALBEDO = mix(base_color, tip_color, along);
	ROUGHNESS = 1.0 - shine;
	SPECULAR = shine;
}
