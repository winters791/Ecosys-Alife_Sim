// Terrain: a flat grid pushed up by the sim's heightmap texture. Colour comes
// from height, slope and temperature, so climate powers are visible: cool the
// world and the snow line comes down.
shader_type spatial;

uniform sampler2D height_tex : filter_linear, repeat_disable;
uniform sampler2D temp_tex : filter_linear, repeat_disable;   // °C
uniform sampler2D light_tex : filter_linear, repeat_disable;  // energy per cell per tick
uniform float extent = 128.0;   // world units per side
uniform float cells = 128.0;    // cells per side
uniform float max_height = 40.0;
uniform int overlay = 0;        // 0 natural, 1 temperature, 2 light
uniform vec3 brush = vec3(0.0, 0.0, -1.0);  // x, z, radius; radius <= 0 hides it
uniform vec3 brush_color : source_color = vec3(1.0);

varying vec3 world_pos;

float height_at(vec2 uv) { return textureLod(height_tex, uv, 0.0).r; }

void vertex() {
	vec2 uv = VERTEX.xz / extent;
	VERTEX.y = height_at(uv);
	world_pos = VERTEX;
}

vec3 temperature_ramp(float t) {
	// -30 °C deep blue, 0 white-ish, +40 red.
	float k = clamp((t + 30.0) / 70.0, 0.0, 1.0);
	vec3 cold = vec3(0.10, 0.25, 0.75), mid = vec3(0.92, 0.94, 0.90), hot = vec3(0.85, 0.20, 0.10);
	return k < 0.43 ? mix(cold, mid, k / 0.43) : mix(mid, hot, (k - 0.43) / 0.57);
}

void fragment() {
	vec2 uv = world_pos.xz / extent;
	float e = 1.0 / cells;
	float cell = extent / cells;
	float hx = height_at(uv + vec2(e, 0.0)) - height_at(uv - vec2(e, 0.0));
	float hz = height_at(uv + vec2(0.0, e)) - height_at(uv - vec2(0.0, e));
	vec3 n = normalize(vec3(-hx, 2.0 * cell, -hz));
	NORMAL = normalize((VIEW_MATRIX * vec4(n, 0.0)).xyz);

	float h = world_pos.y;
	float temp = texture(temp_tex, uv).r;
	float light = texture(light_tex, uv).r;

	vec3 col;
	if (overlay == 1) {
		col = temperature_ramp(temp);
	} else if (overlay == 2) {
		col = mix(vec3(0.03, 0.03, 0.08), vec3(1.0, 0.92, 0.45), clamp(light, 0.0, 1.0));
	} else {
		float k = clamp(h / max_height, 0.0, 1.5);
		vec3 low = vec3(0.26, 0.22, 0.15), mid = vec3(0.33, 0.29, 0.21), high = vec3(0.30, 0.29, 0.28);
		col = k < 0.5 ? mix(low, mid, k * 2.0) : mix(mid, high, clamp(k * 2.0 - 1.0, 0.0, 1.0));
		float steep = smoothstep(0.80, 0.55, n.y);
		col = mix(col, vec3(0.20, 0.19, 0.19), steep);             // bare rock on cliffs
		float snow = smoothstep(1.0, -5.0, temp) * (1.0 - 0.6 * steep);
		col = mix(col, vec3(0.80, 0.84, 0.90), snow);
	}

	// Faint contour lines every 5 height units make the shape readable.
	float line = abs(fract(h / 5.0 - 0.5) - 0.5) / (fwidth(h / 5.0) + 1e-5);
	col *= mix(0.86, 1.0, clamp(line, 0.0, 1.0));

	// Brush ring.
	vec3 glow = vec3(0.0);
	if (brush.z > 0.0) {
		float d = distance(world_pos.xz, brush.xy);
		float w = fwidth(d) * 1.5 + 0.08;
		float ring = 1.0 - smoothstep(0.0, w, abs(d - brush.z));
		float inner = (1.0 - smoothstep(brush.z - w, brush.z, d)) * 0.10;
		col = mix(col, brush_color, clamp(ring + inner, 0.0, 1.0));
		glow = brush_color * ring * 0.6;
	}

	ALBEDO = col;
	ROUGHNESS = 0.95;
	EMISSION = glow + (overlay != 0 ? col * 0.35 : vec3(0.0));
}
