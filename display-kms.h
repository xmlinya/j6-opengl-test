


#ifndef MINGJIAN_VIDEO_CAPTRUE_DISPLAY_MANAGER_H_
#define MINGJIAN_VIDEO_CAPTRUE_DISPLAY_MANAGER_H_


typedef void* disp_hander_t;



disp_hander_t disp_kms_open(int id, char* resolution);
void disp_close(disp_hander_t han);
						
unsigned int disp_get_width(disp_hander_t han);
unsigned int disp_get_height(disp_hander_t han);
int disp_get_fd(disp_hander_t han);

int disp_post_buffer(disp_hander_t han,  int fb_id);
int disp_post_buffer_noblock(disp_hander_t han,  int fb_id);
int disp_wait_for_complete(disp_hander_t han);


#endif 
