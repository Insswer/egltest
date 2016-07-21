#define LOG_TAG "EGL_TEST"

#include <math.h>

#include <cutils/memory.h>
#include <utils/Log.h>
#include <utils/String8.h>
#include <utils/misc.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include <ui/PixelFormat.h>
#include <ui/DisplayInfo.h>
#include <ui/FramebufferNativeWindow.h>

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>

#include <GLES/gl.h>
#include <GLES/glext.h>

#include <EGL/eglext.h>
#include <EGL/egl.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "EGLTest.h"
#include "EGLShader.h"
#include "gimage/SOIL.h"

using namespace android;

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83f1
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83f2
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83f3
#endif

const EGLint EGLTest::attrsPrefer[MAX_PREFER_ATTR] = {
		EGL_SURFACE_TYPE,	EGL_WINDOW_BIT|EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
//		EGL_RECORDABLE_ANDROID, EGL_TRUE,
		EGL_FRAMEBUFFER_TARGET_ANDROID, EGL_TRUE,
		EGL_RED_SIZE,		8,
		EGL_GREEN_SIZE,		8,
		EGL_BLUE_SIZE,		8,
		EGL_DEPTH_SIZE,		8,
		EGL_STENCIL_SIZE,	8,
		EGL_ALPHA_SIZE,		8,
		EGL_NONE
};

EGLTest::EGLTest(int layer) {
	ALOGI ("EGLTest init");
	mInit = false;	
	mLayer = layer;
}

EGLTest::~EGLTest() {
	ALOGI ("EGLTest destroy");
	if (mInit) {
		IPCThreadState::self()->stopProcess();
		eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (mFlingerSurface != NULL) {
			mFlingerSurface.clear();
		}

		if (mFlingerSurfaceControl != NULL) {
			mFlingerSurfaceControl.clear();
		}

		eglTerminate(mDisplay);

		if (mSession != NULL) {
			mSession->dispose();
			//free session
			mSession.clear();
		}
	}
}


int EGLTest::chooseConfigByEGL() {
	EGLint numConfigs;
	if (EGL_FALSE == eglChooseConfig(mDisplay, 
				attrsPrefer, &mConfig, MAX_PREFER_ATTR, &numConfigs)) {
		printf ("eglChooseConfig failed\n");
		return -1;
	} else {
		printf ("chooseConfigByEGL ok\n");
		return 0;
	}
}

class EGLAttributeVector {
    struct Attribute;
    class Adder;
    friend class Adder;
    KeyedVector<Attribute, EGLint> mList;
    struct Attribute {
        Attribute() {};
        Attribute(EGLint v) : v(v) { }
        EGLint v;
        bool operator < (const Attribute& other) const {
            // this places EGL_NONE at the end
            EGLint lhs(v);
            EGLint rhs(other.v);
            if (lhs == EGL_NONE) lhs = 0x7FFFFFFF;
            if (rhs == EGL_NONE) rhs = 0x7FFFFFFF;
            return lhs < rhs;
        }
    };
    class Adder {
        friend class EGLAttributeVector;
        EGLAttributeVector& v;
        EGLint attribute;
        Adder(EGLAttributeVector& v, EGLint attribute)
            : v(v), attribute(attribute) {
        }
    public:
        void operator = (EGLint value) {
            if (attribute != EGL_NONE) {
                v.mList.add(attribute, value);
            }
        }
        operator EGLint () const { return v.mList[attribute]; }
    };
public:
    EGLAttributeVector() {
        mList.add(EGL_NONE, EGL_NONE);
    }
    void remove(EGLint attribute) {
        if (attribute != EGL_NONE) {
            mList.removeItem(attribute);
        }
    }
    Adder operator [] (EGLint attribute) {
        return Adder(*this, attribute);
    }
    EGLint operator [] (EGLint attribute) const {
       return mList[attribute];
    }
    // cast-operator to (EGLint const*)
    operator EGLint const* () const { return &mList.keyAt(0).v; }
};

int selectConfigForAttribute(EGLDisplay disp, EGLint const* attrs,
		EGLint attribute, EGLint wanted, EGLConfig *config) {
	EGLint numConfigs = -1, n = 0;
	eglGetConfigs(disp, NULL, 0, &numConfigs);
	printf ("read %d configs\n", numConfigs); 
	EGLConfig* const configs = new EGLConfig[numConfigs];
	eglChooseConfig(disp, attrs, configs, numConfigs, &n);

	if (n) {
		if (attribute != EGL_NONE) {
			for (int i = 0; i < n; i++) {
				EGLint value = 0;
				eglGetConfigAttrib(disp, configs[i], attribute, &value);
				if (wanted == value) {
					*config = configs[i];
					delete[] configs;
					return 0;
				}
			}
		} else {
			*config = configs[0];
			delete[] configs;
			return 0;
		}
	}

	delete[] configs;
	return -1;	
}

int EGLTest::selectEGLConfig(EGLint renderableType) {
	EGLint wantedAttribute;
	EGLint wantedAttributeValue;

	EGLAttributeVector attribs;
	if (renderableType) {
		attribs[EGL_RENDERABLE_TYPE] = renderableType;
		//attribs[EGL_RECORDABLE_ANDROID] = EGL_TRUE;
		attribs[EGL_SURFACE_TYPE] = EGL_WINDOW_BIT|EGL_PBUFFER_BIT;
		attribs[EGL_FRAMEBUFFER_TARGET_ANDROID] = EGL_TRUE;
		attribs[EGL_RED_SIZE] = 8;
		attribs[EGL_GREEN_SIZE] = 8;
		attribs[EGL_BLUE_SIZE] = 8;
		wantedAttribute = EGL_NONE;
		wantedAttributeValue = EGL_NONE;
	} else {
		wantedAttribute = EGL_NATIVE_VISUAL_ID;
		wantedAttributeValue = HARDWARE_NATIVE_VISUAL_ID;
	}

	if (0 != selectConfigForAttribute(mDisplay, attribs, wantedAttribute,
				wantedAttributeValue, &mConfig)) {
		printf ("selectConfigForAttribute failed\n");
		EGLint caveat;
		if (eglGetConfigAttrib(mDisplay, mConfig, EGL_CONFIG_CAVEAT, &caveat)) {
			printf ("gose caveat with %d\n", caveat);
		}	
		return 0;
	} 

	return 0;
}

sp<SurfaceComposerClient> EGLTest::session() const {
	return mSession;
}

int EGLTest::esLoadShader(GLenum type, const char *shaderSrc) {
	GLuint shader;
	GLint compiled;

	shader = glCreateShader(type);	
	if (shader == 0) {
		printf ("create shader failed");
		return -1;
	}

	glShaderSource (shader, 1, &shaderSrc, NULL);
	glCompileShader (shader);

	glGetShaderiv (shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLint infoLen = 0;
		glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char *infoLog = new char[infoLen];

			glGetShaderInfoLog (shader, infoLen, NULL, infoLog);
			printf ("ERROR COMPILING SHADER:\n%s\n", infoLog);
			delete infoLog;
		}

		glDeleteShader (shader);
		return -1;
	}	
	return shader;
}

int EGLTest::init() {
	sp<ProcessState> proc(ProcessState::self());
	ProcessState::self()->startThreadPool();

	mSession = new SurfaceComposerClient();
	sp<IBinder> dtoken (SurfaceComposerClient::getBuiltInDisplay(
				ISurfaceComposer::eDisplayIdMain));
	status_t status = SurfaceComposerClient::getDisplayInfo (dtoken, &dinfo);
	printf ("width = %d, height = %d\n", dinfo.w, dinfo.h);
	mFlingerSurfaceControl = mSession->createSurface(String8("EGLTest"),
			dinfo.w, dinfo.h, PIXEL_FORMAT_RGBA_8888);

	mFlingerSurface = mFlingerSurfaceControl->getSurface();
	SurfaceComposerClient::openGlobalTransaction();
	mFlingerSurfaceControl->setLayer(mLayer);
	SurfaceComposerClient::closeGlobalTransaction();
	
	int maxVertexUniform = 0;
	int maxFragmentUniform = 0;
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxVertexUniform);
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &maxFragmentUniform);
	printf ("max vertex uniform num is %d\n", maxVertexUniform);
	printf ("max fragment uniform num is %d\n", maxFragmentUniform);

	ALOGI ("prepare EGL");
	mDisplay = eglGetDisplay (EGL_DEFAULT_DISPLAY);

	int eglMajor, eglMinor = 0;
	eglInitialize(mDisplay, &eglMajor, &eglMinor);
	printf ("init with major %d minor %d\n", eglMajor, eglMinor);

	//use choose
	chooseConfigByEGL();
	//selectEGLConfig(EGL_OPENGL_ES2_BIT);

	const EGLint contextAttributes[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
		EGL_NONE, EGL_NONE
	};

	mSurface = eglCreateWindowSurface(mDisplay, mConfig, mFlingerSurface.get(), NULL);
	if (mSurface == EGL_NO_SURFACE) {
		switch (eglGetError()) {
		case EGL_BAD_MATCH:
			// Check window and EGLConfig attributes to determine
			// compatibility, or verify that the EGLConfig
			// supports rendering to a window
			break;
		case EGL_BAD_CONFIG:
			// Verify that provided EGLConfig is valid
			break;
		case EGL_BAD_NATIVE_WINDOW:
			// Verify that provided EGLNativeWindow is valid
			break;
		case EGL_BAD_ALLOC:
			// Not enough resources available.
			break;
		}

		mFlingerSurface.clear();
		mFlingerSurfaceControl.clear();
		eglTerminate(mDisplay);

		mSession->dispose();
		mSession.clear();
		return -1;
	}
	
	mContext = eglCreateContext(mDisplay, mConfig, EGL_NO_CONTEXT, contextAttributes);
	if (mContext == EGL_NO_CONTEXT) {
		EGLint error = eglGetError();
		if (error == EGL_BAD_CONFIG) {
			printf ("bad config when eglCreateContext");
			eglDestroySurface (mDisplay, mSurface);

			mFlingerSurface.clear();
			mFlingerSurfaceControl.clear();
			eglTerminate(mDisplay);

			mSession->dispose();
			mSession.clear();
			return -1;
		}
	}

	eglQuerySurface(mDisplay, mSurface, EGL_WIDTH, &mWidth);
	eglQuerySurface(mDisplay, mSurface, EGL_HEIGHT, &mHeight);
	
	printf ("EGL Width = %d, EGL Height = %d\n", mWidth, mHeight);

	if (eglMakeCurrent(mDisplay, mSurface, mSurface, mContext) == EGL_FALSE) {
		ALOGE ("MAKE CURRENT FAILED");
		eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext (mDisplay, mContext);
		eglDestroySurface (mDisplay, mSurface);

		mFlingerSurface.clear();
		mFlingerSurfaceControl.clear();
		eglTerminate(mDisplay);

		mSession->dispose();
		mSession.clear();
		return -1;
	}
	
	GLint vertexShader;
	GLint fragmentShader;
	GLuint programObject;
	GLint linked;

	//load vertex/fragment shaders
	if ((vertexShader = esLoadShader(GL_VERTEX_SHADER, VERTEX_SHADER3)) < 0) {
		printf ("load vertex shader failed\n");
		return -1;
	}
	
	if ((fragmentShader = esLoadShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER3)) < 0) {
		printf ("load fragment shader failed\n");
		return -1;	
	}

	//create the program object
	programObject = glCreateProgram();

	if (programObject == 0) {
		printf ("create program failed\n");
		return -1;
	}

	glAttachShader(programObject, vertexShader);
	glAttachShader(programObject, fragmentShader);

	//bind vPosition to attribute 0
	//glBindAttribLocation(programObject, 0, "vPosition");
	//glBindAttribLocation(programObject, 0, "a_position");
	//glBindAttribLocation(programObject, 1, "a_color");
	//glBindAttribLocation(mProgramObject, 0, "a_position");

	//link the program
	glLinkProgram(programObject);

	/*
	printf ("check bind with a_color@%d, a_position@%d\n",
		glGetAttribLocation(programObject, "a_color"), 
		glGetAttribLocation(programObject, "a_position"));
	*/

	//check the link status
	glGetProgramiv(programObject, GL_LINK_STATUS, &linked);

	if (!linked) {
		GLint infoLen = 0;

		glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen > 1) {
			char *infoLog = new char[infoLen];

			glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
			printf ("ERROR LINK PROGRAM:\n%s\n", infoLog);

			delete infoLog;
		}

		glDeleteProgram(programObject);
		return -1;
	}

	mProgramObject = programObject;	
	mInit = true;

	//load info from shader
	positionLoc = glGetAttribLocation(mProgramObject, "a_position");
	texCoordLoc = glGetAttribLocation(mProgramObject, "a_texCoord");
	samplerLoc = glGetUniformLocation(mProgramObject, "s_texture");
	offsetLoc = glGetUniformLocation(mProgramObject, "u_offset");
//	textureId = createMipMappedTexture2D();
//	textureId = loadDDS("uvtemplate.DDS");

	textureId = SOIL_load_OGL_texture(
				"uvtemplate.DDS",
				SOIL_LOAD_AUTO,
				SOIL_CREATE_NEW_ID,
				SOIL_FLAG_DDS_LOAD_DIRECT | SOIL_FLAG_MIPMAPS
			);
	printf ("textureId = %d\n", textureId);
	printf ("positionLoc = %d, texCoordLoc = %d\n", positionLoc, texCoordLoc);

	//glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	return 0;
}

void EGLTest::queryActiveUniforms() {
	GLint maxUniformLen;
	GLint numUniforms;
	char *uniformName;
	GLint index;

	glGetProgramiv(mProgramObject, GL_ACTIVE_UNIFORMS, &numUniforms);
	if (numUniforms == 0) {
		printf ("active 0 uniforms...\n");
		return;
	}

	glGetProgramiv(mProgramObject, GL_ACTIVE_UNIFORM_MAX_LENGTH, 
				&maxUniformLen);
	uniformName = new char[maxUniformLen];
	for (index = 0; index < numUniforms; index++) {
		GLint size;
		GLenum type;
		GLint location;

		//get the uniform info
		glGetActiveUniform(mProgramObject, index, maxUniformLen, NULL,
				&size, &type, uniformName);
		
		switch (type) {
		case GL_FLOAT:
			printf ("find %s with GL_FLOAT\n", uniformName);
			break;
		case GL_FLOAT_VEC2:
			printf ("find %s with GL_FLOAT_VEC2\n", uniformName);
			break;
		case GL_FLOAT_VEC3:
			printf ("find %s with GL_FLOAT_VEC3\n", uniformName);
			break;
		case GL_FLOAT_VEC4:
			printf ("find %s with GL_FLOAT_VEC4\n", uniformName);
			break;
		case GL_INT:
			printf ("find %s with GL_INT\n", uniformName);
			break;
		case GL_INT_VEC2:
			printf ("find %s with GL_INT_VEC2\n", uniformName);
		    break;
		case GL_INT_VEC3:
			printf ("find %s with GL_INT_VEC3\n", uniformName);
			break;
		case GL_INT_VEC4:
			printf ("find %s with GL_INT_VEC4\n", uniformName);
			break;
		case GL_BOOL:
			printf ("find %s with GL_BOOL\n", uniformName);
			break;
		case GL_BOOL_VEC2:
			printf ("find %s with GL_BOOL_VEC2\n", uniformName);
			break;
		case GL_BOOL_VEC3:
			printf ("find %s with GL_BOOL_VEC3\n", uniformName);
			break;
		case GL_BOOL_VEC4:
			printf ("find %s with GL_BOOL_VEC4\n", uniformName);
			break;
		case GL_FLOAT_MAT2:
			printf ("find %s with GL_FLOAT_MAT2\n", uniformName);
			break;
		case GL_FLOAT_MAT3:
			printf ("find %s with GL_FLOAT_MAT3\n", uniformName);
			break;
		case GL_FLOAT_MAT4:
			printf ("find %s with GL_FLOAT_MAT4\n", uniformName);
			break;
		case GL_SAMPLER_2D:
			printf ("find %s with GL_SAMPER_2D\n", uniformName);
			break;
		case GL_SAMPLER_CUBE:
			printf ("find %s with GL_SAMPLER_CUBE\n", uniformName);
			break;	
		default:
			printf ("find %s with TYPE_UNKNOWN\n", uniformName);
			break;
		}
	}

	delete[] uniformName;
}

GLuint EGLTest::loadDDS(const char * imagepath){

	unsigned char header[124];

	FILE *fp; 
 
	/* try to open the file */ 
	fp = fopen(imagepath, "rb"); 
	if (fp == NULL){
		printf("%s could not be opened. Are you in the right directory ? Don't forget to read the FAQ !\n", imagepath); getchar(); 
		return 0;
	}
   
	/* verify the type of file */ 
	char filecode[4]; 
	fread(filecode, 1, 4, fp); 
	if (strncmp(filecode, "DDS ", 4) != 0) { 
		fclose(fp); 
		return 0; 
	}
	
	/* get the surface desc */ 
	fread(&header, 124, 1, fp); 

	unsigned int height      = *(unsigned int*)&(header[8 ]);
	unsigned int width	     = *(unsigned int*)&(header[12]);
	unsigned int linearSize	 = *(unsigned int*)&(header[16]);
	unsigned int mipMapCount = *(unsigned int*)&(header[24]);
	unsigned int fourCC      = *(unsigned int*)&(header[80]);

 
	unsigned char * buffer;
	unsigned int bufsize;
	/* how big is it going to be including all mipmaps? */ 
	bufsize = mipMapCount > 1 ? linearSize * 2 : linearSize; 
	buffer = (unsigned char*)malloc(bufsize * sizeof(unsigned char)); 
	fread(buffer, 1, bufsize, fp); 
	/* close the file pointer */ 
	fclose(fp);

	unsigned int components  = (fourCC == FOURCC_DXT1) ? 3 : 4; 
	unsigned int format;
	switch(fourCC) 
	{ 
	case FOURCC_DXT1: 
	    printf ("read DXT1 format\n");	
		format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; 
		break; 
	case FOURCC_DXT3: 
		printf ("read DXT3 format\n");
		format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; 
		break; 
	case FOURCC_DXT5: 
		printf ("read DXT5 format\n");
		format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; 
		break; 
	default: 
		free(buffer); 
		return 0; 
	}

	// Create one OpenGL texture
	GLuint textureID;
	glGenTextures(1, &textureID);

	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, textureID);
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);	
	
	unsigned int blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16; 
	unsigned int offset = 0;

	/* load the mipmaps */ 
	for (unsigned int level = 0; level < mipMapCount && (width || height); ++level) 
	{ 
		unsigned int size = ((width+3)/4)*((height+3)/4)*blockSize; 
		glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height,  
			0, size, buffer + offset); 
	 
		offset += size; 
		width  /= 2; 
		height /= 2; 

		// Deal with Non-Power-Of-Two textures. This code is not included in the webpage to reduce clutter.
		if(width < 1) width = 1;
		if(height < 1) height = 1;

	} 

	free(buffer); 

	return textureID;
}

GLuint EGLTest::createSimpleTexture2D() {
	GLuint textureId;
	GLubyte pixels[4 * 4] = {
		128, 128, 128, 128,
		128, 128, 128, 128,
		0, 0, 255, 128,
		255, 255, 0, 128
	};	

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	return textureId;
}

GLuint EGLTest::createMipMappedTexture2D() {
	GLuint textureId;
	int width = 256, height = 256;
	int level;
	GLubyte *pixels;
	GLubyte *prevImage;
	GLubyte *newImage;

	pixels = genCheckImage(width, height, 8);
	if (pixels == NULL) {
		return 0;
	}

	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);

	//load mipmap 0
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height,
			0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	level = 1;
	prevImage = &pixels[0];
	while (width > 1 && height > 1) {
		int newWidth, newHeight;

		genMipMap2D(prevImage, &newImage, width, height,
				&newWidth, &newHeight);

		glTexImage2D(GL_TEXTURE_2D, level, GL_RGB, newWidth, newHeight,
				0, GL_RGB, GL_UNSIGNED_BYTE, newImage);

		free(prevImage);
		prevImage = newImage;
		level++;

		width = newWidth;
		height = newHeight;

	}
	
	free(newImage);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return textureId;
}

// generate an RGB8 checkerboard image
GLubyte *EGLTest::genCheckImage(int width, int height, int checkSize) {
	int x, y;
	GLubyte *pixels = (GLubyte*)malloc(width * height * 3);

	if (pixels == NULL) {
		return NULL;
	}

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			GLubyte rColor = 0;
			GLubyte bColor = 0;
			if ((x / checkSize) % 2 == 0) {
				rColor = 255 * ((y / checkSize) % 2);
				bColor = 255 * (1 - ((y / checkSize) % 2));
			} else {
				bColor = 255 * ((y / checkSize) % 2);
				rColor = 255 * (1 - ((y / checkSize) % 2));
			}
			
			pixels[(y * height + x) * 3] = rColor;
			pixels[(y * height + x) * 3 + 1] = 0;
			pixels[(y * height + x) * 3 + 2] = bColor;
		}
	}

	return pixels;
}

GLboolean EGLTest::genMipMap2D(GLubyte* src, GLubyte **dst, int srcWidth,
		int srcHeight, int *dstWidth, int *dstHeight) {
	int x, y;
	int texelSize = 3;
	*dstWidth = srcWidth / 2;
	if (*dstWidth <= 0) {
		*dstWidth = 1;
	}

	*dstHeight = srcHeight / 2;
	if (*dstHeight <= 0) {
		*dstHeight = 1;
	}

	*dst = (GLubyte*)malloc(sizeof(GLubyte) * texelSize * (*dstWidth) * (*dstHeight));
	if (*dst == NULL) {
		return GL_FALSE;
	}

	for (y = 0; y < *dstHeight; y++) {
		for (x = 0; x < *dstWidth; x++) {
			int srcIndex[4];
			float r = 0.0f;
			float g = 0.0f;
			float b = 0.0f;

			int sample;
			srcIndex[0] = 
				(((y * 2) * srcWidth) + (x * 2)) * texelSize;
			srcIndex[1] = 
				(((y * 2) * srcWidth) + (x * 2 + 1)) * texelSize;
			srcIndex[2] = 
				((((y * 2) + 1) * srcWidth) + (x * 2)) * texelSize;
			srcIndex[3] = 
				((((y * 2) + 1) * srcWidth) + (x * 2 + 1)) * texelSize;

			for (sample = 0; sample < 4; sample++) {
				r += src[srcIndex[sample]];
				g += src[srcIndex[sample] + 1];
				b += src[srcIndex[sample] + 2];
			}

			r /= 4.0;
			g /= 4.0;
			b /= 4.0;

			(*dst)[(y * (*dstWidth) + x) * texelSize] = (GLubyte)(r);
			(*dst)[(y * (*dstWidth) + x) * texelSize + 1] = (GLubyte)(g);
			(*dst)[(y * (*dstWidth) + x) * texelSize + 2] = (GLubyte)(b);
		}
	}

	return GL_TRUE;
}

void EGLTest::prepare() {
	//get matrix location
#if 0
	mvpLoc = glGetUniformLocation(mProgramObject, "u_mvpMatrix");
	
	//generate cube
	numIndices = esGenCube(1.0, &vertices, NULL, NULL, &indices);

	//init angle
	angle = 45.0f;
#endif
	static const GLfloat vVertices[]	= {
		-0.5f, 0.5f, 0.0f, 1.5f,
		-0.5f, -0.5f, 0.0f, 0.75f,
		0.5f, -0.5f, 0.0f, 0.75f,
		0.5f, 0.5f, 0.0f, 1.5f,
	};

	static const GLfloat guvBuffer[] = {
		0.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f
	};

	GLushort indices[] = {0, 1, 2, 0, 2, 3};

	/* setup VBO */
	GLuint vertexBuffer;
	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices), vVertices, GL_STATIC_DRAW);

	GLuint uvBuffer;
	glGenBuffers(1, &uvBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(guvBuffer), guvBuffer, GL_STATIC_DRAW);

	GLuint indexBuffer;
	glGenBuffers(1, &indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	
	mVertexBuffer = vertexBuffer;
	mUVBuffer = uvBuffer;
	mIndexBuffer = indexBuffer;

	glViewport(0, 0, mWidth, mHeight);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glUseProgram(mProgramObject);
}

void EGLTest::update(long frame) {
#if 0
	ESMatrix perspective;
	ESMatrix modelView;
	float aspect;

	angle += 0.5f;
	if (angle >= 360.0f) {
		angle -= 360.0f;
	}

	aspect = (GLfloat) mWidth / (GLfloat) mHeight;
	
	esMatrixLoadIdentity (&perspective);
	//60 FOV
	esPerspective(&perspective, 60.0f, aspect, 1.0f, 20.0f);
	
	esMatrixLoadIdentity (&modelView);

	esTranslate(&modelView, 0.0, 0.0, -2.0);
	esRotate(&modelView, angle, 1.0, 0.0, 1.0);

	esMatrixMultiply(&mvpMatrix, &modelView, &perspective);
#endif
}

void EGLTest::draw() {
	//ALOGI ("runTest");
	//if (!mInit) {
	//	printf ("egl env not ready\n");
	//	return;
	//}
#if 0
	GLfloat vVertices[] = {
		-0.5f, -0.75f, 0.0f,
		0.5f, -0.75f, 0.0f,
		0.5f, 0.25f, 0.0f,
		-0.5f, 0.25f, 0.0f,
	
		//0.75f, 0.25f, 0.0f,
		//1.75f, 0.25f, 0.0f,
		//1.75f, 1.25f, 0.0f,
		//0.75f, 1.25f, 0.0f,
	};

	GLfloat vVertices2[] = {
		-0.25f, -0.55f, 0.0f,
		0.25f, -0.55f, 0.0f,
		0.25f, -0.05f, 0.0f,
		-0.25f, -0.05f, 0.0f,
	};


	//set the viewport
	glViewport(0, 0, 640, 640);
	//clear the color buffer
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//use program object
	glUseProgram(mProgramObject);
	queryActiveUniforms();
	int numActiveAttribs;
	glGetProgramiv(mProgramObject, GL_ACTIVE_ATTRIBUTES, &numActiveAttribs);
	printf ("active attributes num is %d\n", numActiveAttribs);

	//this example's color is given by fragment shader
	//load the vertex data
	//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
	//glEnableVertexAttribArray(0);
	
	//GLES1.0
	//glEnableClientState(GL_VERTEX_ARRAY);
	//glVertexPointer(3, GL_FLOAT, 0, vVertices);

	//use constant vertex attribute
	GLfloat color[] = {0.0f, 1.0f, 0.0f, 1.0f};
	GLfloat color2[] = {1.0f, 0.0f, 0.0f, 1.0f};
	GLuint vboIds[2];
	GLuint indices[] = {0, 1, 2, 0, 2, 3};
	GLuint indices2[] = {0, 1, 2, 0, 2, 3};

	/*
	glGenBuffers(2, vboIds);
	
	glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
	glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), vVertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIds[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(GLuint), indices, GL_STATIC_DRAW);
	*/

	//GLuint vColorU = glGetUniformLocation(mProgramObject, "v_color_u");
	//glUniform4fv(vColorU, 1, color);

	//use gpu buffer
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
	glEnableVertexAttribArray(0);
	glVertexAttrib4fv(1, color);
	//glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, color);
	//glEnableVertexAttribArray(1);

	//array buffer will copy to gpu
	//glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDrawElements(GL_TRIANGLE_STRIP, 6, GL_UNSIGNED_INT, indices);

	glDepthFunc(GL_LEQUAL);
	//glDeleteBuffers(2, vboIds);
	glEnableVertexAttribArray(0);
	glVertexAttrib4fv(1, color2);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices2);
	glDrawElements(GL_TRIANGLE_STRIP, 6, GL_UNSIGNED_INT, indices2);
#endif

	//draw cube
#if 0
	glViewport (0, 0, mWidth, mHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	//glUseProgram(mProgramObject);
	glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), vertices);
	glEnableVertexAttribArray(0);

	glVertexAttrib4f(1, 0.0f, 1.0f, 0.0f, 1.0f);
	glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, (GLfloat *)&mvpMatrix.m[0][0]);

	glDrawElements(GL_LINE_LOOP, numIndices, GL_UNSIGNED_INT, indices);
#endif

	//sample 2d texture
#if 0
	GLfloat vVertices[] = { -0.5f, 0.5f, 0.0f,
		0.0f, 0.0f,				//texCoord
		-0.5f, -0.5f, 0.0f,
		0.0f, 1.0f,
		0.5f, -0.5f, 0.0f,
		1.0f, 1.0f,
		0.5f, 0.5f, 0.0f,
		1.0f, 0.0f,
	};

	GLushort indices[] = {0, 1, 2, 0, 2, 3};

	glViewport(0, 0, mWidth, mHeight);
	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vVertices);
	glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &vVertices[3]);

	glEnableVertexAttribArray(positionLoc);
	glEnableVertexAttribArray(texCoordLoc);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textureId);

	glUniform1i(samplerLoc, 0);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
#endif

	glEnableVertexAttribArray(positionLoc);
	glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
	glVertexAttribPointer(positionLoc, 4, GL_FLOAT,
			GL_FALSE, 0, (void *)0);


	glEnableVertexAttribArray(texCoordLoc);
	glBindBuffer(GL_ARRAY_BUFFER, mUVBuffer);
	glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT,
			GL_FALSE, 0, (void *)0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glUniform1i(samplerLoc, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glUniform1f(offsetLoc, -0.7f);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (void *)0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
	glUniform1f(offsetLoc, 0.7f);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (void *)0);

	glDeleteBuffers(1, &mVertexBuffer);
	glDeleteBuffers(1, &mUVBuffer);
	glDeleteBuffers(1, &mIndexBuffer);
}

void EGLTest::swap() {
	eglSwapBuffers(mDisplay, mSurface);
}

void EGLTest::eventLoop() {
	ALOGI ("runLoop");
	
	IPCThreadState::self()->joinThreadPool();
}

