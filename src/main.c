#include <gs.h>
#include <gs_xml.h>

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
	}

	gs_xml_node_t* map_node = gs_xml_find_node(doc, "map");
	EXPECT(map_node, "Must have a map node!");

	for (gs_xml_node_iter_t it = gs_xml_new_node_child_iter(map_node, "tileset"); gs_xml_node_iter_next(&it);) {
		printf("%s\n", gs_xml_find_attribute(it.current, "source")->value.string);
	}

	gs_xml_free(doc);
}

void my_update() {

}

void my_shutdown() {

}

gs_app_desc_t gs_main(int32_t argc, char** argv) {
	return (gs_app_desc_t) {
		.init = my_init,
		.update = my_update,
		.shutdown = my_shutdown
	};
}
