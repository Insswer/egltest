#define LOG_TAG "EGL_TEST"

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

using namespace android;

const EGLint EGLTest::attrsPrefer[MAX_PREFER_ATTR] = {
		EGL_SURFACE_TYPE,	EGL_WINDOW_BIT|EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
//		EGL_RECORDABLE_ANDROID, EGL_TRUE,
		EGL_FRAMEBUFFER_TARGET_ANDROID, EGL_TRUE,
		EGL_RED_SIZE,		8,
		EGL_GREEN_SIZE,		8,
		EGL_BLUE_SIZE,		8,
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
	if ((vertexShader = esLoadShader(GL_VERTEX_SHADER, VERTEX_SHADER1)) < 0) {
		printf ("load vertex shader failed\n");
		return -1;
	}
	
	if ((fragmentShader = esLoadShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER1)) < 0) {
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
	glBindAttribLocation(programObject, 0, "a_position");
	glBindAttribLocation(programObject, 1, "a_color");
	//glBindAttribLocation(mProgramObject, 0, "a_position");

	//link the program
	glLinkProgram(programObject);

	printf ("check bind with a_color@%d, a_position@%d\n",
		glGetAttribLocation(programObject, "a_color"), 
		glGetAttribLocation(programObject, "a_position"));

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

void EGLTest::draw() {
	ALOGI ("runTest");
	if (!mInit) {
		printf ("egl env not ready\n");
		return;
	}

	GLfloat vVertices[] = {
		0.0f, 0.5f, 0.0f,
	   -0.5f, -0.5f, 0.0f,
		0.5f, -0.5f, 0.0f
	};

	//set the viewport
	glViewport(0, 0, mWidth, mHeight);
	//clear the color buffer
	glClear (GL_COLOR_BUFFER_BIT);

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
	GLfloat color[4] = {0.0f, 1.0f, 0.0f, 1.0f};

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
	glEnableVertexAttribArray(0);
	glVertexAttrib4fv(1, color);

	//array buffer will copy to gpu
	glDrawArrays(GL_TRIANGLES, 0, 3);
	eglSwapBuffers(mDisplay, mSurface);
}

void EGLTest::eventLoop() {
	ALOGI ("runLoop");
	
	IPCThreadState::self()->joinThreadPool();
}

