
#include <pthread.h>
#include "opengl.h"
#define DRM_OMAP_GEM_INFO		0x06


disp_hander_t g_disp1 = NULL;
disp_hander_t g_disp2 = NULL;

struct drm_fb {
	struct gbm_bo *bo;
	uint32_t fb_id;
	uint32_t size;
	uint64_t offset;		/* offset to mmap() */
	int dmabuf_fd;
	int index;
	unsigned char * buf;
};

struct drm_fb* drm_on_opengl[10];
//struct gbm_bo *next_bo = NULL;
struct gl_obj_t gl;
struct gbm_obj_t gbm;

const EGLint config_attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 1,
	EGL_GREEN_SIZE, 1,
	EGL_BLUE_SIZE, 1,
	EGL_ALPHA_SIZE, 0,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_NONE
};

static int init_gbm(void)
{
	gbm.dev = gbm_create_device(disp_get_fd(g_disp1));
	gbm.surface = gbm_surface_create(gbm.dev,
			disp_get_width(g_disp1), disp_get_height(g_disp1),
			GBM_FORMAT_XRGB8888,
			GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (!gbm.surface) {
		printf("failed to create gbm surface\n");
		return -1;
	}
	return 0;
}

static int init_gl(void)
{
	EGLint major, minor;
	EGLint n;
	
	gl.display = eglGetDisplay((int)gbm.dev);

	if (!eglInitialize(gl.display, &major, &minor)) {
		printf("failed to initialize\n");
		return -1;
	}
/*
	printf("Using display %p with EGL version %d.%d\n",
			gl.display, major, minor);

	printf("EGL Version \"%s\"\n", eglQueryString(gl.display, EGL_VERSION));
	printf("EGL Vendor \"%s\"\n", eglQueryString(gl.display, EGL_VENDOR));
	printf("EGL Extensions \"%s\"\n", eglQueryString(gl.display, EGL_EXTENSIONS));
*/
	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		printf("failed to bind api EGL_OPENGL_ES_API\n");
		return -1;
	}

	if (!(gl.eglCreateImageKHR = (eglCreateImageKHR_t *)eglGetProcAddress("eglCreateImageKHR"))) {
		printf("No eglCreateImageKHR?!");
		return -1;
	}

	if (!(gl.eglDestroyImageKHR = (eglDestroyImageKHR_t *)eglGetProcAddress("eglDestroyImageKHR"))) {
		printf("No eglDestroyImageKHR?!");
		return -1;
	}

	if (!(gl.glEGLImageTargetTexture2DOES = (glEGLImageTargetTexture2DOES_t *)eglGetProcAddress("glEGLImageTargetTexture2DOES"))) {
		printf("No glEGLImageTargetTexture2DOES?!");
		return -1;
	}
	
	if (!eglChooseConfig(gl.display, config_attribs, &gl.config, 1, &n) || n != 1) {
		printf("failed to choose config: %d\n", n);
		return -1;
	}
	
	gl.surface = eglCreateWindowSurface(gl.display, gl.config, gbm.surface, NULL);
	if (gl.surface == EGL_NO_SURFACE) {
		printf("----------b3\n");
		printf("failed to create egl surface\n");
		return -1;
	}
	
	return 0;
}

static void exit_gbm(void)
{
	gbm_surface_destroy(gbm.surface);
	gbm_device_destroy(gbm.dev);
	return;
}

static void exit_gl(void)
{
	eglMakeCurrent(gl.display, EGL_NO_SURFACE , EGL_NO_SURFACE , EGL_NO_CONTEXT);
	close_context1();
	close_context2();

	eglDestroySurface(gl.display, gl.surface);
	eglTerminate(gl.display);
	return;
}

static void
drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
	struct drm_fb *fb = data;
	struct gbm_device *gbm = gbm_bo_get_device(bo);
	munmap(fb->buf, fb->size);
	if (fb->fb_id)
		drmModeRmFB(disp_get_fd(g_disp1), fb->fb_id);

	free(fb);
}


struct drm_fb * drm_fb_get_from_bo(struct gbm_bo *bo)
{
	struct drm_fb *fb = gbm_bo_get_user_data(bo);
	uint32_t width, height, stride, handle;
	int ret;
	static int indexk = 0;

	if (fb)
		return fb;

	fb = calloc(1, sizeof *fb);
	fb->bo = bo;

	width = gbm_bo_get_width(bo);
	height = gbm_bo_get_height(bo);
	stride = gbm_bo_get_stride(bo);
	handle = gbm_bo_get_handle(bo).u32;
	
	struct drm_omap_gem_info req = {
			.handle = handle,
	};
	
	ret = drmCommandWriteRead(disp_get_fd(g_disp1), DRM_OMAP_GEM_INFO,
			&req, sizeof(req));
	if (ret) {
		return NULL;
	}

	fb->offset = req.offset;
	fb->size = req.size;
	fb->dmabuf_fd = gbm_bo_get_fd(bo);
	fb->buf = mmap(0, fb->size, PROT_READ | PROT_WRITE,
				MAP_SHARED, disp_get_fd(g_disp1), fb->offset);
	fb->index = indexk;
	drm_on_opengl[indexk++] = fb;

	ret = drmModeAddFB(disp_get_fd(g_disp1), width, height, 24, 32, stride, handle, &fb->fb_id);
	if (ret) {
		printf("failed to create fb: %s\n", strerror(errno));
		free(fb);
		return NULL;
	}

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

	return fb;
}


void cleanup_kmscube(void)
{
	exit_gl();
	exit_gbm();


	disp_close(g_disp1);
	disp_close(g_disp2);

	printf("Cleanup of GL, GBM and DRM completed\n");
	return;
}

void kms_signalhandler(int signum)
{
	switch(signum) {
	case SIGINT:
        case SIGTERM:
                /* Allow the pending page flip requests to be completed before
                 * the teardown sequence */
                sleep(1);
                printf("Handling signal number = %d\n", signum);
		cleanup_kmscube();
		break;
	default:
		printf("Unknown signal\n");
		break;
	}
	exit(1);
}


void * dispLoop1(void * parm){
	if(init_context1())
		return NULL;
		
	struct drm_fb *fb;
	int i = 0;
	while (1) {
		fb=image_update1();
		disp_post_buffer_noblock(g_disp1, fb->fb_id);
		disp_wait_for_complete(g_disp1);
		usleep(33000);
	}
}

void * dispLoop2(void * parm){
	if(init_context2())
		return NULL;
		
	struct drm_fb *fb;
	int i = 0;
	while (1) {
		fb=image_update2();
		disp_post_buffer_noblock(g_disp2, fb->fb_id);
		disp_wait_for_complete(g_disp2);
		usleep(33000);
	}
}

int main(int argc, char *argv[])
{
	int ret;
	pthread_t render1_tid;
	pthread_t render2_tid;


	signal(SIGINT, kms_signalhandler);
	signal(SIGTERM, kms_signalhandler);

	g_disp1 = disp_kms_open(32, "1280x720");
	if (!g_disp1) {
		printf("unable to create display");
		return 1;
	}
	
	g_disp2 = disp_kms_open(36, "1280x720");
	if (!g_disp2) {
		printf("unable to create display");
		return 1;
	}

	ret = init_gbm();
	if (ret) {
		printf("failed to initialize GBM\n");
		return ret;
	}

	ret = init_gl();
	if (ret) {
		printf("failed to initialize EGL\n");
		return ret;
	}

	pthread_create(&render1_tid, NULL, dispLoop1, NULL);
	sleep(3); 
	pthread_create(&render2_tid, NULL, dispLoop2, NULL); 
	while(1){
		sleep(1000);
	}
	

shutdown:
	cleanup_kmscube();
	printf("\n Exiting kmscube \n");
	return ret;
}
