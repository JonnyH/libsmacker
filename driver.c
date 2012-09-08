#include <stdio.h>

#include "smacker.h"

void dump_bmp(unsigned char *pal, unsigned char *image_data, unsigned int w, unsigned int h, unsigned int framenum)
{
    int i,j;
    FILE *fp;
    char filename[128];
    unsigned int temp;
    sprintf(filename,"bmp/out_%04u.bmp",framenum);
    fp = fopen(filename, "wb");
    fwrite("BM",2,1,fp);
    temp = 1078 + (w * h);
    fwrite(&temp,4,1,fp);
    temp = 0;
    fwrite(&temp,4,1,fp);
    temp = 1078;
    fwrite(&temp,4,1,fp);
    temp = 40;
    fwrite(&temp,4,1,fp);
    fwrite(&w,4,1,fp);
    fwrite(&h,4,1,fp);
    temp = 1;
    fwrite(&temp,2,1,fp);
    temp = 8;
    fwrite(&temp,4,1,fp);
    temp = 0;
    fwrite(&temp,2,1,fp);
    temp = w * h;
    fwrite(&temp,4,1,fp);
    temp = 0;
    fwrite(&temp,4,1,fp);
    fwrite(&temp,4,1,fp);
    temp = 256;
    fwrite(&temp,4,1,fp);
    temp = 256;
    fwrite(&temp,4,1,fp);
    temp = 0;
    for ( i = 0; i < 256; i ++)
    {
        fwrite(&pal[(i * 3) + 2],1,1,fp);
        fwrite(&pal[(i * 3) + 1],1,1,fp);
        fwrite(&pal[(i * 3)],1,1,fp);
        fwrite(&temp,1,1,fp);
    }

    for ( i = h - 1; i >= 0; i --)
    {
        fwrite(&image_data[i * w],w,1,fp);
    }

    fclose(fp);
}

int main (int argc, char *argv[])
{
    unsigned int w,h,f;
    float fps;
    smk s;

    char filename[128];

    FILE *fpo[7] = {NULL};

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

    int i;
    for (i=0; i < 7; i ++)
    {
        unsigned char a_c = smk_info_audio_channels(s,i);
        unsigned char a_d = smk_info_audio_bitdepth(s,i);
        unsigned int a_r = smk_info_audio_rate(s,i);
        printf("Audio track %d: %u bits, %u channels, %uhz\n",i,a_d,a_c,a_r);
    }

/* Turn on decoding for palette, video, and audio track 0 */
    smk_enable_palette(s,1);
    smk_enable_video(s,1);

    for (i = 0; i < 7; i ++)
    {
        if (smk_info_audio_tracks(s) & (1 << i))
        {
            smk_enable_audio(s,i,1);
            sprintf(filename,"out_%01d.raw",i);
            fpo[i] = fopen(filename, "wb");
        } else {
            fpo[i] = NULL;
        }
    }

    // Get a pointer to first frame

    smk_first(s);

    dump_bmp(smk_get_palette(s),smk_get_video(s),w,h,smk_info_cur_frame(s));

    for (i = 0; i < 7; i++)
    {
        if (fpo[i] != NULL)
        {
            fwrite(smk_get_audio(s,i),smk_get_audio_size(s,i),1,fpo[i]);
        }
    }
    printf(" -> Frame %d\n",smk_info_cur_frame(s));


    while ( smk_next(s) )
    {

    dump_bmp(smk_get_palette(s),smk_get_video(s),w,h,smk_info_cur_frame(s));

    for (i = 0; i < 7; i++)
    {
        if (fpo[i] != NULL)
        {
            fwrite(smk_get_audio(s,i),smk_get_audio_size(s,i),1,fpo[i]);
        }
    }
    printf(" -> Frame %d\n",smk_info_cur_frame(s));
        // Advance to next frame

    }

    for (i = 0; i < 7; i++) fclose(fpo[i]);
    smk_close(s);

    return 0;
}
