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
void reshape(int x, int y);
void keyb(unsigned char key, int x, int y);
void keyb_up(unsigned char key, int x, int y);
void mouse(int bn, int state, int x, int y);
void motion(int x, int y);
void sball_motion(int x, int y, int z);
void sball_rotate(int rx, int ry, int rz);
void sball_button(int bn, int state);
unsigned int load_texture(const char *fname);

void spnav_thread_func(void *arg);

int pipefd[2];
thrd_t spnav_thread;
struct scene scn;
unsigned int envmap, envmap_blur;

float movex, movey, movez;
float rotx, roty, rotz;
int button[128];

int ignore_spnav_errors;
int use_glut_spaceball;

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

	glClearColor(0.25, 0.25, 0.25, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, 0, -8);
	glRotatef(30, 1, 0, 0);
	glRotatef(-25, 0, 1, 0);

	/* LED light */
	/*{
		static const float lpos[] = {0, 0, 0, 1};
		static const float led_color[] = {0.2, 0.4, 1.0, 1.0};

		glLightfv(GL_LIGHT1, GL_POSITION, lpos);
		glLightfv(GL_LIGHT1, GL_DIFFUSE, led_color);
		glEnable(GL_LIGHT1);
	}*/

	for(i=0; i<scn.num_meshes; i++) {
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
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);

			draw_mesh(&scn, scn.meshes + i);
			glDisable(GL_BLEND);

		} else {
			draw_mesh(&scn, scn.meshes + i);
		}

		if(i == SPART_TOP || i == SPART_BASETOP || i == SPART_BASE) {
			glDisable(GL_TEXTURE_GEN_S);
			glDisable(GL_TEXTURE_GEN_T);
			glDisable(GL_TEXTURE_2D);
		}
	}

	glutSwapBuffers();
	assert(glGetError() == GL_NO_ERROR);
}

void reshape(int x, int y)
{
	glViewport(0, 0, x, y);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0, (float)x / (float)y, 0.5, 500.0);
}

void keyb(unsigned char key, int x, int y)
{
	switch(key) {
	case 27:
		exit(0);
	}
}

void keyb_up(unsigned char key, int x, int y)
{
}

void mouse(int bn, int st, int x, int y)
{
}

void motion(int x, int y)
{
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

unsigned int load_texture(const char *fname)
{
	unsigned int tex;

	struct image *img = load_image(fname);
	if(!img) {
		fprintf(stderr, "failed to load image: %s\n", fname);
		return 0;
	}

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img->width, img->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img->pixels);
	free_image(img);

	return tex;
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
