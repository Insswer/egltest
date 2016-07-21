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
	"	gl_Position = u_mvpMatrix * a_position;	\n"
	"}									\n";

const char VERTEX_SHADER3[] = 
	"uniform float u_offset;			\n"
	"attribute vec4 a_position;			\n"
	"attribute vec2 a_texCoord;			\n"
	"varying vec2 v_texCoord;			\n"
	"void main()						\n"
	"{									\n"
	"	gl_Position = vec4(a_position.x + u_offset, a_position.y, a_position.z, a_position.w);		\n"
	"	v_texCoord = a_texCoord;		\n"
	"}									\n";

const char FRAGMENT_SHADER3[] = 
	"precision mediump float;			\n"
	"uniform sampler2D s_texture;		\n"
	"varying vec2 v_texCoord;			\n"
	"void main()						\n"
	"{									\n"
	"	gl_FragColor = texture2D(s_texture, v_texCoord);\n"
	"}									\n";

#endif
