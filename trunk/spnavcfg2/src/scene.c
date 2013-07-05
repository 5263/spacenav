#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <GL/glew.h>
#include "scene.h"

static void byteswap_header(struct header *hdr);
static void byteswap_mesh_header(struct mesh_header *hdr);
static void byteswap_materials(struct material *matarr, int num_mat);
static void byteswap_floats(float *ptr, int num);
static void byteswap_uints(unsigned int *ptr, int num);

static void setup_material(const struct material *mat);

int load_scene(struct scene *scn, const char *fname)
{
	int i;
	unsigned int bigendian = 0xdeadbeef;
	FILE *fp;
	struct header hdr;

	bigendian = ((unsigned char*)&bigendian)[0] == 0xde ? 1 : 0;
	printf("%s endian\n", bigendian ? "big" : "little");

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
	if(bigendian) {
		byteswap_header(&hdr);
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
	if(bigendian) {
		byteswap_materials(scn->materials, scn->num_materials);
	}

	/* then all the meshes, prefixed by a mesh header each */
	for(i=0; i<scn->num_meshes; i++) {
		int nverts, ntri;

		if(fread(&scn->meshes[i].hdr, sizeof scn->meshes[i].hdr, 1, fp) < 1) {
			perror("failed to read mesh header\n");
			goto err;
		}
		if(bigendian) {
			byteswap_mesh_header(&scn->meshes[i].hdr);
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

		if(bigendian) {
			byteswap_floats(scn->meshes[i].vert, 3 * nverts);
			byteswap_floats(scn->meshes[i].norm, 3 * nverts);
			byteswap_floats(scn->meshes[i].tang, 3 * nverts);
			byteswap_floats(scn->meshes[i].texcoord, 3 * nverts);
			byteswap_uints(scn->meshes[i].vidx, 3 * ntri);
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
		draw_mesh(scn, scn->meshes + i);
	}
}

void draw_mesh(struct scene *scn, struct mesh *m)
{
	setup_material(scn->materials + m->hdr.matid);

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

static int swap_int(int x)
{
	return (((unsigned int)x & 0xff) << 24) | \
		(((unsigned int)x & 0xff00) << 8) | \
		(((unsigned int)x & 0xff0000) >> 8) | \
		(((unsigned int)x & 0xff000000) >> 24);
}

static float swap_float(float x)
{
	float res;
	int ix = *(int*)&x;
	ix = swap_int(ix);
	res = *(float*)&ix;
	return res;
}

static void byteswap_header(struct header *hdr)
{
	hdr->num_meshes = swap_int(hdr->num_meshes);
	hdr->num_materials = swap_int(hdr->num_materials);
	hdr->num_lights = swap_int(hdr->num_lights);
}

static void byteswap_mesh_header(struct mesh_header *hdr)
{
	hdr->num_verts = swap_int(hdr->num_verts);
	hdr->num_tri = swap_int(hdr->num_tri);
	hdr->matid = swap_int(hdr->matid);
	hdr->data_size = swap_int(hdr->data_size);
}

static void byteswap_materials(struct material *matarr, int num_mat)
{
	int i, j;

	for(i=0; i<num_mat; i++) {
		for(j=0; j<4; j++) {
			matarr[i].diffuse[j] = swap_float(matarr[i].diffuse[j]);
		}
		matarr[i].shininess = swap_float(matarr[i].shininess);
	}
}

static void byteswap_floats(float *ptr, int num)
{
	int i;

	for(i=0; i<num; i++) {
		*ptr = swap_float(*ptr);
		ptr++;
	}
}

static void byteswap_uints(unsigned int *ptr, int num)
{
	int i;

	for(i=0; i<num; i++) {
		*ptr = swap_int(*ptr);
		ptr++;
	}
}
