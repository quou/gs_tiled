#pragma once

#include <gs.h>

struct quad {
	gs_handle(gs_graphics_texture_t) texture;
	gs_vec2 position;
	gs_vec2 dimentions;
	gs_vec4 rectangle;
};

void renderer_init();
void renderer_deinit();
void renderer_begin();
void renderer_end();
void renderer_flush();
void renderer_push(struct quad* quad);
