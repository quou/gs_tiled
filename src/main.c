#include <gs.h>
#include <gs_xml.h>

#include "renderer.h"

#define EXPECT(p_, m_) \
	do { if (!p_) { fputs(m_ "\n", stderr); } } while (0)

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
};

struct layer {
	struct tile* tiles;
	uint32_t width;
	uint32_t height;
};

struct map {
	gs_dyn_array(struct tileset) tilesets;
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

		gs_dyn_array_push(map.tilesets, tileset);
	}

	gs_xml_free(doc);

	renderer_init();
}

void my_update() {
	renderer_begin();

	struct quad quad = {
		.position = { 0.0f, 0.0f },
		.dimentions = { 100.0f, 100.0f },
		.rectangle = { 0 }
	};

	renderer_push(&quad);

	renderer_end();
}

void my_shutdown() {
	renderer_deinit();
}

gs_app_desc_t gs_main(int32_t argc, char** argv) {
	return (gs_app_desc_t) {
		.init = my_init,
		.update = my_update,
		.shutdown = my_shutdown
	};
}
