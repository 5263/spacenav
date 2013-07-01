#ifndef SCENE_H_
#define SCENE_H_

#include "scenefmt.h"

struct scene {
	struct mesh *meshes;
	int num_meshes;

	struct material *materials;
	int num_materials;

	struct light *lights;
	int num_lights;
};

int load_scene(struct scene *scn, const char *fname);
void destroy_scene(struct scene *scn);

void draw_scene(struct scene *scn);
void draw_mesh(struct scene *scn, struct mesh *m);

#endif	/* SCENE_H_ */
