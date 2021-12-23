#version 330 core

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec4 color;
layout (location = 3) in float tex_id;
layout (location = 4) in float use_texture;

uniform mat4 camera;

out VS_OUT {
	vec4 color;
	vec2 uv;
	float texture_id;
	float use_texture;
} vs_out;

void main() {
	vs_out.color = color;
	vs_out.uv = uv;
	vs_out.texture_id = tex_id;
	vs_out.use_texture = use_texture;

	gl_Position = camera * vec4(position, 0.0, 1.0);
}
