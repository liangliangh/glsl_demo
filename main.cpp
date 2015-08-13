
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

#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>
#include <GL/glxext.h>


Display*   glx_display;
GLXContext glx_context;
GLXPbuffer glx_pbuffer;

GLuint    tex_circle, tex_site;
const int width=640, height=480;


const char* glerrors(GLenum code){
	switch(code){
		case GL_NO_ERROR:
			return "GL_NO_ERROR";
		case GL_INVALID_ENUM:
			return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE:
			return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION:
			return "GL_INVALID_OPERATION";
		case GL_STACK_OVERFLOW:
			return "GL_STACK_OVERFLOW";
		case GL_STACK_UNDERFLOW:
			return "GL_STACK_UNDERFLOW";
		case GL_OUT_OF_MEMORY:
			return "GL_OUT_OF_MEMORY";
		default:
			return "undefined error code";
	}
}

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

//	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); // not core profile

//	glEnable(GL_TEXTURE_2D);

	printf("OpenGL Error (after loadtex): %s\n", glerrors(glGetError()));

	return tex;
}

void* glinit(void* arg)
{
	printf("OpenGL init begin.\n");
	typedef GLXContext (*FUNC_glXCreateContextAttribsARB)(
		Display* dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int* attrib_list );
	FUNC_glXCreateContextAttribsARB glXCreateContextAttribsARB =
		(FUNC_glXCreateContextAttribsARB)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
	if( !glXCreateContextAttribsARB ) printf("glXGetProcAddressARB() for \"glXCreateContextAttribsARB\" failed!\n");

	glx_display = XOpenDisplay( NULL ); // connect to X11 server
	if(!glx_display) printf("XOpenDisplay() failed!\n");

	static int visualAttribs[] = { None };
	int numberOfFbConfigs = 0;
	GLXFBConfig* fbConfigs = glXChooseFBConfig( glx_display, DefaultScreen(glx_display), visualAttribs, &numberOfFbConfigs );
	if(!fbConfigs) printf("glXChooseFBConfig() failed!\n");

	int context_attribs[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
		GLX_CONTEXT_MINOR_VERSION_ARB, 3,
		GLX_CONTEXT_FLAGS_ARB, 0, //GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
		None
	};
	glx_context = glXCreateContextAttribsARB( glx_display, fbConfigs[0], 0, True, context_attribs );
	if( !glx_context ) printf("glXCreateContextAttribsARB failed!\n");

	printf("glXIsDirect: %d\n", glXIsDirect(glx_display, glx_context));

	int pbufferAttribs[] = {
		GLX_PBUFFER_WIDTH,  1,
		GLX_PBUFFER_HEIGHT, 1,
		None
	};
	glx_pbuffer = glXCreatePbuffer( glx_display, fbConfigs[0], pbufferAttribs );

	// clean up:
	XFree( fbConfigs ); // Be sure to free the FBConfig list allocated by glXChooseFBConfig
	XSync( glx_display, False ); // Sync to ensure any errors generated are processed.

	// <draw> and <read> are both None and the OpenGL version supported by <ctx> is 3.0 or greater
	// see, https://www.opengl.org/registry/specs/ARB/glx_create_context.txt
	if ( !glXMakeContextCurrent(glx_display, glx_pbuffer, glx_pbuffer, glx_context) )
		printf("glXMakeContextCurrent() failed!\n");
	
	
	printf("  GL Version: %s\n", glGetString(GL_VERSION));
	printf("   GL Vendor: %s\n", glGetString(GL_VENDOR));
//	printf(" OpenGL Render: %s\n", glGetString(GL_RENDER));
	printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

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

	printf("OpenGL Error (after init): %s\n", glerrors(glGetError()));
	
	printf("OpenGL init finished.\n");
	
	tex_circle = loadTex("texture/circle.png");
	tex_site = loadTex("texture/site.png");
	
	glXMakeContextCurrent( glx_display, None, None, NULL ); // release context

	return 0;
}

void* glclean(void* arg)
{
	glXDestroyPbuffer(glx_display, glx_pbuffer);
	glXDestroyContext(glx_display, glx_context);
	XCloseDisplay(glx_display);
}

void* glsetuppipeline(void* arg)
{
	if( !glXMakeContextCurrent(glx_display, glx_pbuffer, glx_pbuffer, glx_context) )
		printf("glXMakeContextCurrent failed in glsetuppipeline().\n");

	//

	glXMakeContextCurrent( glx_display, None, None, NULL ); // release context

	return 0;
}

void* draw(void* arg)
{
	if( !glXMakeContextCurrent(glx_display, glx_pbuffer, glx_pbuffer, glx_context) )
		printf("glXMakeContextCurrent failed in draw().\n");

	for(int i=0; i<2; ++i){
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//		glBegin(GL_TRIANGLES);
//		glColor4f(1,1,0,0.2f);
//		glVertex3f(0,0,0);
//		glVertex3f(1,0,0);
//		glVertex3f(0,1,0);
//		glEnd();

		glFinish();
		cv::Mat img(height, width, CV_8UC3);
		glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, img.data);
		cv::imshow("result", img);
		cv::waitKey(0);

		printf("OpenGL Error (after draw): %s\n", glerrors(glGetError()));
	}

	glXMakeContextCurrent( glx_display, None, None, NULL ); // release context

	return 0;
}

void* process(void* arg)
{
	pthread_t thread;
	pthread_create(&thread, NULL, glsetuppipeline, NULL);
	pthread_join(thread, NULL);

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

	pthread_create(&thread, NULL, glclean, NULL);
	pthread_join(thread, NULL);

	return 0;
}

