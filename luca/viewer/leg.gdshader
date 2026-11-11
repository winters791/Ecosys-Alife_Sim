// A leg: hangs from the hip (the instance origin) down to the ground and
// swings forward and back as the creature walks. Neighbouring legs step in turn.
shader_type spatial;
render_mode skip_vertex_transform, diffuse_lambert_wrap;
#include "creature.gdshaderinc"

varying flat vec4 info;
varying float down;

void vertex() {
	info = COLOR;
	float sleep = info.a;
	vec4 c = INSTANCE_CUSTOM;
	down = 0.5 - VERTEX.y;                       // 0 at the hip .. 1 at the foot
	vec3 v = vec3(VERTEX.x, VERTEX.y - 0.5, VERTEX.z);
	v.xz *= 1.0 + 0.5 * smoothstep(0.75, 1.0, down);   // a chunky foot

	vec3 hip = MODEL_MATRIX[3].xyz;
	vec3 r = (MODEL_MATRIX * vec4(v, 1.0)).xyz - hip;
	// Asleep, the body settles and the legs fold short under it.
	r.y *= 1.0 - 0.6 * sleep;

	// Swing about the hip, around the creature's sideways axis.
	vec3 side = normalize(MODEL_MATRIX[0].xyz);
	float going = c.y * motion * (1.0 - sleep);
	float a = sin(TIME * 10.0 + c.x * 6.2831853 + info.b * 3.14159265) * 0.75 * going;
	r = r * cos(a) + cross(side, r) * sin(a) + side * dot(side, r) * (1.0 - cos(a));

	vec3 world = hip + r;
	world.y += creature_lift(c, sleep) - c.z * (1.0 - 0.75 * sleep);   // the hip rides with the body's bob
	world.y += max(0.0, -sin(TIME * 10.0 + c.x * 6.2831853 + info.b * 3.14159265)) * 0.25 * c.w * going * down;  // lift the foot on the way forward
	VERTEX = (VIEW_MATRIX * vec4(world, 1.0)).xyz;
	NORMAL = normalize((VIEW_MATRIX * vec4(MODEL_NORMAL_MATRIX * NORMAL, 0.0)).xyz);
}

void fragment() {
	ALBEDO = creature_skin(info) * mix(0.82, 0.6, smoothstep(0.7, 1.0, down));
	ROUGHNESS = 0.6;
	RIM = 0.2;
}
