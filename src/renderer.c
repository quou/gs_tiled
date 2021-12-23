#include "renderer.h"

#define BATCH_SIZE 1000
#define VERTS_PER_QUAD 4
#define IND_PER_QUAD 6
#define FLOATS_PER_VERT 10
#define MAX_TEXTURES 32

struct {
	gs_command_buffer_t                      cb;
	gs_handle(gs_graphics_vertex_buffer_t)   vb;
	gs_handle(gs_graphics_index_buffer_t)    ib;
	gs_handle(gs_graphics_pipeline_t)        pip;
	gs_handle(gs_graphics_shader_t)          shader;
	gs_handle(gs_graphics_uniform_t)         u_camera;

	gs_handle(gs_graphics_texture_t) textures[MAX_TEXTURES];
	uint32_t texture_count;

	uint32_t quad_count;
} renderer = { 0 };

void renderer_init() {
	renderer.cb = gs_command_buffer_new();
    
	renderer.vb = gs_graphics_vertex_buffer_create(
		&(gs_graphics_vertex_buffer_desc_t) {
			.data = NULL,
			.size = BATCH_SIZE * VERTS_PER_QUAD * FLOATS_PER_VERT * sizeof(float),
			.usage = GS_GRAPHICS_BUFFER_USAGE_DYNAMIC,
		}
	);

	renderer.ib = gs_graphics_index_buffer_create(
		&(gs_graphics_index_buffer_desc_t) {
			.data = NULL,
			.size = BATCH_SIZE * IND_PER_QUAD * sizeof(uint32_t),
			.usage = GS_GRAPHICS_BUFFER_USAGE_DYNAMIC,
		}
	);

	size_t vert_src_size, frag_src_size;
	char* vert_src = gs_read_file_contents_into_string_null_term("./data/shader/sprite.vert", "rb", &vert_src_size);
	char* frag_src = gs_read_file_contents_into_string_null_term("./data/shader/sprite.frag", "rb", &frag_src_size);

	if (!vert_src || !frag_src) {
		fprintf(stderr, "Failed to read either of the shaders!\n");
	}

	renderer.shader = gs_graphics_shader_create(
		&(gs_graphics_shader_desc_t) {
			.sources = (gs_graphics_shader_source_desc_t[]) {
				{ .type = GS_GRAPHICS_SHADER_STAGE_VERTEX,   .source = vert_src },
				{ .type = GS_GRAPHICS_SHADER_STAGE_FRAGMENT, .source = frag_src },
			},
			.size = 2 * sizeof(gs_graphics_shader_source_desc_t),
			.name = "sprite_shader"
		}
	);

	renderer.u_camera = gs_graphics_uniform_create (
		&(gs_graphics_uniform_desc_t) {
			.name = "camera",
			.layout = &(gs_graphics_uniform_layout_desc_t){.type = GS_GRAPHICS_UNIFORM_MAT4}
		}
	);

	renderer.pip = gs_graphics_pipeline_create(
		&(gs_graphics_pipeline_desc_t) {
			.raster = {
				.shader = renderer.shader,
				.index_buffer_element_size = sizeof(uint32_t)
			},
			.layout = {
				.attrs = (gs_graphics_vertex_attribute_desc_t[]) {
					{ .format = GS_GRAPHICS_VERTEX_ATTRIBUTE_FLOAT2, .name = "position" },
					{ .format = GS_GRAPHICS_VERTEX_ATTRIBUTE_FLOAT2, .name = "uv" },
					{ .format = GS_GRAPHICS_VERTEX_ATTRIBUTE_FLOAT4, .name = "color" },
					{ .format = GS_GRAPHICS_VERTEX_ATTRIBUTE_FLOAT,  .name = "tex_id" },
					{ .format = GS_GRAPHICS_VERTEX_ATTRIBUTE_FLOAT,  .name = "use_texture" },
				},
				.size = 5 * sizeof(gs_graphics_vertex_attribute_desc_t)
			}
		}
	);

	gs_free(vert_src);
	gs_free(frag_src);
}

void renderer_deinit() {

}

void renderer_begin() {
	gs_graphics_clear_desc_t clear = {
		.actions = &(gs_graphics_clear_action_t){.color = {0.1f, 0.1f, 0.1f, 1.0f}}
	};

	gs_graphics_begin_render_pass(&renderer.cb, GS_GRAPHICS_RENDER_PASS_DEFAULT);
	gs_graphics_clear(&renderer.cb, &clear);

	renderer.quad_count = 0;
}

void renderer_flush() {
	const gs_vec2 ws = gs_platform_window_sizev(gs_platform_main_window());
	gs_graphics_set_viewport(&renderer.cb, 0, 0, ws.x, ws.y);

	gs_mat4 camera_mat = gs_mat4_ortho(0.0f, ws.x, ws.y, 0.0f, -1.0f, 1.0f);

	gs_graphics_bind_desc_t binds = {
		.vertex_buffers = {&(gs_graphics_bind_vertex_buffer_desc_t){ .buffer = renderer.vb }},
		.index_buffers = {.desc = &(gs_graphics_bind_index_buffer_desc_t){ .buffer = renderer.ib }},
		.uniforms = {
			.desc = (gs_graphics_bind_uniform_desc_t[33]) {
				{ .uniform = renderer.u_camera, .data = &camera_mat }
			}, .size = (renderer.texture_count + 1) * sizeof(gs_graphics_bind_uniform_desc_t)
		},
		.image_buffers = {
			.desc = (gs_graphics_bind_image_buffer_desc_t[32]) { 0 },
			.size = renderer.texture_count * sizeof(gs_graphics_bind_image_buffer_desc_t)
		}
	};

	for (uint32_t i = 0; i < renderer.texture_count; i++) {
		gs_graphics_uniform_desc_t u_desc = {
				.layout = &(gs_graphics_uniform_layout_desc_t) {
					.type = GS_GRAPHICS_UNIFORM_SAMPLER2D
				},
				.stage = GS_GRAPHICS_SHADER_STAGE_FRAGMENT,
		};

		sprintf(u_desc.name, "textures[%d]", i);

		binds.image_buffers.desc[i] = (gs_graphics_bind_image_buffer_desc_t) {
			renderer.textures[i],
			i,
			GS_GRAPHICS_ACCESS_READ_ONLY
		};

		binds.uniforms.desc[1 + i] = (gs_graphics_bind_uniform_desc_t) {
			.uniform = gs_graphics_uniform_create(&u_desc),
			.data = renderer.textures + i
		};
	}

	gs_graphics_bind_pipeline(&renderer.cb, renderer.pip);
	gs_graphics_apply_bindings(&renderer.cb, &binds);
	gs_graphics_draw(&renderer.cb,
		&(gs_graphics_draw_desc_t) {
			.start = 0,
			.count = renderer.quad_count * IND_PER_QUAD
	});

	renderer.quad_count = 0;
}

void renderer_end() {
	renderer_flush();

	gs_graphics_end_render_pass(&renderer.cb);
	gs_graphics_submit_command_buffer(&renderer.cb);
}

void renderer_push(struct quad* quad) {
	float tx, ty, tw, th;

	int32_t tex_id = -1;
	if (quad->use_texture) {
		tx = (float)quad->rectangle.x / quad->texture_size.x;
		ty = (float)quad->rectangle.y / quad->texture_size.y;
		tw = (float)quad->rectangle.z / quad->texture_size.x;
		th = (float)quad->rectangle.w / quad->texture_size.y;

		for (uint32_t i = 0; i < renderer.texture_count; i++) {
			if (renderer.textures[i].id == quad->texture.id) {
				tex_id = i;
				break;
			}	
		}

		if (tex_id == -1) {
			renderer.textures[renderer.texture_count] = quad->texture;
			tex_id = renderer.texture_count++;

			if (renderer.texture_count >= 32) {
				renderer_flush();
				tex_id = 0;
				renderer.textures[0] = quad->texture;
			}
		}
	}

	const float x = quad->position.x;
	const float y = quad->position.y;
	const float w = quad->dimentions.x;
	const float h = quad->dimentions.y;

	const float r = (float)quad->color.r / 255.0f;
	const float g = (float)quad->color.g / 255.0f;
	const float b = (float)quad->color.b / 255.0f;

	float verts[] = {
		x,     y,     tx,      ty,      r, g, b, 1.0f, (float)tex_id, (float)quad->use_texture,
		x + w, y,     tx + tw, ty,      r, g, b, 1.0f, (float)tex_id, (float)quad->use_texture,
		x + w, y + h, tx + tw, ty + th, r, g, b, 1.0f, (float)tex_id, (float)quad->use_texture,
		x,     y + h, tx,      ty + th, r, g, b, 1.0f, (float)tex_id, (float)quad->use_texture
	};

	const uint32_t idx_off = renderer.quad_count * VERTS_PER_QUAD;

	uint32_t indices[] = {
		idx_off + 3, idx_off + 2, idx_off + 1,
		idx_off + 3, idx_off + 1, idx_off + 0
	};

	gs_graphics_vertex_buffer_request_update(&renderer.cb, renderer.vb,
		&(gs_graphics_vertex_buffer_desc_t) {
			.data = verts,
			.size = VERTS_PER_QUAD * FLOATS_PER_VERT * sizeof(float),

			.usage = GS_GRAPHICS_BUFFER_USAGE_DYNAMIC,

			.update = { 
				.type = GS_GRAPHICS_BUFFER_UPDATE_SUBDATA,
				.offset = renderer.quad_count * VERTS_PER_QUAD * FLOATS_PER_VERT * sizeof(float)
			}
		});

	gs_graphics_index_buffer_request_update(&renderer.cb, renderer.ib,
		&(gs_graphics_index_buffer_desc_t) {
			.data = indices,
			.size = IND_PER_QUAD * sizeof(uint32_t),

			.usage = GS_GRAPHICS_BUFFER_USAGE_DYNAMIC,

			.update = { 
				.type = GS_GRAPHICS_BUFFER_UPDATE_SUBDATA,
				.offset = renderer.quad_count * IND_PER_QUAD * sizeof(uint32_t)
			}
		}
	);

	renderer.quad_count++;

	if (renderer.quad_count > BATCH_SIZE) {
		renderer_flush();
	}
}
