#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>
#include "scenefmt.h"

int convert_file(const char *fname);
int write_mesh(FILE *fp, struct mesh *mesh);

int swap_yz;

int main(int argc, char **argv)
{
	int i;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(strcmp(argv[i], "-swapyz") == 0) {
				swap_yz = 1;
			} else {
				fprintf(stderr, "invalid option: %s\n", argv[i]);
				return 1;
			}
		} else {
			if(convert_file(argv[i]) == -1) {
				fprintf(stderr, "failed to convert scene file: %s\n", argv[i]);
			}
		}
	}
	return 0;
}

#define AIFLAGS	\
	(aiProcess_CalcTangentSpace | \
	 aiProcess_JoinIdenticalVertices | \
	 aiProcess_Triangulate | \
	 aiProcess_GenSmoothNormals)

int convert_file(const char *fname)
{
	FILE *fp = 0;
	const struct aiScene *scn = 0;
	struct mesh *mesh = 0;
	struct material *mat = 0;
	struct light *light = 0;
	char *out_fname, *dot;
	int i, j;
	struct header hdr;

	if(!(scn = aiImportFile(fname, AIFLAGS))) {
		fprintf(stderr, "failed to import scene file: %s\n", fname);
		return -1;
	}

	out_fname = alloca(strlen(fname) + 1 + strlen(".scene"));
	strcpy(out_fname, fname);
	if(!(dot = strrchr(out_fname, '.'))) {
		dot = out_fname + strlen(out_fname);
	}
	strcpy(dot, ".scene");

	if(!(fp = fopen(out_fname, "wb"))) {
		fprintf(stderr, "failed to open output file: %s: %s\n", out_fname, strerror(errno));
		goto err;
	}

	if(!(mesh = calloc(scn->mNumMeshes, sizeof *mesh))) {
		perror("failed to allocate memory");
		goto err;
	}
	if(!(mat = malloc(scn->mNumMaterials * sizeof *mat))) {
		perror("failed to allocate memory");
		goto err;
	}
	if(!(light = malloc(scn->mNumLights * sizeof *light))) {
		perror("failed to allocate memory");
		goto err;
	}

	for(i=0; i<(int)scn->mNumMaterials; i++) {
		struct aiColor4D color;
		float shin_str;
		unsigned int one = 1;

		printf("material[%d]:\n", i);

		if(aiGetMaterialColor(scn->mMaterials[i], AI_MATKEY_COLOR_DIFFUSE, &color) != 0) {
			color.r = color.g = color.b = 1.0;
		}
		mat[i].diffuse[0] = color.r;
		mat[i].diffuse[1] = color.g;
		mat[i].diffuse[2] = color.b;
		mat[i].diffuse[3] = 1.0;

		printf("  diffuse: %f %f %f\n", color.r, color.g, color.b);

		if(aiGetMaterialColor(scn->mMaterials[i], AI_MATKEY_COLOR_SPECULAR, &color) != 0) {
			color.r = color.g = color.b = 0.0;
		}
		mat[i].specular[0] = color.r;
		mat[i].specular[1] = color.g;
		mat[i].specular[2] = color.b;
		mat[i].specular[3] = 1.0;

		printf("  specular: %f %f %f\n", color.r, color.g, color.b);

		if(aiGetMaterialFloatArray(scn->mMaterials[i], AI_MATKEY_SHININESS_STRENGTH, &shin_str, &one) != 0) {
			printf("FOO1\n");
			shin_str = 1.0;
		}
		if(aiGetMaterialFloatArray(scn->mMaterials[i], AI_MATKEY_SHININESS, &mat[i].shininess, &one) != 0) {
			printf("FOO2\n");
			mat[i].shininess = 0.0;
		}
		mat[i].shininess *= shin_str * 0.001 * 128.0;

		printf("  shininess: %f (str: %f)\n", mat[i].shininess, shin_str);

		memset(mat[i].texfname, 0, sizeof mat[i].texfname);
	}

	for(i=0; i<(int)scn->mNumMeshes; i++) {
		int nverts, nfaces;
		float *varr, *narr, *tarr, *tcarr;
		unsigned int *idxarr;

		nverts = scn->mMeshes[i]->mNumVertices;
		nfaces = scn->mMeshes[i]->mNumFaces;

		mesh[i].hdr.num_verts = nverts;
		mesh[i].hdr.num_tri = nfaces;
		mesh[i].hdr.matid = scn->mMeshes[i]->mMaterialIndex;
		mesh[i].hdr.xform[0][0] = mesh[i].hdr.xform[1][1] = mesh[i].hdr.xform[2][2] = mesh[i].hdr.xform[3][3] = 1.0;
		mesh[i].hdr.data_size = nverts * 3 * sizeof(float) * 4 + nfaces * 3 * sizeof(unsigned int);

		varr = mesh[i].vert = calloc(nverts, sizeof *mesh[i].vert * 3);
		narr = mesh[i].norm = calloc(nverts, sizeof *mesh[i].norm * 3);
		tarr = mesh[i].tang = calloc(nverts, sizeof *mesh[i].tang * 3);
		tcarr = mesh[i].texcoord = calloc(nverts, sizeof *mesh[i].texcoord * 3);
		idxarr = mesh[i].vidx = calloc(nfaces, 3 * sizeof *mesh[i].vidx);

		if(!varr || !narr || !tarr || !tcarr || !idxarr) {
			perror("failed to allocate vertex arrays");
			goto err;
		}

		for(j=0; j<nverts; j++) {
			*varr++ = scn->mMeshes[i]->mVertices[j].x;
			*varr++ = scn->mMeshes[i]->mVertices[j].y;
			*varr++ = scn->mMeshes[i]->mVertices[j].z;

			if(swap_yz) {
				float tmp = varr[-2];
				varr[-2] = varr[-1];
				varr[-1] = tmp;
			}

			if(scn->mMeshes[i]->mNormals) {
				*narr++ = scn->mMeshes[i]->mNormals[j].x;
				*narr++ = scn->mMeshes[i]->mNormals[j].y;
				*narr++ = scn->mMeshes[i]->mNormals[j].z;

				if(swap_yz) {
					float tmp = narr[-2];
					narr[-2] = narr[-1];
					narr[-1] = tmp;
				}
			}

			if(scn->mMeshes[i]->mTangents) {
				*tarr++ = scn->mMeshes[i]->mTangents[j].x;
				*tarr++ = scn->mMeshes[i]->mTangents[j].y;
				*tarr++ = scn->mMeshes[i]->mTangents[j].z;
				if(swap_yz) {
					float tmp = tarr[-2];
					tarr[-2] = tarr[-1];
					tarr[-1] = tmp;
				}
			}

			if(scn->mMeshes[i]->mTextureCoords[0]) {
				*tcarr++ = scn->mMeshes[i]->mTextureCoords[0][j].x;
				*tcarr++ = scn->mMeshes[i]->mTextureCoords[0][j].y;
				*tcarr++ = scn->mMeshes[i]->mTextureCoords[0][j].z;
				if(swap_yz) {
					float tmp = tcarr[-2];
					tcarr[-2] = tcarr[-1];
					tcarr[-1] = tmp;
				}
			}
		}

		for(j=0; j<nfaces; j++) {
			assert(scn->mMeshes[i]->mFaces[j].mNumIndices == 3);
			*idxarr++ = scn->mMeshes[i]->mFaces[j].mIndices[0];
			*idxarr++ = scn->mMeshes[i]->mFaces[j].mIndices[swap_yz ? 2 : 1];
			*idxarr++ = scn->mMeshes[i]->mFaces[j].mIndices[swap_yz ? 1 : 2];
		}
	}

	for(i=0; i<scn->mNumLights; i++) {
		light[i].pos[0] = scn->mLights[i]->mPosition.x;
		light[i].pos[swap_yz ? 2 : 1] = scn->mLights[i]->mPosition.y;
		light[i].pos[swap_yz ? 1 : 2] = scn->mLights[i]->mPosition.z;

		light[i].color[0] = scn->mLights[i]->mColorDiffuse.r;
		light[i].color[1] = scn->mLights[i]->mColorDiffuse.g;
		light[i].color[2] = scn->mLights[i]->mColorDiffuse.b;
		light[i].color[3] = 1.0;
	}

	/* dump everything */
	memcpy(hdr.magic, "SPNAVSCN", 8);
	hdr.num_materials = scn->mNumMaterials;
	hdr.num_meshes = scn->mNumMeshes;
	hdr.num_lights = scn->mNumLights;
	fwrite(&hdr, sizeof hdr, 1, fp);

	if(fwrite(mat, sizeof *mat, hdr.num_materials, fp) < hdr.num_materials) {
		perror("failed to write materials");
		goto err;
	}

	for(i=0; i<hdr.num_meshes; i++) {
		if(write_mesh(fp, mesh + i) == -1) {
			goto err;
		}
		free(mesh[i].vert);
		free(mesh[i].norm);
		free(mesh[i].tang);
		free(mesh[i].texcoord);
		free(mesh[i].vidx);
	}

	if(fwrite(light, sizeof *light, hdr.num_lights, fp) < hdr.num_lights) {
		perror("failed to write lights");
		goto err;
	}

	free(mesh);
	free(light);
	free(mat);

	fclose(fp);
	aiReleaseImport(scn);
	return 0;

err:
	if(mesh) {
		for(i=0; i<scn->mNumMeshes; i++) {
			free(mesh[i].vert);
			free(mesh[i].norm);
			free(mesh[i].tang);
			free(mesh[i].texcoord);
			free(mesh[i].vidx);
		}
		free(mesh);
	}
	free(light);
	free(mat);
	if(scn) {
		aiReleaseImport(scn);
	}
	if(fp) {
		fclose(fp);
	}
	return -1;
}

int write_mesh(FILE *fp, struct mesh *mesh)
{
	printf("writing mesh with %d vertices and %d triangles (mat: %d)\n", mesh->hdr.num_verts, mesh->hdr.num_tri, mesh->hdr.matid);
	if(fwrite(&mesh->hdr, sizeof mesh->hdr, 1, fp) < 1) {
		fprintf(stderr, "failed to write mesh header\n");
		return -1;
	}

	if(fwrite(mesh->vert, sizeof *mesh->vert * 3, mesh->hdr.num_verts, fp) < mesh->hdr.num_verts) {
		fprintf(stderr, "failed to write vertices\n");
		return -1;
	}
	if(fwrite(mesh->norm, sizeof *mesh->norm * 3, mesh->hdr.num_verts, fp) < mesh->hdr.num_verts) {
		fprintf(stderr, "failed to write normals\n");
		return -1;
	}
	if(fwrite(mesh->tang, sizeof *mesh->tang * 3, mesh->hdr.num_verts, fp) < mesh->hdr.num_verts) {
		fprintf(stderr, "failed to write tangents\n");
		return -1;
	}
	if(fwrite(mesh->texcoord, sizeof *mesh->texcoord * 3, mesh->hdr.num_verts, fp) < mesh->hdr.num_verts) {
		fprintf(stderr, "failed to write texcoords\n");
		return -1;
	}
	if(fwrite(mesh->vidx, sizeof *mesh->vidx * 3, mesh->hdr.num_tri, fp) < mesh->hdr.num_tri) {
		fprintf(stderr, "failed to write triangle indices\n");
		return -1;
	}

	return 0;
}
