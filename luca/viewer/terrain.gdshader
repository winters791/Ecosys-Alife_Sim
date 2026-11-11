// The ground, as pixel art seen from straight above. One flat quad; every
// cell of the sim is a tile, and each tile is drawn on the same pixel grid as
// the creatures. Height shows as colour bands, hill shading and dark cliff
// edges; cold ground whitens; rich soil darkens; the dead leave pale specks.
shader_type spatial;
render_mode unshaded;

uniform sampler2D height_tex : filter_nearest, repeat_disable;
uniform sampler2D temp_tex : filter_nearest, repeat_disable;    // °C
uniform sampler2D light_tex : filter_nearest, repeat_disable;   // energy per cell per tick at noon
uniform sampler2D soil_tex : filter_nearest, repeat_disable;    // r = free nutrient, g = corpse energy, b = waste
uniform float extent = 128.0;    // world units per side
uniform float cells = 128.0;     // cells per side
uniform float px_size = 0.125;   // world units per pixel (shared with the creatures)
uniform float max_height = 40.0;
uniform int overlay = 0;         // 0 natural, 1 temperature, 2 light, 3 nutrients, 4 the dead, 5 waste
uniform vec3 brush = vec3(0.0, 0.0, -1.0);  // x, z, radius; radius <= 0 hides it
uniform vec3 brush_color : source_color = vec3(1.0);
uniform vec3 tint = vec3(1.0);   // day / night

varying vec3 world_pos;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Ordered 4x4 dither: the regular cross-hatch of pixel art, not random noise.
float bayer(vec2 p) {
	const float M[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
	ivec2 i = ivec2(mod(p, 4.0));
	return (M[i.x + i.y * 4] + 0.5) / 16.0;
}

float height_of(vec2 cell) {
	return texture(height_tex, (clamp(cell, vec2(0.0), vec2(cells - 1.0)) + 0.5) / cells).r;
}

void vertex() {
	world_pos = VERTEX;
}

// A ramp cut into flat steps: pixel art has no gradients.
vec3 steps3(float k, vec3 a, vec3 b, vec3 c) {
	k = floor(clamp(k, 0.0, 0.999) * 8.0) / 7.0;
	return k < 0.5 ? mix(a, b, k * 2.0) : mix(b, c, k * 2.0 - 1.0);
}

void fragment() {
	vec2 wp = world_pos.xz;
	vec2 pix = floor(wp / px_size);                 // which pixel of the world
	vec2 cellf = wp * cells / extent;
	vec2 cell = floor(cellf);
	vec2 in_cell = fract(cellf);
	vec2 uv = (cell + 0.5) / cells;
	float grain = bayer(floor(pix / 2.0));          // dither in 2x2 pixel blocks

	float h = height_of(cell);
	float h_n = height_of(cell + vec2(0.0, -1.0)), h_s = height_of(cell + vec2(0.0, 1.0));
	float h_w = height_of(cell + vec2(-1.0, 0.0)), h_e = height_of(cell + vec2(1.0, 0.0));
	float temp = texture(temp_tex, uv).r;
	float light = texture(light_tex, uv).r;
	vec3 soil = texture(soil_tex, uv).rgb;

	vec3 col;
	if (overlay == 1) {
		col = steps3((temp + 30.0) / 70.0, vec3(0.12, 0.25, 0.75), vec3(0.92, 0.94, 0.90), vec3(0.85, 0.22, 0.12));
	} else if (overlay == 2) {
		col = steps3(light, vec3(0.05, 0.05, 0.12), vec3(0.60, 0.50, 0.25), vec3(1.0, 0.92, 0.45));
	} else if (overlay == 3) {
		col = steps3(soil.r / 6.0, vec3(0.10, 0.09, 0.08), vec3(0.45, 0.30, 0.12), vec3(0.95, 0.75, 0.25));
	} else if (overlay == 4) {
		col = steps3(soil.g / 150.0, vec3(0.08, 0.08, 0.10), vec3(0.50, 0.25, 0.40), vec3(0.98, 0.92, 0.85));
	} else if (overlay == 5) {
		col = steps3(soil.b / 30.0, vec3(0.08, 0.10, 0.08), vec3(0.45, 0.42, 0.12), vec3(0.80, 0.90, 0.20));
	} else {
		// Height bands, with the boundary between two bands dithered.
		const vec3 BANDS[6] = vec3[](
			vec3(0.80, 0.69, 0.44), vec3(0.67, 0.54, 0.34), vec3(0.52, 0.41, 0.28),
			vec3(0.40, 0.35, 0.31), vec3(0.50, 0.49, 0.50), vec3(0.68, 0.69, 0.72));
		float k = clamp(h / max_height, 0.0, 1.2) * 5.0;
		int band = int(clamp(floor(k + (grain - 0.5) * 0.35), 0.0, 5.0));
		col = BANDS[band];

		// Rich soil is darker; bare ground is paler.
		col *= mix(1.08, 0.80, clamp(floor(soil.r * 1.5) / 6.0, 0.0, 1.0));

		// Hill shading in three flat steps, lit from the top left.
		float slope = (h - h_w) + (h - h_n) - (h - h_e) - (h - h_s);
		col *= slope > 2.0 ? 1.08 : (slope < -2.0 ? 0.90 : 1.0);

		// Cliffs: a dark face along the lower edge of a tile that drops
		// away to the south, and a shadow under one that rises to the north.
		if (h - h_s > 2.5 && in_cell.y > 0.72) col *= 0.55;
		if (h_n - h > 2.5 && in_cell.y < 0.28) col *= 0.75;

		// Frost and snow creep in as it gets colder.
		if (grain < clamp(-temp / 8.0, 0.0, 1.0)) col = vec3(0.88, 0.92, 0.97);

		// The dead: pale specks, more of them where more lies uneaten.
		if (hash(pix + 31.0) < clamp(soil.g / 400.0, 0.0, 0.35)) col = vec3(0.88, 0.84, 0.72);

		// Waste: fouled ground goes murky olive where it builds up.
		col = mix(col, vec3(0.42, 0.40, 0.16), clamp((soil.b - 10.0) / 60.0, 0.0, 0.45));
	}

	// Brush ring, on the pixel grid.
	if (brush.z > 0.0) {
		float d = distance((pix + 0.5) * px_size, brush.xy);
		if (abs(d - brush.z) < px_size * 0.75) col = brush_color;
		else if (d < brush.z) col = mix(col, brush_color, 0.08);
	}

	ALBEDO = col * (overlay == 0 ? tint : vec3(1.0));
}
