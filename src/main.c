#include <gs.h>
#include <gs_xml.h>

#include "renderer.h"

#define EXPECT(p_, m_) \
	do { if (!(p_)) { fputs(m_ "\n", stderr); } } while (0)

#define SPRITE_SCALE 2

struct tile {
	uint32_t id;
	uint32_t tileset_id;
};

struct tileset {
	gs_handle(gs_graphics_texture_t) texture;
	uint32_t tile_count;
	uint32_t tile_width;
	uint32_t tile_height;
	uint32_t first_gid;

	uint32_t width, height;
};

struct layer {
	struct tile* tiles;
	uint32_t width;
	uint32_t height;

	gs_color_t tint;
};

struct object {
	uint32_t id;
	int32_t x, y, width, height;
};

struct object_group {
	gs_dyn_array(struct object) objects;

	gs_color_t color;
};

struct map {
	gs_dyn_array(struct tileset) tilesets;
	gs_dyn_array(struct object_group) object_groups;
	gs_dyn_array(struct layer) layers;
};

struct map map = { 0 };

void my_init() {
	gs_xml_document_t* doc = gs_xml_parse_file("./data/map.tmx");
	if (!doc) {
		fprintf(stderr, "Failed to parse XML: %s\n", gs_xml_get_error());
		return;
	}

	gs_xml_node_t* map_node = gs_xml_find_node(doc, "map");
	EXPECT(map_node, "Must have a map node!");

	for (gs_xml_node_iter_t it = gs_xml_new_node_child_iter(map_node, "tileset"); gs_xml_node_iter_next(&it);) {
		struct tileset tileset = { 0 };

		tileset.first_gid = (uint32_t)gs_xml_find_attribute(it.current, "firstgid")->value.number;

		char tileset_path[256];
		strcpy(tileset_path, "./data/");
		strcat(tileset_path, gs_xml_find_attribute(it.current, "source")->value.string);
		gs_xml_document_t* tileset_doc = gs_xml_parse_file(tileset_path);
		if (!tileset_doc) {
			fprintf(stderr, "Failed to parse XML from %s: %s\n", tileset_path, gs_xml_get_error());
			return;
		}

		gs_xml_node_t* tileset_node = gs_xml_find_node(tileset_doc, "tileset");
		tileset.tile_width  = (uint32_t)gs_xml_find_attribute(tileset_node, "tilewidth")->value.number;
		tileset.tile_height = (uint32_t)gs_xml_find_attribute(tileset_node, "tileheight")->value.number;
		tileset.tile_count  = (uint32_t)gs_xml_find_attribute(tileset_node, "tilecount")->value.number;

		gs_xml_node_t* image_node = gs_xml_find_node_child(tileset_node, "image");
		const char* image_path = gs_xml_find_attribute(image_node, "source")->value.string;

		char full_image_path[256];
		strcpy(full_image_path, "./data/");
		strcat(full_image_path, image_path);

		FILE* checker = fopen(full_image_path, "rb"); /* Check that the file exists. */
		if (!checker) { fprintf(stderr, "Failed to fopen texture file: %s\n"); return; }
		fclose(checker);

		void* tex_data = NULL;
		uint32_t w, h, cc;
		gs_util_load_texture_data_from_file(full_image_path, &w, &h, &cc, &tex_data, false);

		tileset.texture = gs_graphics_texture_create (
			&(gs_graphics_texture_desc_t){
				.width = w,
				.height = h,
				.format = GS_GRAPHICS_TEXTURE_FORMAT_RGBA8,
				.min_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST,
				.mag_filter = GS_GRAPHICS_TEXTURE_FILTER_NEAREST,
				.data = tex_data
			}
		);

		tileset.width = w;
		tileset.height = h;

		gs_free(tex_data);

		gs_dyn_array_push(map.tilesets, tileset);
	}

	for (gs_xml_node_iter_t it = gs_xml_new_node_child_iter(map_node, "layer"); gs_xml_node_iter_next(&it);) {
		gs_xml_node_t* layer_node = it.current;

		struct layer layer = { 0 };
		layer.tint = (gs_color_t) { 255, 255, 255, 255 };

		layer.width  = (uint32_t)gs_xml_find_attribute(layer_node, "width")->value.number;
		layer.height = (uint32_t)gs_xml_find_attribute(layer_node, "height")->value.number;

		gs_xml_attribute_t* tint_attrib = gs_xml_find_attribute(layer_node, "tintcolor");
		if (tint_attrib) {
			const char* hexstring = tint_attrib->value.string;
			uint32_t* cols = (uint32_t*)layer.tint.rgba;
			*cols = (uint32_t)strtol(hexstring + 1, NULL, 16);
			layer.tint.a = 255;
		}

		gs_xml_node_t* data_node = gs_xml_find_node_child(layer_node, "data");

		const char* encoding = gs_xml_find_attribute(data_node, "encoding")->value.string;

		if (strcmp(encoding, "csv") != 0) {
			fprintf(stderr, "Only CSV data encoding is supported.\n");
			return;
		}

		const char* data_text = data_node->text;

		const char* cd_ptr = data_text;

		layer.tiles = gs_malloc(layer.width * layer.height * sizeof(struct tile));

		for (uint32_t y = 0; y < layer.height; y++) {
			for (uint32_t x = 0; x < layer.width; x++) {
				uint32_t gid = (uint32_t)strtod(cd_ptr, NULL);
				uint32_t tls_id = 0;

				uint32_t closest = 0;
				for (uint32_t i = 0; i < gs_dyn_array_size(map.tilesets); i++) {
					if (map.tilesets[i].first_gid <= gid) {
						if (map.tilesets[i].first_gid > closest) {
							closest = map.tilesets[i].first_gid;
							tls_id = i;
						}
					}
				}

				layer.tiles[x + y * layer.width].id = gid;
				layer.tiles[x + y * layer.width].tileset_id = tls_id;

				while (*cd_ptr && *cd_ptr != ',') {
					cd_ptr++;
				}

				cd_ptr++; /* Skip the comma. */
			}
		}

		gs_dyn_array_push(map.layers, layer);
	}

	for (gs_xml_node_iter_t it = gs_xml_new_node_child_iter(map_node, "objectgroup"); gs_xml_node_iter_next(&it);) {
		gs_xml_node_t* object_group_node = it.current;

		struct object_group object_group = { 0 };
		object_group.color = (gs_color_t) { 255, 255, 255, 255 };

		gs_xml_attribute_t* color_attrib = gs_xml_find_attribute(object_group_node, "color");
		if (color_attrib) {
			const char* hexstring = color_attrib->value.string;
			uint32_t* cols = (uint32_t*)object_group.color.rgba;
			*cols = (uint32_t)strtol(hexstring + 1, NULL, 16);
			object_group.color.a = 128;
		}
	
		for (gs_xml_node_iter_t iit = gs_xml_new_node_child_iter(object_group_node, "object"); gs_xml_node_iter_next(&iit);) {
			gs_xml_node_t* object_node = iit.current;

			struct object object = { 0 };
			object.id = (int32_t)gs_xml_find_attribute(object_node, "id")->value.number;
			object.x  = (int32_t)gs_xml_find_attribute(object_node, "x")->value.number;
			object.y  = (int32_t)gs_xml_find_attribute(object_node, "y")->value.number;

			gs_xml_attribute_t* attrib;
			if (attrib = gs_xml_find_attribute(object_node, "width")) {
				object.width = attrib->value.number;
			} else {
				object.width = 1;
			}

			if (attrib = gs_xml_find_attribute(object_node, "height")) {
				object.height = attrib->value.number;
			} else {
				object.height = 1;
			}

			gs_dyn_array_push(object_group.objects, object);
		}

		gs_dyn_array_push(map.object_groups, object_group);
	}

	gs_xml_free(doc);

	renderer_init();
}

void my_update() {
	renderer_begin();

	for (uint32_t i = 0; i < gs_dyn_array_size(map.layers); i++) {
		struct layer* layer = map.layers + i;

		for (uint32_t y = 0; y < layer->height; y++) {
			for (uint32_t x = 0; x < layer->width; x++) {
				struct tile* tile = layer->tiles + (x + y * layer->width);
				if (tile->id != 0) {
					struct tileset* tileset = map.tilesets + tile->tileset_id;

					int32_t tsxx = (tile->id % (tileset->width / tileset->tile_width) - 1) * tileset->tile_width;
					int32_t tsyy = tileset->tile_height * ((tile->id - tileset->first_gid) / (tileset->width / tileset->tile_width));

					struct quad quad = {
						.position = {
							(float)(x * tileset->tile_width * SPRITE_SCALE),
							(float)(y * tileset->tile_height * SPRITE_SCALE)
						},
						.use_texture = true,
						.texture_size = { tileset->width, tileset->height },
						.dimentions = { tileset->tile_width * SPRITE_SCALE, tileset->tile_height * SPRITE_SCALE },
						.texture = tileset->texture,
						.rectangle = { tsxx, tsyy, tileset->tile_width, tileset->tile_height },
						.color = layer->tint
					};
					renderer_push(&quad);
				}
			}
		}
	}

	for (uint32_t i = 0; i < gs_dyn_array_size(map.object_groups); i++) {
		struct object_group* group = map.object_groups + i;

		for (uint32_t ii = 0; ii < gs_dyn_array_size(map.object_groups[i].objects); ii++) {
			struct object* object = group->objects + ii;

			struct quad quad = {
				.position = {
					(float)(object->x * SPRITE_SCALE),
					(float)(object->y * SPRITE_SCALE)
				},
				.use_texture = false,
				.dimentions = { object->width * SPRITE_SCALE, object->height * SPRITE_SCALE },
				.color = group->color
			};
			renderer_push(&quad);
		}
	}

	renderer_end();
}

void my_shutdown() {
	for (uint32_t i = 0; i < gs_dyn_array_size(map.tilesets); i++) {
		gs_graphics_texture_destroy(map.tilesets[i].texture);
	}

	for (uint32_t i = 0; i < gs_dyn_array_size(map.layers); i++) {
		gs_free(map.layers[i].tiles);
	}

	gs_dyn_array_free(map.layers);
	gs_dyn_array_free(map.tilesets);

	renderer_deinit();
}

gs_app_desc_t gs_main(int32_t argc, char** argv) {
	return (gs_app_desc_t) {
		.init = my_init,
		.update = my_update,
		.shutdown = my_shutdown
	};
}
