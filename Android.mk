LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	main.cpp \
	EGLTest.cpp \
	EGLUtil.cpp

LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES

LOCAL_SHARED_LIBRARIES := \
	libandroid_runtime \
	libcutils \
	libutils \
	libbinder \
	libandroidfw \
	libui \
	libgui \
	liblog \
	libEGL \
	libGLESv1_CM \
	libGLESv2 

LOCAL_MODULE:= egl_test
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
