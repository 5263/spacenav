#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <GL/glew.h>
#include "scene.h"

static void setup_material(const struct material *mat);

int load_scene(struct scene *scn, const char *fname)
{
	int i;
	FILE *fp;
	struct header hdr;

	memset(scn, 0, sizeof *scn);

	if(!(fp = fopen(fname, "rb"))) {
		fprintf(stderr, "failed to load scene file: %s: %s\n", fname, strerror(errno));
		return -1;
	}

	if(fread(&hdr, sizeof hdr, 1, fp) < 1) {
		perror("failed to read file header");
		goto err;
	}
	if(memcmp(hdr.magic, "SPNAVSCN", 8) != 0) {
		fprintf(stderr, "failed to load scene file: %s: invalid magic\n", fname);
		goto err;
	}

	scn->num_meshes = hdr.num_meshes;
	scn->num_lights = hdr.num_lights;
	scn->num_materials = hdr.num_materials;

	scn->meshes = malloc(scn->num_meshes * sizeof *scn->meshes);
	scn->lights = malloc(scn->num_lights * sizeof *scn->lights);
	scn->materials = malloc(scn->num_materials * sizeof *scn->materials);

	if(!scn->meshes || !scn->lights || !scn->materials) {
		perror("failed to allocate memory");
		goto err;
	}

	/* first in the file are the materials */
	if(fread(scn->materials, sizeof *scn->materials, scn->num_materials, fp) < scn->num_materials) {
		perror("failed to read materials");
		goto err;
	}

	/* then all the meshes, prefixed by a mesh header each */
	for(i=0; i<scn->num_meshes; i++) {
		int nverts, ntri;

		if(fread(&scn->meshes[i].hdr, sizeof scn->meshes[i].hdr, 1, fp) < 1) {
			perror("failed to read mesh header\n");
			goto err;
		}
		nverts = scn->meshes[i].hdr.num_verts;
		ntri = scn->meshes[i].hdr.num_tri;

		/* allocate buffers */
		scn->meshes[i].vert = malloc(nverts * 3 * sizeof(float));
		scn->meshes[i].norm = malloc(nverts * 3 * sizeof(float));
		scn->meshes[i].tang = malloc(nverts * 3 * sizeof(float));
		scn->meshes[i].texcoord = malloc(nverts * 3 * sizeof(float));
		scn->meshes[i].vidx = malloc(ntri * 3 * sizeof(unsigned int));

		if(!scn->meshes[i].vert || !scn->meshes[i].norm || !scn->meshes[i].tang ||
				!scn->meshes[i].texcoord || !scn->meshes[i].vidx) {
			perror("failed to allocate mesh buffers");
			goto err;
		}

		if(fread(scn->meshes[i].vert, 3 * sizeof(float), nverts, fp) < nverts) {
			perror("failed to read mesh vertices");
			goto err;
		}
		if(fread(scn->meshes[i].norm, 3 * sizeof(float), nverts, fp) < nverts) {
			perror("failed to read mesh normals");
			goto err;
		}
		if(fread(scn->meshes[i].tang, 3 * sizeof(float), nverts, fp) < nverts) {
			perror("failed to read mesh tangents");
			goto err;
		}
		if(fread(scn->meshes[i].texcoord, 3 * sizeof(float), nverts, fp) < nverts) {
			perror("failed to read mesh texcoords");
			goto err;
		}
		if(fread(scn->meshes[i].vidx, 3 * sizeof(unsigned int), ntri, fp) < ntri) {
			perror("failed to read mesh face indices");
			goto err;
		}
	}

	/* finally read the lights */
	if(fread(scn->lights, sizeof *scn->lights, scn->num_lights, fp) < scn->num_lights) {
		perror("failed to read lights");
		goto err;
	}
	fclose(fp);
	return 0;

err:
	destroy_scene(scn);
	fclose(fp);
	return -1;
}

void destroy_scene(struct scene *scn)
{
	int i;

	if(!scn) {
		return;
	}

	if(scn->meshes) {
		for(i=0; i<scn->num_meshes; i++) {
			free(scn->meshes[i].vert);
			free(scn->meshes[i].norm);
			free(scn->meshes[i].tang);
			free(scn->meshes[i].texcoord);
			free(scn->meshes[i].vidx);
		}
		free(scn->meshes);
	}
	free(scn->lights);
	free(scn->materials);
}

void draw_scene(struct scene *scn)
{
	int i;

	for(i=0; i<scn->num_meshes; i++) {
		setup_material(scn->materials + scn->meshes[i].hdr.matid);
		draw_mesh(scn, scn->meshes + i);
	}
}

void draw_mesh(struct scene *scn, struct mesh *m)
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(3, GL_FLOAT, 0, m->vert);
	glNormalPointer(GL_FLOAT, 0, m->norm);
	glTexCoordPointer(3, GL_FLOAT, 0, m->texcoord);

	glDrawElements(GL_TRIANGLES, m->hdr.num_tri * 3, GL_UNSIGNED_INT, m->vidx);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

static void setup_material(const struct material *mat)
{
	float shininess = mat->shininess <= 0 ? 0 : (mat->shininess > 128 ? 128 : mat->shininess);

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mat->diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat->specular);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
}
