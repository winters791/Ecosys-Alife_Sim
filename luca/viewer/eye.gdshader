// An eye: a white ball with a pupil looking forward. It blinks, and shuts when asleep.
shader_type spatial;
render_mode skip_vertex_transform, diffuse_lambert_wrap;
#include "creature.gdshaderinc"

varying vec3 local;
varying flat vec4 info;
varying flat float beat;

void vertex() {
	info = COLOR;
	beat = INSTANCE_CUSTOM.x;
	local = VERTEX;
	vec3 world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world.y += creature_lift(INSTANCE_CUSTOM, info.a) - 0.22 * info.a * INSTANCE_CUSTOM.w;
	VERTEX = (VIEW_MATRIX * vec4(world, 1.0)).xyz;
	NORMAL = normalize((VIEW_MATRIX * vec4(MODEL_NORMAL_MATRIX * NORMAL, 0.0)).xyz);
}

void fragment() {
	vec3 n = normalize(local);
	// Shut: asleep, or a quick blink every few seconds.
	bool shut = info.a > 0.5 || fract(TIME * 0.23 + beat * 7.0) < 0.035;
	vec3 col = vec3(0.98);
	if (info.b < 0.0) {
		// A blind body still has a face: two small dark dots, not working eyes.
		col = vec3(0.10, 0.09, 0.13);
		if (shut) col = creature_skin(info) * 0.7;
		else if (distance(n, normalize(vec3(-0.3, 0.4, 0.85))) < 0.35) col = vec3(0.9);
	} else if (shut) {
		col = creature_skin(info) * 0.8;
		if (abs(n.y) < 0.1 && n.z > 0.0) col = vec3(0.12, 0.10, 0.14);   // the lash line
	} else {
		float look = distance(n, normalize(vec3(0.0, -0.05, 1.0)));
		if (look < 0.62) col = vec3(0.08, 0.08, 0.12);
		if (distance(n, normalize(vec3(-0.25, 0.3, 0.9))) < 0.2) col = vec3(1.0);   // a glint
	}
	ALBEDO = col;
	ROUGHNESS = shut ? 0.6 : 0.15;
	SPECULAR = 0.5;
}
