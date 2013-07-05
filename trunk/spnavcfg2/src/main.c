#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <GL/glew.h>
#ifndef __APPLE__
#include <GL/glut.h>
#else
#include <GLUT/glut.h>
#endif
#ifdef HAVE_C11_THREADS_H_
#include <threads.h>
#else
#include "c11threads.h"
#endif
#include "scene.h"
#include "spnav.h"
#include "image.h"
#include "fblur.h"

/* space navigator model parts */
enum {
	SPART_BASE,
	SPART_LEDGLOW,
	SPART_BASETOP,
	SPART_PUCK,
	SPART_TOP
};

void cleanup(void);
void disp(void);
void draw_space_navigator(void);
void post_glow(void);
void reshape(int x, int y);
void keyb(unsigned char key, int x, int y);
void keyb_up(unsigned char key, int x, int y);
void mouse(int bn, int state, int x, int y);
void motion(int x, int y);
void sball_motion(int x, int y, int z);
void sball_rotate(int rx, int ry, int rz);
void sball_button(int bn, int state);
unsigned int create_texture(int xsz, int ysz, unsigned char *pixels);
unsigned int load_texture(const char *fname);
int next_pow2(int x);
void resample_image(unsigned char *pixels, int xsz, int ysz, int factor, int brighten, int *nx, int *ny);

void spnav_thread_func(void *arg);

int width, height;
unsigned char *framebuf;

int opt_use_glow = 1;

int pipefd[2];
thrd_t spnav_thread;
struct scene scn;
unsigned int envmap, envmap_blur;
unsigned int glow_tex;

#define GLOW_SZ_DIV		6
int tex_xsz, tex_ysz, glow_xsz, glow_ysz;

int glow_iter = 3;
int blur_size = 3;

float movex, movey, movez;
float rotx, roty, rotz;
int button[128];

int ignore_spnav_errors;
int use_glut_spaceball;

float cam_phi = 30;
float cam_theta = -25;
float cam_dist = 8;

int main(int argc, char **argv)
{
	int i;
	float ldir[] = {-1, 1, 2, 0};

	for(i=1; i<argc; i++) {
		if(strcmp(argv[i], "-dummy") == 0) {
			ignore_spnav_errors = 1;
		} else if(strcmp(argv[i], "-glut-spaceball") == 0) {
			use_glut_spaceball = 1;
		} else {
			fprintf(stderr, "invalid argument: %s\n", argv[i]);
			return 1;
		}
	}

	glutInit(&argc, argv);
	glutInitWindowSize(1024, 768);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE | GLUT_MULTISAMPLE);
	glutCreateWindow("Spacenav configuration tool");

	glutIdleFunc(glutPostRedisplay);
	glutDisplayFunc(disp);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyb);
	glutKeyboardUpFunc(keyb_up);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	if(use_glut_spaceball) {
		glutSpaceballMotionFunc(sball_motion);
		glutSpaceballRotateFunc(sball_rotate);
		glutSpaceballButtonFunc(sball_button);
	}

	glewInit();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_CULL_FACE);

	glLightfv(GL_LIGHT0, GL_POSITION, ldir);

	if(GLEW_ARB_multisample) {
		glEnable(GL_MULTISAMPLE);
	}

	if(!(envmap = load_texture("data/envmap.tga"))) {
		return 1;
	}
	if(!(envmap_blur = load_texture("data/envmap_blurry.tga"))) {
		return 1;
	}
	glow_tex = create_texture(64, 64, 0);	/* will fix the size in reshape */

	if(load_scene(&scn, "data/spacenav.scene") == -1) {
		return 1;
	}

	if(!use_glut_spaceball) {
		/* setup a pipe to signal the spacenav thread when we want to quit */
		if(pipe(pipefd) == -1) {
			perror("failed to create self-pipe");
			return 1;
		}

		if(thrd_create(&spnav_thread, spnav_thread_func, 0) == -1) {
			perror("failed to spawn spacenav monitoring thread");
			return 1;
		}
	}

	atexit(cleanup);
	glutMainLoop();
	return 0;
}

void cleanup(void)
{
	/* signal the thread to quit, and join */
	if(!use_glut_spaceball) {
		int res;
		write(pipefd[1], pipefd, 1);
		thrd_join(spnav_thread, &res);
	}
}

void disp(void)
{
	int i;

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, 0, -cam_dist);
	glRotatef(cam_phi, 1, 0, 0);
	glRotatef(-cam_theta, 0, 1, 0);

	if(opt_use_glow) {
		glow_xsz = width / GLOW_SZ_DIV;
		glow_ysz = height / GLOW_SZ_DIV;

		glViewport(0, 0, glow_xsz, glow_ysz);

		/* first render the LED into the render target */
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		/* fill the zbuffer */
		glColorMask(0, 0, 0, 0);
		draw_space_navigator();
		glColorMask(1, 1, 1, 1);

		/* draw the led rim */
		draw_mesh(&scn, scn.meshes + SPART_LEDGLOW);

		glReadPixels(0, 0, glow_xsz, glow_ysz, GL_RGBA, GL_UNSIGNED_BYTE, framebuf);

		glViewport(0, 0, width, height);
	}

	glClearColor(0.25, 0.25, 0.25, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	draw_space_navigator();

	if(opt_use_glow) {
		/* add the glow */
		for(i=0; i<glow_iter; i++) {
			fast_blur(BLUR_BOTH, blur_size, (uint32_t*)framebuf, glow_xsz, glow_ysz);
			glBindTexture(GL_TEXTURE_2D, glow_tex);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, glow_xsz, glow_ysz, GL_RGBA, GL_UNSIGNED_BYTE, framebuf);

			post_glow();
		}
	}

	glutSwapBuffers();
	assert(glGetError() == GL_NO_ERROR);

	/* TODO framerate monitor and limiter */
}

void draw_space_navigator(void)
{
	int i;

	for(i=0; i<scn.num_meshes; i++) {
		glPushAttrib(GL_ENABLE_BIT);

		if(i == SPART_TOP || i == SPART_BASETOP || i == SPART_BASE) {
			glEnable(GL_TEXTURE_2D);

			if(i == SPART_BASE) {
				glBindTexture(GL_TEXTURE_2D, envmap_blur);
			} else {
				glBindTexture(GL_TEXTURE_2D, envmap);
			}
			glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
			glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
			glEnable(GL_TEXTURE_GEN_S);
			glEnable(GL_TEXTURE_GEN_T);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
		}

		if(i == SPART_PUCK || i == SPART_TOP) {
			glPushMatrix();
			glTranslatef(movex * 0.003, movey * 0.003, -movez * 0.003);
			glRotatef(rotx * 0.3, 1, 0, 0);
			glRotatef(roty * 0.3, 0, 1, 0);
			glRotatef(-rotz * 0.3, 0, 0, 1);

			draw_mesh(&scn, scn.meshes + i);
			glPopMatrix();

		} else if(i == SPART_BASE) {
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, envmap_blur);

			draw_mesh(&scn, scn.meshes + i);
			glDisable(GL_TEXTURE_2D);

		} else if(i == SPART_LEDGLOW) {

		} else {
			draw_mesh(&scn, scn.meshes + i);
		}

		glPopAttrib();
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	glDepthMask(0);
	draw_mesh(&scn, scn.meshes + SPART_LEDGLOW);
	glDepthMask(1);
}

void post_glow(void)
{
	float max_s = (float)glow_xsz / (float)tex_xsz;
	float max_t = (float)glow_ysz / (float)tex_ysz;

	glPushAttrib(GL_ENABLE_BIT);

	glBlendFunc(GL_ONE, GL_ONE);
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, glow_tex);

	glBegin(GL_QUADS);
	glColor4f(1, 1, 1, 1);
	glTexCoord2f(0, 0);
	glVertex2f(-1, -1);
	glTexCoord2f(max_s, 0);
	glVertex2f(1, -1);
	glTexCoord2f(max_s, max_t);
	glVertex2f(1, 1);
	glTexCoord2f(0, max_t);
	glVertex2f(-1, 1);
	glEnd();

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glPopAttrib();
}

void reshape(int x, int y)
{
	width = x;
	height = y;
	glViewport(0, 0, x, y);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0, (float)x / (float)y, 0.5, 500.0);

	if(opt_use_glow) {
		glow_xsz = x / GLOW_SZ_DIV;
		glow_ysz = y / GLOW_SZ_DIV;
		printf("glow image size: %dx%d\n", glow_xsz, glow_ysz);

		/* resize the frame buffer */
		free(framebuf);
		if(!(framebuf = malloc(glow_xsz * glow_ysz * 4))) {
			perror("failed to allocate frame buffer");
			abort();
		}

		/* resize the large-enough-for-frame texture */
		tex_xsz = next_pow2(glow_xsz);
		tex_ysz = next_pow2(glow_ysz);
		glBindTexture(GL_TEXTURE_2D, glow_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_xsz, tex_ysz, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	}
}

void keyb(unsigned char key, int x, int y)
{
	switch(key) {
	case 27:
		exit(0);

	case 'g':
		opt_use_glow = !opt_use_glow;
		if(opt_use_glow) {
			// re-validate the frame buffer and glow texture
			reshape(width, height);
		}
		glutPostRedisplay();
		break;

	case '=':
		printf("glow_iter: %d\n", ++glow_iter);
		glutPostRedisplay();
		break;

	case '-':
		if(--glow_iter < 1) {
			glow_iter = 1;
		} else {
			printf("glow_iter: %d\n", glow_iter);
			glutPostRedisplay();
		}
		break;

	case ']':
		printf("blur_size: %d\n", ++blur_size);
		glutPostRedisplay();
		break;

	case '[':
		if(--blur_size < 1) {
			blur_size = 1;
		} else {
			printf("blur_size: %d\n", blur_size);
			glutPostRedisplay();
		}
		break;
	}
}

void keyb_up(unsigned char key, int x, int y)
{
}

static int bnstate[32];
static int prev_x, prev_y;

void mouse(int bn, int st, int x, int y)
{
	bnstate[bn - GLUT_LEFT_BUTTON] = st == GLUT_DOWN;
	prev_x = x;
	prev_y = y;
}

void motion(int x, int y)
{
	int dx = x - prev_x;
	int dy = y - prev_y;
	prev_x = x;
	prev_y = y;

	if(bnstate[0]) {
		cam_theta += dx * 0.5;
		cam_phi += dy * 0.5;

		if(cam_phi < -90) cam_phi = -90;
		if(cam_phi > 90) cam_phi = 90;
		glutPostRedisplay();
	}
	if(bnstate[2]) {
		cam_dist += dy * 0.1;

		if(cam_dist < 0.0) cam_dist = 0.0;
		glutPostRedisplay();
	}
}

void sball_motion(int x, int y, int z)
{
	/* glut specifies a range of -1000 to 1000, spnav goes -500 to 500 */
	x /= 2;
	y /= 2;
	z /= 2;

	movex = x;
#ifdef __APPLE__
	movey = -z;
	movez = -y;
#else
	movey = y;
	movez = z;
#endif
	glutPostRedisplay();
}

void sball_rotate(int rx, int ry, int rz)
{
	/* glut specifies a range of -1800 to 1800, spnav goes -500 to 500 */
	rx = 500 * rx / 1800;
	ry = 500 * ry / 1800;
	rz = 500 * rz / 1800;

	rotx = rx;
#ifdef __APPLE__
	roty = -rz;
	rotz = -ry;
#else
	roty = ry;
	rotz = rz;
#endif
	glutPostRedisplay();
}

void sball_button(int bn, int state)
{
}

unsigned int create_texture(int xsz, int ysz, unsigned char *pixels)
{
	unsigned int tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, xsz, ysz, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels ? pixels : 0);
	return tex;
}

unsigned int load_texture(const char *fname)
{
	unsigned int tex;

	struct image *img = load_image(fname);
	if(!img) {
		fprintf(stderr, "failed to load image: %s\n", fname);
		return 0;
	}

	tex = create_texture(img->width, img->height, img->pixels);
	free_image(img);

	return tex;
}

void resample_image(unsigned char *pixels, int xsz, int ysz, int factor, int brighten, int *nx, int *ny)
{
	int i, j;
	unsigned char *src = pixels;
	unsigned char *dest = pixels;

	if(factor <= 1) {
		*nx = xsz;
		*ny = ysz;
		return;
	}

	/* resample by halving everything in place */
	for(i=0; i<ysz; i+=2) {
		for(j=0; j<xsz; j+=2) {
			int r = (src[0] + src[4] + src[xsz * 4] + src[xsz * 4 + 4]) / 4;
			int g = (src[1] + src[5] + src[xsz * 4 + 1] + src[xsz * 4 + 5]) / 4;
			int b = (src[2] + src[6] + src[xsz * 4 + 2] + src[xsz * 4 + 6]) / 4;

			if(brighten > 0) {
				r += (r * brighten) / 255;
				g += (g * brighten) / 255;
				b += (b * brighten) / 255;
			}

			*dest++ = r > 255 ? 255 : r;
			*dest++ = g > 255 ? 255 : g;
			*dest++ = b > 255 ? 255 : b;
			*dest++ = 255;
			src += 8;
		}
		src += xsz * 4;
	}

	resample_image(pixels, xsz / 2, ysz / 2, factor - 1, brighten, nx, ny);
}

/* ---- spacenav monitoring thread ---- */
void spnav_thread_func(void *arg)
{
	int sfd, maxfd;

	if(spnav_open() == -1) {
		fprintf(stderr, "failed to connect to the spacenav daemon\n");
		if(ignore_spnav_errors) {
			thrd_exit(0);
		}
		exit(0);
	}
	sfd = spnav_fd();

	maxfd = sfd > pipefd[0] ? sfd : pipefd[0];

	for(;;) {
		int res;
		fd_set rdset;

		FD_ZERO(&rdset);
		FD_SET(sfd, &rdset);
		FD_SET(pipefd[0], &rdset);

		while((res = select(maxfd + 1, &rdset, 0, 0, 0)) == -1 && errno == EINTR);

		if(res <= 0) {
			continue;
		}

		if(FD_ISSET(pipefd[0], &rdset)) {
			break;	/* we're done... */
		}

		if(FD_ISSET(sfd, &rdset)) {
			spnav_event ev;

			while(spnav_poll_event(&ev)) {
				switch(ev.type) {
				case SPNAV_EVENT_MOTION:
					movex = ev.motion.x;
					movey = ev.motion.y;
					movez = ev.motion.z;
					rotx = ev.motion.rx;
					roty = ev.motion.ry;
					rotz = ev.motion.rz;
					spnav_remove_events(SPNAV_EVENT_MOTION);
					glutPostRedisplay();
					break;

				case SPNAV_EVENT_BUTTON:
					button[ev.button.bnum] = ev.button.press;
					glutPostRedisplay();
					break;

				default:
					break;
				}
			}
		}
	}

	spnav_close();
}

int next_pow2(int x)
{
	x--;
	x = (x >> 1) | x;
	x = (x >> 2) | x;
	x = (x >> 4) | x;
	x = (x >> 8) | x;
	x = (x >> 16) | x;
	return x + 1;
}
