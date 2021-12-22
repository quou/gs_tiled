#version 330 core

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec4 color;
layout (location = 3) in float tex_id;

uniform mat4 camera;

void main() {
	gl_Position = camera * vec4(position, 0.0, 1.0);
}
