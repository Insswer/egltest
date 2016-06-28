#ifndef EGL_TEST_SHADER_H
#define EGL_TEST_SHADER_H

const char VERTEX_SHADER[] = 
	"attribute vec4 vPosition;				 \n"
	"void main()							 \n"
	"{										 \n"
	"	gl_Position = vPosition;			 \n"
	"}										 \n";

const char FRAGMENT_SHADER[] = 
	"precision mediump float;				 \n"
	"void main()							 \n"
	"{										 \n"
	"	gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); \n"
	"}										 \n";

#endif
