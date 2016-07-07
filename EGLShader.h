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


const char VERTEX_SHADER1[] =
	"attribute vec4 a_position;				\n"
	"attribute vec4 a_color;				\n"
	"varying mediump vec4 v_color;					\n"
	"void main()							\n"
	"{										\n"
	"		v_color = a_color;			\n"
	"		gl_Position = a_position;		\n"
	"}";

const char FRAGMENT_SHADER1[] =		
	"varying mediump vec4 v_color;					\n"
	"void main()							\n"
	"{										\n"
	"		gl_FragColor = v_color;			\n"
	"}";

const char VERTEX_SHADER2[] = 
	"uniform mat4 u_mvpMatrix;			\n"
	"attribute vec4 a_position;			\n"
	"attribute vec4 a_color;			\n"
	"varying mediump vec4 v_color;		\n"
	"void main()						\n"
	"{									\n"
	"	v_color = a_color;				\n"
	"	gl_Position = u_mvpMatrix * a_position;	\n";

#endif
