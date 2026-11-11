// The ground: soft rolling hills. One flat grid of vertices, lifted by the
// sim's height field (so the terrain brush reshapes it as you paint) and
// coloured from the same fields the creatures live by.
shader_type spatial;
render_mode diffuse_lambert_wrap;

uniform sampler2D height_tex : filter_linear, repeat_disable;
uniform sampler2D temp_tex : filter_linear, repeat_disable;     // °C
uniform sampler2D light_tex : filter_linear, repeat_disable;    // energy per cell per tick at noon
uniform sampler2D soil_tex : filter_linear, repeat_disable;     // r = free nutrient, g = corpse energy
uniform float extent = 128.0;        // world units per side
uniform float cells = 128.0;
uniform float height_scale = 0.35;   // the view's hills are gentler than the sim's
uniform int overlay = 0;             // 0 natural, 1 temperature, 2 light, 3 nutrients, 4 the dead
uniform vec3 brush = vec3(0.0, 0.0, -1.0);   // x, z, radius; radius <= 0 hides it
uniform vec3 brush_color : source_color = vec3(1.0);

varying vec2 ground;                 // world x, z

float height_at(vec2 p) {
	return texture(height_tex, clamp(p / extent, 0.0, 1.0)).r * height_scale;
}

void vertex() {
	ground = VERTEX.xz;
	// The outermost ring of vertices drops away to make a rim round the world.
	bool rim = ground.x < 0.01 || ground.y < 0.01 || ground.x > extent - 0.01 || ground.y > extent - 0.01;
	VERTEX.y = rim ? -3.0 : height_at(ground);
	float e = extent / cells;
	NORMAL = normalize(vec3(height_at(ground - vec2(e, 0.0)) - height_at(ground + vec2(e, 0.0)), 2.0 * e,
	                        height_at(ground - vec2(0.0, e)) - height_at(ground + vec2(0.0, e))));
}

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec3 ramp(float t, vec3 a, vec3 b, vec3 c) {
	return t < 0.5 ? mix(a, b, t * 2.0) : mix(b, c, t * 2.0 - 2.0 + 1.0);
}

void fragment() {
	vec2 uv = clamp(ground / extent, 0.0, 1.0);
	float temp = texture(temp_tex, uv).r;
	float light = texture(light_tex, uv).r;
	vec2 soil = texture(soil_tex, uv).rg;
	float h = texture(height_tex, uv).r;

	// Natural: pale meadow green. Rich soil is a deeper green, bare soil is
	// sandy; dim ground is cooler; cold ground frosts over; hot ground bakes.
	vec3 meadow = mix(vec3(0.84, 0.84, 0.58), vec3(0.56, 0.82, 0.48), smoothstep(0.2, 2.5, soil.r));
	meadow = mix(meadow, vec3(0.38, 0.70, 0.44), 0.6 * smoothstep(2.5, 8.0, soil.r));
	meadow = mix(meadow * vec3(0.80, 0.88, 0.98), meadow, smoothstep(0.35, 0.95, light));
	meadow = mix(meadow, vec3(0.93, 0.85, 0.62), 0.6 * smoothstep(28.0, 42.0, temp));
	meadow = mix(meadow, vec3(0.95, 0.97, 1.0), smoothstep(4.0, -8.0, temp));
	// Soft drifts of lighter and darker grass, so flat ground is not one flat colour.
	meadow *= 0.96 + 0.08 * (0.5 + 0.5 * sin(ground.x * 0.61 + sin(ground.y * 0.37) * 2.0) * sin(ground.y * 0.53 + sin(ground.x * 0.29) * 2.0));
	// The dead: pale flecks where bodies lie.
	float bones = smoothstep(20.0, 400.0, soil.g);
	if (hash(floor(ground * 5.0)) < bones * 0.5) meadow = mix(meadow, vec3(0.97, 0.94, 0.86), 0.8);

	vec3 col = meadow;
	if (overlay == 1) col = ramp(clamp((temp + 10.0) / 50.0, 0.0, 1.0), vec3(0.45, 0.62, 0.95), vec3(0.95, 0.95, 0.85), vec3(0.96, 0.50, 0.38));
	if (overlay == 2) col = mix(vec3(0.20, 0.22, 0.38), vec3(1.0, 0.95, 0.62), clamp(light, 0.0, 1.0));
	if (overlay == 3) col = mix(vec3(0.88, 0.84, 0.74), vec3(0.20, 0.55, 0.30), clamp(soil.r / 6.0, 0.0, 1.0));
	if (overlay == 4) col = mix(vec3(0.80, 0.84, 0.80), vec3(0.62, 0.25, 0.42), clamp(soil.g / 500.0, 0.0, 1.0));
	// Faint contour lines help read the hills in the flat-coloured overlays.
	if (overlay != 0) col *= 0.94 + 0.06 * step(0.12, fract(h * 0.25));

	// The brush, or the ring round the creature being followed.
	if (brush.z > 0.0) {
		float d = distance(ground, brush.xy);
		float line = smoothstep(0.35, 0.0, abs(d - brush.z));
		col = mix(col, brush_color, 0.85 * line + 0.10 * step(d, brush.z));
	}
	ALBEDO = col;
	ROUGHNESS = 0.95;
	SPECULAR = 0.05;
}
