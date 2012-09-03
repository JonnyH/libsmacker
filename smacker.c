/* see smacker.h */
#include "smacker.h"

/* defines NULL... */
#include <stdlib.h>
/* FILE pointers and etc */
#include <stdio.h>
/* memset */
#include <string.h>
/* error handling ("C Exceptions") */
#include <setjmp.h>

/* some flags defines */
#define	SMK_FLAG_RING_FRAME	0x01
#define	SMK_FLAG_Y_INTERLACE	0x02
#define	SMK_FLAG_Y_DOUBLE	0x04

#define SMK_FLAG_KEYFRAME	0x01

#define SMK_FLAG_AUDIO_COMPRESS	(1 << 31)
#define SMK_FLAG_AUDIO_EXISTS	(1 << 30)
#define SMK_FLAG_AUDIO_BITRATE  (1 << 29)
#define SMK_FLAG_AUDIO_STEREO   (1 << 28)
#define SMK_FLAG_AUDIO_V2       (0x03 << 26)

/* GLOBALS */
/* for error handling */
static jmp_buf jb;


struct smk_huff_t
{
    struct smk_huff_t *b0;
    struct smk_huff_t *b1;
    unsigned short value;
};

struct smk_t
{
/* meta-info */
    unsigned int	w;
    unsigned int	h;
    unsigned int	f;
    float       	fps;
    unsigned char	v;
    unsigned char	flags;

    unsigned char       audio_compress[7];
    unsigned char       audio_exists[7];
    unsigned char       audio_channels[7];
    unsigned char       audio_bitdepth[7];
    unsigned int        audio_rate[7];

/* persistence, and usability */
    unsigned int    cur_frame;
    unsigned char	*palette;
    unsigned char	*audio[7];
    unsigned char	*image;

/* Huffman trees */
    struct smk_huff_t *mmap;
    struct smk_huff_t *mclr;
    struct smk_huff_t *full;
    struct smk_huff_t *type;

/* unprocessed image chunks */
    unsigned char     *frame_flags;

    unsigned char     **chunk_pal;
    unsigned char     **chunk_audio[7];
    unsigned char     **chunk_video;
};

/* An fread wrapper: consumes N bytes, throws an except
   when size doesn't match expected */
static void read(void *buf, size_t size, FILE *fp)
{
    if (fread(buf,size,1,fp) != 1)
    {
        fprintf(stderr,"libsmacker::read() - ERROR - Short read looking for %lu bytes\n",size);
        longjmp(jb,0);
    }
}

/* And helper functions to do the reading, plus
   byteswap from LE to host order */
static unsigned int smk_read_ui(FILE *fp)
{
    unsigned char buf[4];
    read(buf,4,fp);
    return ((unsigned int) buf[3] << 24) |
           ((unsigned int) buf[2] << 16) |
           ((unsigned int) buf[1] << 8) |
           ((unsigned int) buf[0]);
}

/* static int smk_read_i(FILE *fp)
{
    return (int)smk_read_ui(fp);
} */

static unsigned char smk_read_uc(FILE *fp)
{
    unsigned char buf;
    read(&buf,1,fp);
    return buf;
}

/* "Renders" (unpacks) the frame at cur_frame
   Preps all the image and audio pointers */
static void smk_render(smk s)
{
    const unsigned char palmap[64] = {
        0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C,
        0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
        0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D,
        0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
        0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E,
        0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
        0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF,
        0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF
    };

    const unsigned short sizetable[64] = {
        1,     2,    3,    4,    5,    6,    7,    8,
        9,    10,   11,   12,   13,   14,   15,   16,
        17,   18,   19,   20,   21,   22,   23,   24,
        25,   26,   27,   28,   29,   30,   31,   32,
        33,   34,   35,   36,   37,   38,   39,   40,
        41,   42,   43,   44,   45,   46,   47,   48,
        49,   50,   51,   52,   53,   54,   55,   56,
        57,   58,   59,  128,  256,  512, 1024, 2048
    };

    unsigned char i;
    unsigned short j;

    /* First: unpack palette, if a pal rec exists */
    if (s->chunk_pal[s->cur_frame] != NULL)
    {
        unsigned char *p = s->chunk_pal[s->cur_frame];
        j = 0;
        while (j < 256)
        {
            if (*p & 0x80)
            {
                j += (*p & 0x7F);
            } else if (*p & 0x40) {
            } else {
            }
        }
    }

    /* Audio - two modes, uncompressed PCM or Smacker DPCM */
    for (i=0; i<7; i++)
    {
        /* skip any processing if no such track */
        if (s->audio_exists[i])
        {
            if (s->chunk_audio[i][s->cur_frame] != NULL)
            {
                if (s->audio_compress[i])
                {
printf("todo: decompress audio\n");
                    /* decompress SMACK DPCM */
                } else {
                    /* uncompressed PCM */
                    /*  set audio buf size */
                    /* s->audio_len[i] = s->chunk_audio_size[i][s->cur_frame]; */
                    /* memcpy data.  This could be faster if we just
                        pointed audio[i] straight at chunk, but then there
                        is mayhem when freeing (below). */
                    /* memcpy(s->audio[i], s->chunk_audio[i][s->cur_frame],s->audio_len[i]); */
                    fprintf(stderr,"ERROR: don't know how to handle raw PCM samples, please send file for testing\n");
                }
            }
        }
    }
}

/* PUBLIC FUNCTIONS */
/* open an smk (from a file) */
smk smk_open(const char* fn)
{
    /* sadly we must declare this volatile */
    volatile smk s;

    size_t retval;
    int temp_i;
    unsigned int temp_u, temp_u2, framedata_size;
    unsigned char b[4];
    unsigned char *hufftree_chunk;

    /* make some temp arrays for holding things */
    unsigned int tree_size;

    unsigned int *frame_sizes;
    unsigned char     *frame_types;

    FILE *fp = fopen(fn,"rb");
    if (fp == NULL) return NULL;

    s = (smk) malloc (sizeof (struct smk_t));
    if (s == NULL)
    {
        fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: Unable to allocate %lu bytes for smk struct\n",fn,sizeof(struct smk_t));
        fclose(fp);
        return NULL;
    }
    memset(s,0,sizeof(struct smk_t));

    /* let's read! */
    if (!setjmp(jb))
    {
        /* Check for a valid signature */
        retval = fread(b,1,3,fp);
        if (retval != 3 || (memcmp(b,"SMK",3) ))
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: invalid SMKn signature\n",fn);
            longjmp(jb,0);
        }

        s->v = smk_read_uc(fp);
        if (s->v != '2' && s->v != '4')
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: invalid SMK version %u (expected: 2 or 4)\n",fn,s->v);
            longjmp(jb,0);
        }

        /* allocate space for palette buffer */
        s->palette = malloc(s->w * s->h);
        if (s->palette == NULL)
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: unable to malloc %u bytes for palette buf\n",fn,s->w * s->h);
            longjmp(jb,0);
        }

        /* width, height, total num frames */
	s->w = smk_read_ui(fp);
	s->h = smk_read_ui(fp);

        /* allocate space for image buffer */
        s->image = malloc(s->w * s->h);
        if (s->image == NULL)
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: unable to malloc %u bytes for frame buffer\n",fn,s->w * s->h);
            longjmp(jb,0);
        }
 
	s->f = smk_read_ui(fp);

        /* frames per second calculation */
	temp_i = (int)smk_read_ui(fp);
        if (temp_i > 0)
            s->fps = 1000.0f / temp_i;
        else if (temp_i < 0)
            s->fps = -100000.0f / temp_i;
        else
            s->fps = 10.0f;

	temp_u = smk_read_ui(fp);
        s->flags = temp_u % 256;

        for (temp_i = 0; temp_i < 7; temp_i ++)
        {
            temp_u = smk_read_ui(fp);
            if (temp_u > 0)
            {
printf("Audio buffer %d max size %u\n",temp_i,temp_u);
                /* allocate space for audio buffer */
                s->audio[temp_i] = malloc(temp_u);
                if (s->audio[temp_i] == NULL)
                {
                    fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: unable to malloc %u bytes for audio[%d] buf\n",fn,s->w * s->h,temp_i);
                    longjmp(jb,0);
                }
            }
        }

        tree_size = smk_read_ui(fp);

        /* Skip over these "unpacked" sizes, they are specific to
           the official smackw32.dll usage */
        if (fseek(fp,16,SEEK_CUR))
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: huff-table size not skipped OK.\n",fn);
            longjmp(jb,0);
        }

        /* read audio rate data */
        for (temp_i = 0; temp_i < 7; temp_i ++)
        {
            temp_u = smk_read_ui(fp);
            s->audio_compress[temp_i] = ((temp_u & SMK_FLAG_AUDIO_COMPRESS) ? 1 : 0);
            s->audio_exists[temp_i] = ((temp_u & SMK_FLAG_AUDIO_EXISTS) ? 1 : 0);
            s->audio_bitdepth[temp_i] = ((temp_u & SMK_FLAG_AUDIO_BITRATE) ? 16 : 8);
            s->audio_channels[temp_i] = ((temp_u & SMK_FLAG_AUDIO_STEREO) ? 2 : 1);
            if ((temp_u & SMK_FLAG_AUDIO_V2) != 0)
            {
                fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: audio track %d is compressed with Bink (perceptual) Audio Codec: this is currently unsupported by libsmacker\n",fn,temp_i);
                longjmp(jb,0);
            }
            s->audio_rate[temp_i] = (temp_u & 0x00FFFFFF);
        }

        if (fseek(fp,4,SEEK_CUR))
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: dummy value not skipped OK.\n",fn);
            longjmp(jb,0);
        }

        /* Onto FrameSizes array */
        s->frame_flags = malloc(s->f);

        frame_sizes = malloc(s->f * 4);
        for (temp_u = 0; temp_u < s->f; temp_u ++)
        {
            frame_sizes[temp_u] = smk_read_ui(fp);

            if (frame_sizes[temp_u] & SMK_FLAG_KEYFRAME)
{
                s->frame_flags[temp_u] = SMK_FLAG_KEYFRAME;
printf("KEYFRAME!\n");}
            else
                s->frame_flags[temp_u] = 0;
            frame_sizes[temp_u] &= 0xFFFFFFFC;
        }

        /* That was easy... FrameTypes! */
        frame_types = malloc(s->f);
        for (temp_u = 0; temp_u < s->f; temp_u ++)
        {
            frame_types[temp_u] = smk_read_uc(fp);
        }

        /* HuffmanTrees
           We know the sizes already: read and assemble into
           something actually parse-able at run-time */
        hufftree_chunk = malloc(tree_size);
        retval = fread(hufftree_chunk,1,tree_size,fp);
        if (retval != tree_size)
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: short read on hufftree block (wanted %u, got %lu)\n",fn,tree_size,retval);
            longjmp(jb,0);
        }

        /* Read in the rest of the data. */
        s->chunk_pal = malloc(s->f * sizeof(unsigned int*));
        for (temp_i = 0; temp_i < 7; temp_i ++)
        {
            s->chunk_audio[temp_i] = malloc(s->f * sizeof(unsigned int*));
        }
        s->chunk_video = malloc(s->f * sizeof(unsigned int *));

        for (temp_u = 0; temp_u < s->f; temp_u ++)
        {
            /* Set up the initial FrameSize amount */
            framedata_size = frame_sizes[temp_u];

            /* First, a palette? */
            if (frame_types[temp_u] & 0x01)
            {
                temp_u2 = (4 * smk_read_uc(fp));
                framedata_size -= temp_u2;
                temp_u2 --;

                s->chunk_pal[temp_u] = malloc(temp_u2);
                retval = fread(s->chunk_pal[temp_u],1,temp_u2,fp);
                if (retval != temp_u2)
                {
                    fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: short read on palette %u (wanted %u, got %lu)\n",fn,temp_u,temp_u2,retval);
                    longjmp(jb,0);
                }
            } else {
                s->chunk_pal[temp_u] = NULL;
            }

            /* Next, audio data. */
            for (temp_i = 0; temp_i < 7; temp_i ++)
            {
                if (frame_types[temp_u] & (0x02 << temp_i))
                {
                    temp_u2 = smk_read_ui(fp);
                    framedata_size -= temp_u2;
                    temp_u2 -= 4;
printf("audio size %u\n",temp_u2);
                    s->chunk_audio[temp_i][temp_u] = malloc(temp_u2);
		    retval = fread(s->chunk_audio[temp_i][temp_u],1,temp_u2,fp);
                    if (retval != temp_u2)
                    {
                        fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: short read on audiochunk[channel %d][frame %u] (wanted %u, got %lu)\n",fn,temp_i,temp_u,temp_u2,retval);
                        longjmp(jb,0);
                    }
                } else {
                    s->chunk_audio[temp_i][temp_u] = NULL;
                }
            }

            /* finally, video frame */
            s->chunk_video[temp_u] = malloc(framedata_size);
            retval = fread(s->chunk_video[temp_u],1,framedata_size,fp);
            if (retval != framedata_size)
            {
                fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: short read on imagechunk[frame %u] (wanted %u, got %lu)\n",fn,temp_u,framedata_size,retval);
                longjmp(jb,0);
            }
        }

        /* guess that's it... try reading one more byte just to see if there was anything left to read */
        retval = fread(b,1,1,fp);
        if (retval != 0 || !feof(fp))
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: still data left to read at position %lu\n",fn,ftell(fp));
            longjmp(jb,0);
        }

        /* unpack the first frame */
        smk_render(s);
    } else {
        fprintf(stderr,"libsmacker::smk_open(%s) - Errors encountered, bailing out\n",fn);
        smk_close(s);
        s = NULL;
    }

    fclose(fp);

    return s;
}

static void smk_del_huffman(struct smk_huff_t *t)
{
    if (t->b0 != NULL) smk_del_huffman(t->b0);
    if (t->b1 != NULL) smk_del_huffman(t->b1);
    free(t);
}

/* close out an smk file and clean up memory */
void smk_close(smk s)
{
    int i;
    unsigned int j;

    if (s != NULL)
    {
    if (s->image != NULL) free(s->image);
    if (s->palette != NULL) free(s->palette);
    for (i=0; i<7; i++)
    {
        if (s->audio[i] != NULL) free(s->audio[i]);
    }

/* Huffman trees */
    if (s->mmap != NULL) smk_del_huffman(s->mmap);
    if (s->mclr != NULL) smk_del_huffman(s->mclr);
    if (s->full != NULL) smk_del_huffman(s->full);
    if (s->type != NULL) smk_del_huffman(s->type);

/* unprocessed image chunks */
    if (s->frame_flags != NULL) free(s->frame_flags);

    if (s->chunk_pal != NULL)
    {
        for (j=0; j<s->f; j++)
            if (s->chunk_pal[j] != NULL) free(s->chunk_pal[j]);
        free(s->chunk_pal);
    }

    for (i=0; i<7; i++)
    {
        if (s->chunk_audio[i] != NULL)
        {
            for (j=0; j<s->f; j++)
                if (s->chunk_audio[i][j] != NULL) free(s->chunk_audio[i][j]);
        }
        free(s->chunk_audio[i]);
    }

    if (s->chunk_video != NULL)
    {
        for (j=0; j<s->f; j++)
            if (s->chunk_video[j] != NULL) free(s->chunk_video[j]);
        free(s->chunk_video);
    }

    free(s);
    }
}

/* tell some info about the file */
unsigned int smk_info_h(smk s) { return s->h; }
unsigned int smk_info_w(smk s) { return s->w; }
unsigned int smk_info_f(smk s) { return s->f; }
float        smk_info_fps(smk s) { return s->fps; }

unsigned int smk_info_cur_frame(smk s) { return s->cur_frame; }

/* get info about audio tracks */
/* returns a BYTE with bitfields set, indicating presence of
   audio for each of 7 tracks */
unsigned char smk_info_audio_tracks(smk s)
{
    return ( ( (s->audio_exists[0]) << 0 ) |
             ( (s->audio_exists[1]) << 1 ) |
             ( (s->audio_exists[2]) << 2 ) |
             ( (s->audio_exists[3]) << 3 ) |
             ( (s->audio_exists[4]) << 4 ) |
             ( (s->audio_exists[5]) << 5 ) |
             ( (s->audio_exists[6]) << 6 ) );
}

unsigned char smk_info_audio_channels(smk s, unsigned char t) { return s->audio_channels[t]; }
unsigned char smk_info_audio_bitdepth(smk s, unsigned char t) { return s->audio_bitdepth[t]; }
unsigned int smk_info_audio_rate(smk s, unsigned char t) { return s->audio_rate[t]; }

unsigned char * smk_get_palette(smk s) { return s->palette; }
unsigned char * smk_get_frame(smk s) { return s->image; }
unsigned char * smk_get_audio(smk s, unsigned char t) { return s->audio[t]; }

/* advance to next frame */
int smk_next(smk s)
{
    if (s->cur_frame + 1 < s->f)
    {
        s->cur_frame ++;
        smk_render(s);
        if (s->cur_frame + 1 == s->f) return SMK_LAST;
        return SMK_MORE;
    }
    return SMK_DONE;
}

/* seek to a keyframe in an smk */
int smk_seek_keyframe(smk s, unsigned int f)
{
    /* rewind (or fast forward!) exactly to f */
    s->cur_frame = f;

    /* roll back to previous keyframe in stream, or 0 if no keyframes exist */
    while (s->cur_frame > 0 && !(s->frame_flags[s->cur_frame] & SMK_FLAG_KEYFRAME)) s->cur_frame --;

    /* render the frame: we're ready */
    smk_render(s);

    /* return "actual seek position" */
    return s->cur_frame;
}

/* seek to an exact frame in an smk */
int smk_seek_exact(smk s, unsigned int f)
{
    smk_seek_keyframe(s,f);
    while (s->cur_frame < f) smk_next(s);
    return s->cur_frame;
}
