// Shared by every creature shader: how a body moves, and what colour its skin is.
// Every piece of a creature (body, eyes, legs...) is its own instance, so they
// all run the same motion here and stay glued together.
//
// Per instance:
//   COLOR           = (species hue, pigment, a per-piece value, sleep)
//   INSTANCE_CUSTOM = (phase, how fast it is going 0..1, lift, body radius)
//                     lift >= 0: held this far off the ground by its legs
//                     lift <  0: no legs; it hops when it moves

uniform float motion = 1.0;          // 0 while the world is paused: everything holds still
uniform bool vivid = false;          // "colour by species": flat bright colours

// How far off the ground the whole creature is right now.
float creature_lift(vec4 c, float sleep) {
	float going = c.y * motion * (1.0 - sleep);
	float beat = c.x * 6.2831853;
	if (c.z < 0.0) {
		// A hopper: up and down in arcs, higher the faster it goes.
		return abs(sin(TIME * 7.0 + beat)) * c.w * 1.2 * going;
	}
	// A walker stands on its legs, bobs a little as it steps, and settles when asleep.
	return c.z * (1.0 - 0.75 * sleep) + abs(sin(TIME * 10.0 + beat)) * c.w * 0.12 * going;
}

// Squash and stretch that goes with the lift: tall at the top of a hop, flat on landing.
float creature_stretch(vec4 c, float sleep) {
	float going = c.y * motion * (1.0 - sleep);
	float beat = c.x * 6.2831853;
	float breath = 0.03 * sin(TIME * 2.2 + beat) * (1.0 - 0.5 * sleep);
	float hop = c.z < 0.0 ? 0.16 * (abs(sin(TIME * 7.0 + beat)) - 0.4) * going : 0.05 * sin(TIME * 20.0 + beat) * going;
	return 1.0 + breath + hop - 0.22 * sleep;
}

vec3 hue_to_rgb(float h) {
	return clamp(abs(fract(h + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
}

// Skin: the species gives the hue, pigment makes it darker, sleep greys it.
vec3 creature_skin(vec4 col) {
	vec3 pure = hue_to_rgb(col.r);
	vec3 skin = vivid ? mix(vec3(1.0), pure, 0.85) * 0.95
	                  : mix(vec3(1.0), pure, 0.68) * mix(1.0, 0.5, col.g);
	float grey = dot(skin, vec3(0.3, 0.5, 0.2));
	return mix(skin, vec3(grey) * 0.85, 0.45 * col.a);
}
