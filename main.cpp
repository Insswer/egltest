/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cutils/memory.h>

#include <utils/Log.h>

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

#include <glm/glm.hpp>

#include "EGLTest.h"

using namespace glm;
using namespace android;

int main(int argc, char** argv)
{
	EGLTest eglTest(0x80000000);
	
	glm::vec3 test(1.0f, 2.0f, 3.0f);

	if (eglTest.init() < 0) {
		printf ("init egl test failed\n");
		return -1;
	}

	long frame = 0L;
	eglTest.prepare();
	printf ("prepare done\n");	
	while (1) {
		//poll event async
		
		eglTest.update(frame);
		eglTest.draw();	
		//commit
		eglTest.swap();

		//for 30fps
		usleep(26000);
		frame++;
		//eglTest.eventLoop();
	}
    return 0;
}
