#include <stdio.h>

#include "smacker.h"

int main (int argc, char *argv[])
{
    unsigned int w,h,f;
    float fps;
    unsigned char *image_data;
    smk s;

    if (argc != 2)
    {
        printf("Usage: %s file.smk\n",argv[0]);
        return -1;
    }

    s = smk_open(argv[1],SMK_MODE_DISK);
    if (s == NULL)
    {
        printf("Errors encountered opening %s, exiting.\n",argv[1]);
        return -1;
    }

/* print some info about the file */
    w = smk_info_video_w(s);
    h = smk_info_video_h(s);
    f = smk_info_f(s);
    fps = smk_info_fps(s);

    printf("Opened file %s\nWidth: %d\nHeight: %d\nFrames: %d\nFPS: %f\n",argv[1],w,h,f,fps);

/* Turn on decoding for palette, video, and audio track 0 */
    smk_enable_palette(s,1);
    smk_enable_video(s,1);
    smk_enable_audio(s,0,1);

    // Get a pointer to first frame

    smk_first(s);
    image_data = smk_get_video(s);
    printf(" -> Frame %d\n",smk_info_cur_frame(s));

    while ( smk_next(s) )
    {
        image_data = smk_get_video(s);
        printf(" -> Frame %d\n",smk_info_cur_frame(s));
        // Advance to next frame
    }

    smk_close(s);

    return 0;
}
