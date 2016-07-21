LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	main.cpp \
	EGLTest.cpp \
	EGLUtil.cpp	\
	gimage/image_DXT.c \
	gimage/image_helper.c \
	gimage/SOIL.c \
	gimage/stb_image_aug.c

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

LOCAL_C_INCLUDES += bionic \
					gimage \
					prebuilts/ndk/10/sources/cxx-stl/llvm-libc++/libcxx/include
LIBS := prebuilts/ndk/10/sources/cxx-stl/llvm-libc++/libs/armeabi-v7a/libc++_static.a

LOCAL_LDFLAGS += "-Wl,--start-group" $(LIBS) "-Wl,--end-group"

LOCAL_MODULE:= egl_test
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
