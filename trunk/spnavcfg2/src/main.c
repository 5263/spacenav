#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <GL/glew.h>
#ifndef __APPLE__
#include <GL/glut.h>
#else
#include <GLUT/glut.h>
#endif
#include "scene.h"

void disp(void);
void reshape(int x, int y);
void keyb(unsigned char key, int x, int y);
void keyb_up(unsigned char key, int x, int y);
void mouse(int bn, int state, int x, int y);
void motion(int x, int y);

struct scene spnav_scn;

int main(int argc, char **argv)
{
	float ldir[] = {-1, 1, 2, 0};

	glutInit(&argc, argv);
	glutInitWindowSize(1024, 768);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE | GLUT_MULTISAMPLE);
	glutCreateWindow("Spacenav configuration tool");

	glutDisplayFunc(disp);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyb);
	glutKeyboardUpFunc(keyb_up);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);

	glewInit();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_CULL_FACE);

	glLightfv(GL_LIGHT0, GL_POSITION, ldir);

	if(GLEW_ARB_multisample) {
		glEnable(GL_MULTISAMPLE);
	}

	if(load_scene(&spnav_scn, "data/spacenav.scene") == -1) {
		return 1;
	}

	glutMainLoop();
	return 0;
}

void disp(void)
{
	glClearColor(0.15, 0.15, 0.15, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, 0, -8);
	glRotatef(30, 1, 0, 0);
	glRotatef(-25, 0, 1, 0);

	draw_scene(&spnav_scn);

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
