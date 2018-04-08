
ROOTFS_NAME=rootfs

LINFLAGS = -I${SDK_PATH_TARGET}/usr/include/omap \
			-I${SDK_PATH_TARGET}/usr/include/drm \
			-I${SDK_PATH_TARGET}/usr/include/libdrm \
			-I${SDK_PATH_TARGET}/usr/include/gbm


SRC_OPENGL_FILE =  opengl.c \
				render1.c \
				render2.c \
				display-kms.c \
				esTransform.c 
	

LIB_CUBETEST_SET = -ldrm -ldrm_omap  -lgbm  -lEGL  -lGLESv2  -lpthread  -lm 

			
all:clean opengl


opengl: $(SRC_OPENGL_FILE)
		$(CC) $(CFLAGS) $(LINFLAGS) $(SRC_OPENGL_FILE) -o opengl $(LIB_CUBETEST_SET) \


clean:
	rm -rf opengl *.o *.so
