#ifndef EGL_TEST_H
#define EGL_TEST_H

#include <stdint.h>
#include <sys/types.h>

#include <gui/Surface.h>
#include <gui/SurfaceControl.h>
#include <gui/SurfaceComposerClient.h>

#include <utils/threads.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

using namespace android;


#define MAX_PREFER_ATTR 32
//HAL_PIXEL_FORMAT_RGBA_8888 
#define HARDWARE_NATIVE_VISUAL_ID 1

#define VERTEX_POS_SIZE			3
#define VERTEX_NORMAL_SIZE		3
#define VERTEX_TEXCOORD0_SIZE	2

#define VERTEX_POS_INDX			0
#define VERTEX_NORMAL_INDX		1
#define VERTEX_TEXCOORD0_INDX	2	

class EGLTest {
public:
	EGLTest(int layer);
	virtual ~EGLTest();

	/* session for SurfaceFlinger */
	sp<SurfaceComposerClient> session() const;
	
	void eventLoop();
	void draw();
	int init();
	int esLoadShader (GLenum type, const char *shaderSrc);

	int chooseConfigByEGL();
	int selectEGLConfig(EGLint rederableType);
private:
	void queryActiveUniforms();

	sp<SurfaceComposerClient> mSession;
	static const EGLint attrsPrefer[MAX_PREFER_ATTR]; 
	int mWidth;
	int mHeight;
	int mLayer;
	bool mInit;

	EGLDisplay mDisplay;
	EGLContext mContext;
	EGLSurface mSurface;
	EGLConfig mConfig;
	
	DisplayInfo dinfo;
	sp<SurfaceControl> mFlingerSurfaceControl;
	sp<Surface> mFlingerSurface;

	GLuint mProgramObject;
	
	GLint mvpLoc;
	GLfloat *vertices;
	GLuint *indices;
	int numIndices;

	GLfloat angle;
	ESMatrix mvpMatrix;
};



#endif
