
#ifndef MINGJIAN_OPENGL_COMMON_H_
#define MINGJIAN_OPENGL_COMMON_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <gbm.h>
#include <sys/time.h>

#include <omap_drm.h>
#include <omap_drmif.h>

#include <drm_fourcc.h>
#include "esUtil.h"
#include "display-kms.h"

#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <xf86drmMode.h>

#define INPUT_FMT FOURCC_STR("YUYV")
#define INPUT_WIDTH  1280 
#define INPUT_HEIGHT 720

#define OUTPUT_FMT FOURCC_STR("NV12")
#define OUTPUT_WIDTH  1280 
#define OUTPUT_HEIGHT 720


typedef EGLImageKHR (eglCreateImageKHR_t)(EGLDisplay dpy, EGLContext ctx,
			EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);

typedef EGLBoolean (eglDestroyImageKHR_t)(EGLDisplay dpy, EGLImageKHR image);

typedef void (glEGLImageTargetTexture2DOES_t)(GLenum target, GLeglImageOES image);

struct gl_obj_t{
	EGLDisplay display;
	EGLConfig  config;

	EGLSurface surface;
	EGLSurface surface2;
	eglCreateImageKHR_t *eglCreateImageKHR;
	eglDestroyImageKHR_t *eglDestroyImageKHR;
	glEGLImageTargetTexture2DOES_t *glEGLImageTargetTexture2DOES;

};

struct gbm_obj_t{
	struct gbm_device *dev;
	struct gbm_surface *surface;
};

extern struct gl_obj_t gl;
extern struct gbm_obj_t gbm;
extern unsigned int   g_chnset; 
//extern struct gbm_bo *next_bo;

struct drm_fb * drm_fb_get_from_bo(struct gbm_bo *bo);


int init_context1();
int init_context2();

void close_context1();
void close_context2();

struct drm_fb * image_update1();
struct drm_fb * image_update2();


#endif /* MINGJIAN_OPENGL_COMMON_H_ */
