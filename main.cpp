
#include <iostream>
#include "opencv2/opencv.hpp"
#include <pthread.h>
#include <omp.h>

#define GLM_FORCE_RADIANS // need not for glm 9.6
#include "glm/glm.hpp"
#include "glm/ext.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>


Display*   glx_display;
GLXContext glx_context;
GLXPbuffer glx_pbuffer;

GLuint    tex_circle, tex_site;
const int width=640, height=480;

GLuint loadTex(const char* file)
{
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	cv::Mat img_rgb = cv::imread(file);
	if( img_rgb.empty() ){
		std::cout << "VisWithGL::loadTex(): load image \"" << file << "\" failed!\n";
		return 0;
	}
	cv::Mat img_rgba(img_rgb.rows, img_rgb.cols, CV_8UC4);
	for(int i=0; i<img_rgb.rows; ++i){
		for(int j=0; j<img_rgb.cols; ++j){
			memcpy(&img_rgba.at<cv::Vec4b>(i,j)[0], &img_rgb.at<cv::Vec3b>(i,j)[0], 3*sizeof(unsigned char));
			if(img_rgb.at<cv::Vec3b>(i,j)[0]<=45) img_rgba.at<cv::Vec4b>(i,j)[3] = 0;
			else img_rgba.at<cv::Vec4b>(i,j)[3] = 255;
		}
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_rgb.cols, img_rgb.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, img_rgba.data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glEnable(GL_TEXTURE_2D);

	return tex;
}

void* glinit(void* arg)
{
	printf("OpenGL init begin.\n");
	typedef GLXContext (*FUNC_glXCreateContextAttribsARB)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
	FUNC_glXCreateContextAttribsARB glXCreateContextAttribsARB =
		(FUNC_glXCreateContextAttribsARB)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
	if( !glXCreateContextAttribsARB ) printf("glXGetProcAddressARB() failed!\n");

	glx_display = XOpenDisplay( NULL ); // connect to X11 server
	if(!glx_display) printf("XOpenDisplay() failed!\n");

	static int visualAttribs[] = { None };
	int numberOfFramebufferConfigurations = 0;
	GLXFBConfig* fbConfigs = glXChooseFBConfig( glx_display, DefaultScreen(glx_display), visualAttribs, &numberOfFramebufferConfigurations );
	if(!fbConfigs) printf("glXChooseFBConfig() failed!\n");

	int context_attribs[] = { None };
	glx_context = glXCreateContextAttribsARB( glx_display, fbConfigs[0], 0, True, context_attribs);
	if( !glx_context ) printf("glXCreateContextAttribsARB failed!\n");

	int pbufferAttribs[] = {
		GLX_PBUFFER_WIDTH,  1,
		GLX_PBUFFER_HEIGHT, 1,
		None
	};
	glx_pbuffer = glXCreatePbuffer( glx_display, fbConfigs[0], pbufferAttribs );

	// clean up:
	XFree( fbConfigs );
	XSync( glx_display, False );

	// <draw> and <read> are both None and the OpenGL version supported by <ctx> is 3.0 or greater
	// see, https://www.opengl.org/registry/specs/ARB/glx_create_context.txt
	if ( !glXMakeContextCurrent( glx_display, glx_pbuffer, glx_pbuffer, glx_context ) )
		printf("glXMakeContextCurrent() failed!\n");
	
	
	printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
	printf("OpenGL Vendor: %s\n", glGetString(GL_VENDOR));

	// render to frambuffer object, there is no default osmesa_framebuffer!
	GLuint frame_buffer_s;
	glGenFramebuffers(1, &frame_buffer_s);
	glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_s);

	GLuint render_buff_rgba, render_buff_depth;
	glGenRenderbuffers(1, &render_buff_rgba);
	glBindRenderbuffer(GL_RENDERBUFFER, render_buff_rgba);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, width, height);
	glGenRenderbuffers(1, &render_buff_depth);
	glBindRenderbuffer(GL_RENDERBUFFER, render_buff_depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_RENDERBUFFER, render_buff_rgba);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
			GL_RENDERBUFFER, render_buff_depth);

	glViewport(0, 0, width, height);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	printf("OpenGL Error: 0x%x\n", glGetError());
	
	printf("OpenGL init finished.\n");
	
	tex_circle = loadTex("texture/circle.png");
	tex_site = loadTex("texture/site.png");
	
	return 0;
}

void* draw(void* arg)
{
	return 0;
}

void* process(void* arg)
{
	pthread_t thread;
	pthread_create(&thread, NULL, draw, NULL);
	pthread_join(thread, NULL);
	
	return 0;
}

int main()
{
	pthread_t thread;
	
	pthread_create(&thread, NULL, glinit, NULL);
	pthread_join(thread, NULL);
	
	pthread_create(&thread, NULL, process, NULL);
	pthread_join(thread, NULL);
	
	return 0;
}
