#ifndef EGL_UTIL_H
#define EGL_UTIL_H

#include <stdlib.h>

#include <GLES/gl.h>
#include <GLES/glext.h>

#include <EGL/eglext.h>
#include <EGL/egl.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
	GLfloat m[4][4];
} ESMatrix;

//basic geometry

int esGenCube(float scale, GLfloat **vertices, GLfloat **normals,
				GLfloat **texCoords, GLuint **indices);

int esGenSquareGrid(int size, GLfloat **vertices, GLuint **indices);

//transform

void esScale(ESMatrix *result, GLfloat sx, GLfloat sy, GLfloat sz);

void esTranslate(ESMatrix *result, GLfloat tx, GLfloat ty, GLfloat tz);

void esRotate(ESMatrix *result, GLfloat angle, GLfloat x, GLfloat y, GLfloat z);

void esFrustum(ESMatrix *result, float left, float right, float bottom,
			float top, float nearZ, float farZ);

void esPerspective(ESMatrix *result, float fovy, float aspect, float nearZ,
			float farZ);

void esOrtho(ESMatrix *result, float left, float right, float botoom, float top,
			float nearZ, float farZ);

void esMatrixLookAt (ESMatrix *result, 
					float posX, float posY, float posZ,
					float lookAtX, float lookAtY, float lookAtZ,
					float upX, float upY, float upZ);
void esMatrixMultiply (ESMatrix *result, ESMatrix *srcA, ESMatrix *srcB);

void esMatrixLoadIdentity (ESMatrix *result);

#ifdef __cplusplus
}
#endif

#endif
