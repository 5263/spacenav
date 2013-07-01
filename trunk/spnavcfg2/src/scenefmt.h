#ifndef SCENEFMT_H_
#define SCENEFMT_H_

struct header {
	char magic[8];
	int num_materials;
	int num_meshes;
	int num_lights;
} __attribute__((packed));

struct mesh_header {
	int num_verts;
	int num_tri;
	int matid;
	float xform[4][4];
	int data_size;
} __attribute__((packed));

struct mesh {
	struct mesh_header hdr;
	float *vert;
	float *norm;
	float *tang;
	float *texcoord;
	unsigned int *vidx;
};

struct light {
	float pos[4];
	float color[4];
} __attribute__((packed));

#define MAX_TEX_FNAME	256
struct material {
	float diffuse[4];
	float specular[4];
	float shininess;
	char texfname[MAX_TEX_FNAME];
} __attribute__((packed));


#endif	/* SCENEFMT_H_ */
