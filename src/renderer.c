#include "renderer.h"

#define BATCH_SIZE 1000
#define VERTS_PER_QUAD 4
#define IND_PER_QUAD 6
#define FLOATS_PER_VERT 9

struct {
	gs_command_buffer_t                      cb;
	gs_handle(gs_graphics_vertex_buffer_t)   vb;
	gs_handle(gs_graphics_index_buffer_t)    ib;
	gs_handle(gs_graphics_pipeline_t)        pip;
	gs_handle(gs_graphics_shader_t)          shader;
	gs_handle(gs_graphics_uniform_t)         u_camera;

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
				},
				.size = 4 * sizeof(gs_graphics_vertex_attribute_desc_t)
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

	gs_mat4 camera_mat = gs_mat4_ortho(0.0f, ws.x, ws.y, 0.0f, -1.0f, 1.0f);

	gs_graphics_bind_desc_t binds = {
		.vertex_buffers = {&(gs_graphics_bind_vertex_buffer_desc_t){ .buffer = renderer.vb }},
		.index_buffers = {.desc = &(gs_graphics_bind_index_buffer_desc_t){ .buffer = renderer.ib }},
		.uniforms = {.desc = &(gs_graphics_bind_uniform_desc_t){.uniform = renderer.u_camera, .data = &camera_mat}}
	};

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
	const float x = quad->position.x;
	const float y = quad->position.y;
	const float w = quad->dimentions.x;
	const float h = quad->dimentions.y;

	float verts[] = {
		x,     y,     0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		x + w, y,     0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		x + w, y + h, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		x,     y + h, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
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
