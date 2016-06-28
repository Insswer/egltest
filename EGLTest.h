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

};



#endif
