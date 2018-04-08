
#include "opengl.h"
#include "opengl-data2.h"

const EGLint context_attribs2[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
};

struct {
	EGLContext context;
	GLuint program;
	GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix, uniform_texture;
	GLuint texture_name;
	GLuint vertex_shader, fragment_shader;
} gl_rander2;

int init_program2(){
	GLint ret;
	gl_rander2.vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(gl_rander2.vertex_shader, 1, &vertex_shader_source2, NULL);
	glCompileShader(gl_rander2.vertex_shader);
	glGetShaderiv(gl_rander2.vertex_shader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		char *log;
		printf("vertex shader compilation failed!: bbc\n");
		glGetShaderiv(gl_rander2.vertex_shader, GL_INFO_LOG_LENGTH, &ret);
		if (ret > 1) {
			log = malloc(ret);
			glGetShaderInfoLog(gl_rander2.vertex_shader, ret, NULL, log);
			printf("%s", log);
		}
		return -1;
	}

	gl_rander2.fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(gl_rander2.fragment_shader, 1, &fragment_shader_source2, NULL);
	glCompileShader(gl_rander2.fragment_shader);
	glGetShaderiv(gl_rander2.fragment_shader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		char *log;
		printf("fragment shader compilation failed!:\n");
		glGetShaderiv(gl_rander2.fragment_shader, GL_INFO_LOG_LENGTH, &ret);
		if (ret > 1) {
			log = malloc(ret);
			glGetShaderInfoLog(gl_rander2.fragment_shader, ret, NULL, log);
			printf("%s", log);
		}
		return -1;
	}

	gl_rander2.program = glCreateProgram();
	glAttachShader(gl_rander2.program, gl_rander2.vertex_shader);
	glAttachShader(gl_rander2.program, gl_rander2.fragment_shader);
	glBindAttribLocation(gl_rander2.program, 0, "in_position");
	glBindAttribLocation(gl_rander2.program, 1, "in_normal");
	glBindAttribLocation(gl_rander2.program, 2, "in_color");

	glLinkProgram(gl_rander2.program);
	glGetProgramiv(gl_rander2.program, GL_LINK_STATUS, &ret);
	if (!ret) {
		char *log;
		printf("program linking failed!:\n");
		glGetProgramiv(gl_rander2.program, GL_INFO_LOG_LENGTH, &ret);
		if (ret > 1) {
			log = malloc(ret);
			glGetProgramInfoLog(gl_rander2.program, ret, NULL, log);
			printf("%s", log);
		}
		return -1;
	}
}

int init_context2(){
	GLint ret;
	gl_rander2.context = eglCreateContext(gl.display, gl.config,
			EGL_NO_CONTEXT, context_attribs2);
	if (gl_rander2.context == NULL) {
		printf("failed to create context\n");
		return -1;
	}
	eglMakeCurrent(gl.display, gl.surface, gl.surface, gl_rander2.context);
	init_program2();
	glUseProgram(gl_rander2.program);
	gl_rander2.modelviewmatrix = glGetUniformLocation(gl_rander2.program, "modelviewMatrix");
	gl_rander2.modelviewprojectionmatrix = glGetUniformLocation(gl_rander2.program, "modelviewprojectionMatrix");
	gl_rander2.normalmatrix = glGetUniformLocation(gl_rander2.program, "normalMatrix");
	gl_rander2.uniform_texture = glGetUniformLocation(gl_rander2.program, "uniform_texture");
	glViewport(0, 0, OUTPUT_WIDTH, OUTPUT_HEIGHT);
	
	glEnable(GL_CULL_FACE);
	
	static int positionsoffset = 0;
	static int colorsoffset = sizeof(vVertices2);
	static int normalsoffset = sizeof(vVertices2) + sizeof(vColors2);
	
	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices2) + sizeof(vColors2) + sizeof(vNormals2), 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, positionsoffset, sizeof(vVertices2), &vVertices2[0]);
	glBufferSubData(GL_ARRAY_BUFFER, colorsoffset, sizeof(vColors2), &vColors2[0]);
	glBufferSubData(GL_ARRAY_BUFFER, normalsoffset, sizeof(vNormals2), &vNormals2[0]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)positionsoffset);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)normalsoffset);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)colorsoffset);
	glEnableVertexAttribArray(2);
	
	return 0;
}


void draw2(uint32_t i)
{
	int face, tex_name;
	ESMatrix modelview;
	
	eglMakeCurrent(gl.display, gl.surface, gl.surface, gl_rander2.context);

	/* clear the color buffer */
	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	esMatrixLoadIdentity(&modelview);
	esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
	esRotate(&modelview, 45.0f + (0.25f * i), 1.0f, 0.0f, 0.0f);
	esRotate(&modelview, 45.0f - (0.5f * i), 0.0f, 1.0f, 0.0f);
	esRotate(&modelview, 10.0f + (0.15f * i), 0.0f, 0.0f, 1.0f);

	GLfloat aspect = (GLfloat)(720) / (GLfloat)(1280);

	ESMatrix projection;
	esMatrixLoadIdentity(&projection);
	esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f, 10.0f);

	ESMatrix modelviewprojection;
	esMatrixLoadIdentity(&modelviewprojection);
	esMatrixMultiply(&modelviewprojection, &modelview, &projection);

	float normal[9];
	normal[0] = modelview.m[0][0];
	normal[1] = modelview.m[0][1];
	normal[2] = modelview.m[0][2];
	normal[3] = modelview.m[1][0];
	normal[4] = modelview.m[1][1];
	normal[5] = modelview.m[1][2];
	normal[6] = modelview.m[2][0];
	normal[7] = modelview.m[2][1];
	normal[8] = modelview.m[2][2];

	glUniformMatrix4fv(gl_rander2.modelviewmatrix, 1, GL_FALSE, &modelview.m[0][0]);
	glUniformMatrix4fv(gl_rander2.modelviewprojectionmatrix, 1, GL_FALSE, &modelviewprojection.m[0][0]);
	glUniformMatrix3fv(gl_rander2.normalmatrix, 1, GL_FALSE, normal);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);
}



struct drm_fb * image_update2()
{
	struct gbm_bo *cur_bo = NULL;
	static struct gbm_bo *next_boa = NULL;
	static int i=0;

	if(next_boa){
		printf("render2 release\n");
		gbm_surface_release_buffer(gbm.surface, next_boa);
	}

	draw2(i++);

	eglSwapBuffers(gl.display, gl.surface);
	printf("render2 gbm_surface_lock enter i=%d\n", i);
	cur_bo = gbm_surface_lock_front_buffer(gbm.surface);
	printf("render2 gbm_surface_lock complete i=%d\n", i);
	next_boa = cur_bo;
	return drm_fb_get_from_bo(cur_bo);
}

void close_context2()
{
	glDeleteProgram(gl_rander2.program);
	glDeleteShader(gl_rander2.fragment_shader);
	glDeleteShader(gl_rander2.vertex_shader);
	eglDestroyContext(gl.display, gl_rander2.context);
}
