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

#include "EGLUtil.h"

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

#define FOURCC_DXT1 0x31545844 // Equivalent to "DXT1" in ASCII
#define FOURCC_DXT3 0x33545844 // Equivalent to "DXT3" in ASCII
#define FOURCC_DXT5 0x35545844 // Equivalent to "DXT5" in ASCII



class EGLTest {
public:
	EGLTest(int layer);
	virtual ~EGLTest();

	/* session for SurfaceFlinger */
	sp<SurfaceComposerClient> session() const;
	
	void eventLoop();
	void prepare();
	void draw();
	void swap();
	void update(long frame);
	int init();
	int esLoadShader (GLenum type, const char *shaderSrc);

	int chooseConfigByEGL();
	int selectEGLConfig(EGLint rederableType);
private:
	void queryActiveUniforms();
	GLuint createSimpleTexture2D();
	GLuint createMipMappedTexture2D();
	GLubyte *genCheckImage(int width, int height, int checkSize);
	GLboolean genMipMap2D(GLubyte *src, GLubyte **dst, int srcWidth, 
			int srcHeight, int *dstWidth, int *dstHeight);
	GLuint loadDDS(const char *imagepath);
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

	//for VBO
	GLuint mVertexBuffer;
	GLuint mUVBuffer;
	GLuint mIndexBuffer;


	//for texture demo
	GLuint textureId;
	GLint positionLoc;
	GLint texCoordLoc;
	GLint samplerLoc;

	//for offset mipmap
	GLuint offsetLoc;
};



#endif
