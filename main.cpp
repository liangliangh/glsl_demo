
#include <iostream>
#include "opencv2/opencv.hpp"
#include <pthread.h>
#include <omp.h>

#define GLM_FORCE_RADIANS // need not for glm 9.6
#include "glm/glm.hpp"
#include "glm/ext.hpp"

#define GL_GLEXT_PROTOTYPES // used in <GL/glcorearb.h>
//#include <GL/gl.h>
//#include <GL/glext.h>
#include <GL/glcorearb.h> // use only the core profile
#define __gl_h_ // do not include <GL/gl.h>

#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>
#include <GL/glxext.h>


Display*   glx_display;
GLXContext glx_context;
GLXPbuffer glx_pbuffer;

GLuint    tex_circle, tex_site;
const int width=640, height=480;


const char* glerrorstring(GLenum code){
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

//	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); // not core profile
//	glEnable(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);

	printf("OpenGL Error (after loadtex): %s\n", glerrorstring(glGetError()));

	return tex;
}

void* glinit(void* arg)
{
//	printf("OpenGL init begin.\n");
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
		//GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
		//GLX_CONTEXT_MINOR_VERSION_ARB, 3,
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

	glDrawBuffer(GL_COLOR_ATTACHMENT0); // draw index 0: "GL_COLOR_ATTACHMENT0", in fragment shader "layout(location=0)"

	glViewport(0, 0, width, height);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	printf("OpenGL Error (after init): %s\n", glerrorstring(glGetError()));
	
//	printf("OpenGL init finished.\n");
	
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


char* textFileRead(const char* fn)
{
    FILE *fp = fopen(fn,"rt");
    if(!fp){
    	printf("textFileRead(\"%s\") failed.\n", fn);
    	return NULL;
    }

	fseek(fp, 0, SEEK_END);
	int count = ftell(fp);
	rewind(fp);

	char *content = NULL;
	if (count > 0) {
		content = (char *)malloc(sizeof(char) * (count+1));
		count = fread(content,sizeof(char),count,fp);
		content[count] = '\0';
	}
	fclose(fp);

    return content;
}

// http://www.lighthouse3d.com/tutorials/glsl-tutorial/
void printShaderInfoLog(GLuint obj)
{
	GLint ret; glGetShaderiv(obj, GL_COMPILE_STATUS, &ret);
	if(ret==GL_TRUE) printf("compile shader %d success.\n", obj);
	else 			 printf("compile shader %d failed.\n", obj);

    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;
    glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
    if (infologLength > 0) {
        infoLog = (char*)malloc(infologLength);
        glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
        printf("compile log:\n%s\n",infoLog);
        free(infoLog);
    }
}

void printProgramInfoLog(GLuint obj)
{
	GLint ret; glGetProgramiv(obj, GL_LINK_STATUS, &ret);
	if(ret==GL_TRUE) printf("link program %d success.\n", obj);
	else 			 printf("link program %d failed.\n", obj);

    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;
    glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
    if (infologLength > 0) {
        infoLog = (char *)malloc(infologLength);
        glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
        printf("link log:\n%s\n",infoLog);
        free(infoLog);
    }
}


GLuint verts, frags, prog;
GLint uni_mat_transformation, uni_model_scale, uni_model_trans, uni_tri_color, uni_tex_cicle;

void* glsetuppipeline(void* arg)
{
	if( !glXMakeContextCurrent(glx_display, glx_pbuffer, glx_pbuffer, glx_context) )
		printf("glXMakeContextCurrent failed in glsetuppipeline().\n");

	verts = glCreateShader(GL_VERTEX_SHADER);
	frags = glCreateShader(GL_FRAGMENT_SHADER);
	char* vss = textFileRead("vertex.glsl");
	char* fss = textFileRead("fragment.glsl");
	glShaderSource(verts, 1, &vss, NULL);
	glShaderSource(frags, 1, &fss, NULL);
	free(vss); vss=0;
	free(fss); fss=0;

	glCompileShader(verts); printShaderInfoLog(verts);
	glCompileShader(frags); printShaderInfoLog(frags);

	prog = glCreateProgram();
	glAttachShader(prog, verts);
	glAttachShader(prog, frags);

	glLinkProgram(prog); printProgramInfoLog(prog);
	glUseProgram(prog);

	uni_mat_transformation = glGetUniformLocation(prog, "mat_transformation");
	uni_model_scale = glGetUniformLocation(prog, "model_scale");
	uni_model_trans = glGetUniformLocation(prog, "model_trans");
	uni_tri_color = glGetUniformLocation(prog, "tri_color");
	uni_tex_cicle = glGetUniformLocation(prog, "tex_cicle");

	printf("OpenGL Error (after set up pipeline): %s\n", glerrorstring(glGetError()));

	glXMakeContextCurrent( glx_display, None, None, NULL ); // release context

	return 0;
}

void* glcleanpipeline(void* arg)
{
	if( !glXMakeContextCurrent(glx_display, glx_pbuffer, glx_pbuffer, glx_context) )
		printf("glXMakeContextCurrent failed in glcleanpipeline().\n");

	glUseProgram(0);
	glDeleteProgram(prog);
	glDeleteShader(verts);
	glDeleteShader(frags);

	glXMakeContextCurrent( glx_display, None, None, NULL ); // release context

	return 0;
}

void* draw(void* arg)
{
	if( !glXMakeContextCurrent(glx_display, glx_pbuffer, glx_pbuffer, glx_context) )
		printf("glXMakeContextCurrent failed in draw().\n");

	GLuint vert_pos_buffer, vert_texcoord_buffer;
	glGenBuffers(1, &vert_pos_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vert_pos_buffer);
	float pos[] = {0.5f,0.5f,0, -0.5f,0.5f,0, -0.5f,-0.5f,0, 0.5f,-0.5f,0, };
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STATIC_DRAW);

	glGenBuffers(1, &vert_texcoord_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vert_texcoord_buffer);
	float texcoor[] = {1,1, 0,1, 0,0, 1,0, };
	glBufferData(GL_ARRAY_BUFFER, sizeof(texcoor), texcoor, GL_STATIC_DRAW);

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vert_pos_buffer);
	glVertexAttribPointer(0, 3, GL_FLOAT, 0, 0, 0); // index 0(see the shader code): pos

	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, vert_texcoord_buffer);
	glVertexAttribPointer(1, 2, GL_FLOAT, 0, 0, 0); // index 1(see the shader code): tex coord

	GLuint ele_idx_buffer;
	glGenBuffers(1, &ele_idx_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ele_idx_buffer);
	int index[] = {0,1,2, 2,3,0 };
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index), index, GL_STATIC_DRAW);

{
	const int dsize = 640*480*4;

	unsigned char pixel_rgb[dsize];
	unsigned char pixel_rgb2[dsize];

	float pixel_f[dsize];
	float pixel_f2[dsize];

	for(int i=0; i<sizeof(pixel_rgb); ++i)
		pixel_rgb[i] = (unsigned char)(i%255);
	printf("\n\ndata copy:\n");
	for(int i=0; i<4; ++i){
		printf("\n");
		double t;
		t = omp_get_wtime();
		memcpy(pixel_rgb2, pixel_rgb, sizeof(pixel_rgb));
		printf("memcpy(): %.3f ms\n", (omp_get_wtime()-t)*100);

		t = omp_get_wtime();
		for(int i=0; i<sizeof(pixel_rgb); ++i)
			pixel_rgb2[i] = pixel_rgb[i];
		printf("for copy: %.3f ms\n", (omp_get_wtime()-t)*100);

		t = omp_get_wtime();
		for(int i=0; i<dsize; ++i)
			pixel_f[i] = pixel_rgb[i] / 255.0f;
		printf("to float: %.3f ms\n", (omp_get_wtime()-t)*100);

		t = omp_get_wtime();
		for(int i=0; i<dsize; ++i)
			pixel_rgb[i] = (unsigned char)(pixel_f[i] * 255.0f);
		printf("to uchar: %.3f ms\n", (omp_get_wtime()-t)*100);
	}
}
{
	GLuint buffer;
	glGenBuffers(1, &buffer);
	glBindBuffer(GL_TEXTURE_BUFFER, buffer);

	double t = omp_get_wtime();
	glBufferData(GL_TEXTURE_BUFFER, width*height*sizeof(float), 0, GL_DYNAMIC_DRAW);
	printf("\n\nglBufferData(GL_TEXTURE_BUFFER): %.3f\n", (omp_get_wtime()-t)*1000);
	char bdata[640*480*sizeof(float)] = {'\0'};
	printf("\nglBufferData():\n");
	for(int i=0; i<5; ++i){
		t = omp_get_wtime();
		glBufferData(GL_TEXTURE_BUFFER, width*height*sizeof(float), bdata, GL_DYNAMIC_DRAW);
		printf("time ms: %.3f\n", (omp_get_wtime()-t)*1000);
	}
	printf("\nglBufferSubData():\n");
	for(int i=0; i<5; ++i){
		t = omp_get_wtime();
		glBufferSubData(GL_TEXTURE_BUFFER, 0, width*height*sizeof(float), bdata);
		printf("time ms: %.3f\n", (omp_get_wtime()-t)*1000);
	}
	printf("\nglMapBuffer() write memcpy():\n");
	for(int i=0; i<5; ++i){
		t = omp_get_wtime();
		void* p = glMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
		memcpy(p, bdata, width*height*sizeof(float));
		glUnmapBuffer(GL_TEXTURE_BUFFER);
		printf("time ms: %.3f\n", (omp_get_wtime()-t)*1000);
	}
	printf("\nglMapBuffer() write for:\n");
	for(int i=0; i<5; ++i){
		t = omp_get_wtime();
		char* p = (char*)glMapBuffer(GL_TEXTURE_BUFFER, GL_WRITE_ONLY);
		for(int j=0; j<width*height*sizeof(float); ++j) p[j] = bdata[j];
		glUnmapBuffer(GL_TEXTURE_BUFFER);
		printf("time ms: %.3f\n", (omp_get_wtime()-t)*1000);
	}
	printf("\nglGetBufferSubData():\n");
	for(int i=0; i<5; ++i){
		t = omp_get_wtime();
		glGetBufferSubData(GL_TEXTURE_BUFFER, 0, width*height*sizeof(float), bdata);
		printf("time ms: %.3f\n", (omp_get_wtime()-t)*1000);
	}
	printf("\nglMapBuffer() read memcpy():\n");
	for(int i=0; i<5; ++i){
		t = omp_get_wtime();
		void* p = glMapBuffer(GL_TEXTURE_BUFFER, GL_READ_ONLY);
		memcpy(bdata, p, width*height*sizeof(float));
		glUnmapBuffer(GL_TEXTURE_BUFFER);
		printf("time ms: %.3f\n", (omp_get_wtime()-t)*1000);
	}
	printf("\nglMapBuffer() read for:\n");
	for(int i=0; i<5; ++i){
		t = omp_get_wtime();
		char* p = (char*)glMapBuffer(GL_TEXTURE_BUFFER, GL_READ_ONLY);
		for(int j=0; j<width*height*sizeof(float); ++j) bdata[j] = p[j];
		glUnmapBuffer(GL_TEXTURE_BUFFER);
		printf("time ms: %.3f\n", (omp_get_wtime()-t)*1000);
	}
	
}

{
	GLuint buffer;
	glGenTextures(1, &buffer);
	glBindTexture(GL_TEXTURE_RECTANGLE, buffer);
	GLuint pupack, ppack;
	glGenBuffers(1, &pupack);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pupack);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, width*height*sizeof(float), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	glGenBuffers(1, &ppack);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, ppack);
	glBufferData(GL_PIXEL_PACK_BUFFER, width*height*sizeof(float), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	double t = omp_get_wtime();
	glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	printf("\n\nglTexImage2D(GL_TEXTURE_RECTANGLE): %.3f\n", (omp_get_wtime()-t)*1000);
	char bdata[640*480*sizeof(float)] = {'\0'};
	printf("\nglTexImage2D():\n");
	for(int i=0; i<5; ++i){
		t = omp_get_wtime();
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, bdata);
		printf("time ms: %.3f\n", (omp_get_wtime()-t)*1000);
	}
	printf("\nglTexSubImage2D():\n");
	for(int i=0; i<5; ++i){
		t = omp_get_wtime();
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glTexSubImage2D(GL_TEXTURE_RECTANGLE,0, 0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, bdata);
		printf("time ms: %.3f\n", (omp_get_wtime()-t)*1000);
	}
	printf("\nglTexSubImage2D() map from pixel unpack buffer:\n");
	for(int i=0; i<5; ++i){
		t = omp_get_wtime();
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pupack);
		char* p = (char*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
		memcpy(p, bdata, width*height*sizeof(float));
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		glTexSubImage2D(GL_TEXTURE_RECTANGLE,0, 0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		printf("time ms: %.3f\n", (omp_get_wtime()-t)*1000);
	}
	printf("\nglGetTexImage():\n");
	for(int i=0; i<5; ++i){
		t = omp_get_wtime();
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glGetTexImage(GL_TEXTURE_RECTANGLE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, bdata);
		printf("time ms: %.3f\n", (omp_get_wtime()-t)*1000);
	}
	printf("\nglGetTexImage() map to pixel pack buffer:\n");
	for(int i=0; i<5; ++i){
		t = omp_get_wtime();
		glBindBuffer(GL_PIXEL_PACK_BUFFER, ppack);
		glGetTexImage(GL_TEXTURE_RECTANGLE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		char* p = (char*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		memcpy(bdata, p, width*height*sizeof(float));
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		printf("time ms: %.3f\n", (omp_get_wtime()-t)*1000);
	}
	
}


	for(int i=0; i<1; ++i){
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//		glBegin(GL_TRIANGLES);
//		glColor4f(1,1,0,0.2f);
//		glVertex3f(0,0,0);
//		glVertex3f(1,0,0);
//		glVertex3f(0,1,0);
//		glEnd();

		glActiveTexture(GL_TEXTURE0 + 1); // binding=1
		glBindTexture(GL_TEXTURE_2D, tex_circle);
		glUniform1i(uni_tex_cicle, 1);

		glUniform4f(uni_tri_color, 1,0,0,0.5f);
		glm::mat4 trans = glm::perspective(glm::radians(40.0f), float(width)/height, 1.0f, 1000.0f)
			* glm::lookAt(glm::vec3(0,-2, 1), glm::vec3(0), glm::vec3(0,1,0));
//		glm::mat4 trans = glm::translate(glm::vec3(0.0f,0.0f, 0));
		glUniformMatrix4fv(uni_mat_transformation, 1, 0, &trans[0][0]);

		glBindVertexArray(vao);
		glDrawElements(GL_TRIANGLES, sizeof(index)/sizeof(index[0]), GL_UNSIGNED_INT, NULL);
//		glDrawArrays(GL_TRIANGLES, 0, 3);

		printf("OpenGL Error (after draw): %s\n", glerrorstring(glGetError()));

//		glFinish();
		cv::Mat img(height, width, CV_8UC3);
		glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, img.data);
		cv::Mat img2;
		cv::flip(img, img2, 0);
		cv::imshow("result", img2);
		cv::waitKey(500);
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

	pthread_create(&thread, NULL, glcleanpipeline, NULL);
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

