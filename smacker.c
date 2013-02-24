/*
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2013 Greg Kennedy

	See smacker.h for more information.

	smacker.c
		Main implementation file of libsmacker
*/
	
#include "smacker.h"

/* safe malloc and free */
#include "smk_malloc.h"

/* data structures */
#include "smk_bitstream.h"
#include "smk_hufftree.h"

/* fp error handling */
#include <errno.h>
/* memset */
#include <string.h>
/* error handling ("C Exceptions") */
#include <setjmp.h>

/* some flags defines */
#define    SMK_FLAG_RING_FRAME    0x01
#define    SMK_FLAG_Y_INTERLACE    0x02
#define    SMK_FLAG_Y_DOUBLE    0x04

#define SMK_FLAG_KEYFRAME    0x01

#define SMK_FLAG_AUDIO_COMPRESS    (1 << 31)
#define SMK_FLAG_AUDIO_EXISTS    (1 << 30)
#define SMK_FLAG_AUDIO_BITRATE  (1 << 29)
#define SMK_FLAG_AUDIO_STEREO   (1 << 28)
#define SMK_FLAG_AUDIO_V2       (0x03 << 26)

/* GLOBALS */
/* for error handling */
static jmp_buf jb;

/* SMACKER DATA STRUCTURES */
struct smk_palette_t
{
    /* user-switch */
    unsigned char       enable;

    unsigned char       *buffer;
};

struct smk_video_t
{
    unsigned int    w;
    unsigned int    h;

    /* display flags: Y-interlace, Y-double, ring frame */
    unsigned char       flags;

    /* user-switch */
    unsigned char       enable;

    /* Huffman trees */
    struct smk_huff_big_t   *mmap;
    struct smk_huff_big_t   *mclr;
    struct smk_huff_big_t   *full;
    struct smk_huff_big_t   *type;

    unsigned char       *buffer;
};

struct smk_audio_t
{
    unsigned char       compress;
    unsigned char       exists;
    unsigned char       channels;
    unsigned char       bitdepth;
    unsigned int        rate;

    /* unsigned int        max_buffer; */

    /* user-switch */
    unsigned char       enable;

    void                *buffer;
    unsigned int        buffer_size;
};

struct smk_t
{
    /* meta-info */
    unsigned char    v;
    unsigned char       mode;

    /* total frames, frames per second */
    unsigned int    f;
    float           fps;

    /* Holds per-frame flags (i.e. 'keyframe') */
    unsigned char       *frame_flags;

    /* Holds per-frame types (i.e. 'audio track 3, 2, and palette swap'') */
    unsigned char       *frame_type;

    /* Index of current frame */
    unsigned int        cur_frame;

    /* on-disk mode */
    FILE                *fp;
    unsigned int  *chunk_offset;

    /* in-memory mode: unprocessed chunks */
    unsigned char **chunk_data;

    /* shared */
    unsigned int *chunk_size;

    /* decoded components */
    /* palette */
    struct smk_palette_t  palette;

    /* video */
    struct smk_video_t  video;

    /* audio */
    struct smk_audio_t  audio[7];

};

/* An fread wrapper: consumes N bytes, throws an except
   when size doesn't match expected */
static void smk_read(void *buf, size_t size, FILE *fp)
{
    size_t bytesRead = fread(buf,1,size,fp);
    if (bytesRead != size)
    {
        fprintf(stderr,"libsmacker::smk_read() - ERROR - Short read looking for %lu bytes, got %lu instead\n",(unsigned long)size, (unsigned long)bytesRead);
        longjmp(jb,0);
    }
}

/* And helper functions to do the reading, plus
   byteswap from LE to host order */
static unsigned int smk_read_ui(FILE *fp)
{
    unsigned char buf[4];
    smk_read(buf,4,fp);
    return ((unsigned int) buf[3] << 24) |
        ((unsigned int) buf[2] << 16) |
        ((unsigned int) buf[1] << 8) |
        ((unsigned int) buf[0]);
}

static unsigned char smk_read_uc(FILE *fp)
{
    unsigned char buf;
    smk_read(&buf,1,fp);
    return buf;
}

/* Same as above, except it reads from a RAM buffer */
static unsigned int smk_grab_ui(unsigned char *buf)
{
    return ((unsigned int) buf[3] << 24) |
        ((unsigned int) buf[2] << 16) |
        ((unsigned int) buf[1] << 8) |
        ((unsigned int) buf[0]);
}

/* Decompresses a palette-frame. */
static unsigned char *smk_render_palette(smk s, unsigned char *p)
{
    unsigned int i,j,k,size;
    unsigned char *t;

    /* Smacker palette map */
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

    /* sanity check */
    if (p == NULL)
    {
        fprintf(stderr,"libsmacker::palette_render() - ERROR: NULL palette");
        return NULL;
    }

    /* Byte 1 in block, *4, tells how many subsequent bytes are present */
    size = 4 * (*p);

    /* If palette rendering enabled... */
    if (s->palette.enable)
    {
        p ++; size --;
        smk_malloc(t,768);
        /* memset(t,0,768); */ /* is this needed? */
        i = 0; /* index into NEW palette */
        j = 0; /* Index into OLD palette */
        while ( (i < 256) && (size > 0) ) /* looping index into NEW palette */
        {
            if ((*p) & 0x80)
            {
                /* Copy next (c + 1) color entries of the previous palette to the next entries of the new palette. */
                k = ((*p) & 0x7F) + 1;
                if (j + k > 256 || i + k > 256)
                {
                    fprintf(stderr,"libsmacker::palette_render() - ERROR: frame %u, overflow, 0x80 attempt to copy %d bytes from %d to %d\n",s->cur_frame,k,j,i);
                } else if (s->palette.buffer != NULL)
                {
                    memcpy(&t[i*3],&s->palette.buffer[j*3],k * 3);
                } else {
                    memset(&t[i*3],0,k*3);
                }
                i += k;
                j += k;
                p ++; size --;
            } else if ((*p) & 0x40) {
                /* Copy (c + 1) color entries of the previous palette, starting from entry (s) to the next entries of the new palette. */
                if (size == 1)
                {
                    fprintf(stderr,"libsmacker::palette_render() - ERROR: frame %u, 0x40 ran out of bytes for copy\n",s->cur_frame);
                }
                k = ((*p) & 0x3F) + 1;  /* count */
                p ++; size --;
                j = (*p);
                if (j + k > 256 || i + k > 256)
                {
                    fprintf(stderr,"libsmacker::palette_render() - ERROR: frame %u, overflow, 0x40 attempt to copy %d bytes from %d to %d\n",s->cur_frame,k,j,i);
                } else if (s->palette.buffer != NULL)
                {
                    memcpy(&t[i*3],&s->palette.buffer[j*3],k * 3);
                } else {
                    memset(&t[i*3],0,k*3);
                }
                i += k;
                j += k;
                p ++; size --;
            } else {
                if (size == 2)
                {
                    fprintf(stderr,"libsmacker::palette_render - ERROR: frame %u, 0x00 ran out of bytes for copy, size=%d\n",s->cur_frame,size);
                }
                t[(i * 3)] = palmap[*p]; p++; size --;
                t[(i * 3) + 1] = palmap[*p]; p++; size --;
                t[(i * 3) + 2] = palmap[*p]; p++; size --;
                i ++;
            }
        }

        smk_free (s->palette.buffer);
        s->palette.buffer = t;
    }

    /* advance any remaining unparsed distance */
#ifdef _DEBUG
    if (size > 0) printf ("Info: advancing %u bytes after palette decompress.\n",size);
    if (i < 256) printf ("Info: filled only %u bytes of palette in decompress.\n",i);
#endif
    p += size;

    return p;
}

/* Decompress audio track i. */
static unsigned char *smk_render_audio(smk s, unsigned int i, unsigned char *p)
{
    unsigned int j,k,size;
    unsigned char *t;
    struct smk_bit_t *bs;

    /* used for audio decoding */
    struct smk_huff_t *aud_tree[4] = {NULL,NULL,NULL,NULL};

    /* unsigned int tells us size of audio chunk */
    size = smk_grab_ui(p);

    /* only perform decompress if track is enabled */
    if (s->audio[i].enable)
    {
        p += 4; size -= 4;

        /* only perform decompress if track is enabled */
        if (s->audio[i].compress)
        {
            /* chunk is compressed (huff-compressed dpcm), retrieve unpacked buffer size */
            s->audio[i].buffer_size = smk_grab_ui(p);
            p += 4; size -= 4;

            /* malloc a buffer to hold unpacked audio */
            smk_malloc(t,s->audio[i].buffer_size);

            /* Compressed audio: must unpack here */
            /*  Set up a bitstream */
            bs = smk_bs_init (p, size);

            if (smk_bs_1(bs))
            {
                if (s->audio[i].channels != (smk_bs_1(bs) == 1 ? 2 : 1))
                {
                    fprintf(stderr,"libsmacker::smk_render - ERROR: frame %u, audio channel %u, mono/stereo mismatch\n",s->cur_frame,i);
                    /* advance any remaining unparsed distance */
                    p += size;
                    return p;
                }
                if (s->audio[i].bitdepth != (smk_bs_1(bs) == 1 ? 16 : 8))
                {
                    fprintf(stderr,"libsmacker::smk_render - ERROR: frame %u, audio channel %u, 8-/16-bit mismatch\n",s->cur_frame,i);
                    /* advance any remaining unparsed distance */
                    p += size;
                    return p;
                }

                if (s->audio[i].channels == 1)
                {
                    aud_tree[0] = smk_build_tree(bs);
                    aud_tree[2] = NULL;
                    aud_tree[3] = NULL;
                    if (s->audio[i].bitdepth == 8)
                    {
                        aud_tree[1] = NULL;
                        ((unsigned char *)t)[0] = smk_bs_8(bs);
                        k=1;
                    } else {
                        aud_tree[1] = smk_build_tree(bs);
                        ((short *)t)[0] = (((unsigned short)smk_bs_8(bs)) << 8) | ((unsigned short)(smk_bs_8(bs)));
                        k=2;
                    }
                    j = 1;
                } else {
                    aud_tree[0] = smk_build_tree(bs);
                    aud_tree[1] = smk_build_tree(bs);
                    if (s->audio[i].bitdepth == 8)
                    {
                        aud_tree[2] = NULL;
                        aud_tree[3] = NULL;
                        ((unsigned char *)t)[1] = smk_bs_8(bs);
                        ((unsigned char *)t)[0] = smk_bs_8(bs);
                        k=2;
                    } else {
                        aud_tree[2] = smk_build_tree(bs);
                        aud_tree[3] = smk_build_tree(bs);
                        ((short *)t)[1] = (((unsigned short)smk_bs_8(bs)) << 8) | ((unsigned short)(smk_bs_8(bs)));
                        ((short *)t)[0] = (((unsigned short)smk_bs_8(bs)) << 8) | ((unsigned short)(smk_bs_8(bs)));
                        k=4;
                    }
                    j = 2;
                }

/* All set: let's read some DATA! */
                while (k < s->audio[i].buffer_size)
                {
                    if (s->audio[i].channels == 1)
                    {
                        if (s->audio[i].bitdepth == 8)
                        {
                            ((unsigned char *)t)[j] = ((char)smk_tree_lookup(bs,aud_tree[0])) + ((unsigned char *)t)[j - 1];
                            j ++;
                            k++;
                        } else {
                            ((short *)t)[j] = (short) ( smk_tree_lookup(bs,aud_tree[0]) | (smk_tree_lookup(bs,aud_tree[1]) << 8) )
                                                         + ((short *)t)[j - 1];
                            j ++;
                            k+=2;
                        }
                    } else {
                        if (s->audio[i].bitdepth == 8)
                        {
                            ((char *)t)[j] = (smk_tree_lookup(bs,aud_tree[0])) + ((char *)t)[j - 2];
                            j ++;
                            ((char *)t)[j] = (smk_tree_lookup(bs,aud_tree[1])) + ((char *)t)[j - 2];
                            j ++;
                            k+=2;
                        }  else {
                            ((short *)t)[j] = (short) ( smk_tree_lookup(bs,aud_tree[0]) | (smk_tree_lookup(bs,aud_tree[1]) << 8) )
                                                         + ((short *)t)[j - 2];
                            j ++;
                            ((short *)t)[j] = (short) ( smk_tree_lookup(bs,aud_tree[2]) | (smk_tree_lookup(bs,aud_tree[3]) << 8) )
                                                         + ((short *)t)[j - 2];
                            j ++;
                            k+=4;
                        }
                    }
                }
            }

            p += (bs->byte_num + 1);
            size -= (bs->byte_num + 1);

            /* All done with the trees, free them. */
            smk_del_huffman(aud_tree[0]);
            smk_del_huffman(aud_tree[1]);
            smk_del_huffman(aud_tree[2]);
            smk_del_huffman(aud_tree[3]);

            /* All done with the bitstream, free it. */
            smk_free(bs);
        } else {
            /* Raw PCM data, update buffer size and malloc */
            s->audio[i].buffer_size = size;
            smk_malloc(t,s->audio[i].buffer_size);

            memcpy(t,p,size);
            p += size;
            size = 0;
        }

        smk_free (s->audio[i].buffer);

        s->audio[i].buffer = t;
    }

    /* advance any remaining unparsed distance */
    p += size;
    return p;
}

static unsigned char *smk_render_video(smk s, unsigned int size, unsigned char *p)
{
    unsigned char *t,s1,s2;

    unsigned int j,k, row, col;

    /* used for video decoding */
    struct smk_bit_t *bs;
    unsigned short unpack;
    unsigned char type;
    unsigned char blocklen;
    unsigned char typedata;

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

    if (s->video.enable)
    {
        smk_malloc(t,s->video.w * s->video.h);

        row = 0;
        col = 0;

    /* Set up a bitstream for video unpacking */
        bs = smk_bs_init (p, size);

        smk_bigtree_reset(s->video.mmap);
        smk_bigtree_reset(s->video.mclr);
        smk_bigtree_reset(s->video.full);
        smk_bigtree_reset(s->video.type);

        while ( ((row * s->video.w) + col) < (s->video.w * s->video.h) )
        {
            if (!smk_bits_left(bs))
            {
                fprintf(stderr,"libsmacker::smk_render_video() - ERROR - out of bits in bitstream\n");
    		p += size;
    		return p;
            }
            unpack = smk_bigtree_lookup(bs,s->video.type);
            type = ((unpack & 0x0003));
            blocklen = ((unpack & 0x00FC) >> 2 );
            typedata = ((unpack & 0xFF00) >> 8);
 /* printf("Retrieved: type %u, blocklen %u, typedata %02X.  (overall unpack: %04X) BS position %u.%u\n",type,blocklen,typedata,unpack,bs->byte_num,bs->bit_num); */
            /* support for v4 full-blocks */
            if (type == 1 && s->v == '4')
            {
              if (smk_bs_1(bs))
              {
                type = 4;
              } else if (smk_bs_1(bs))
              {
                  type = 5;
              }
            }

            for (j = 0; (j < sizetable[blocklen]) && (((row * s->video.w) + col) < (s->video.w * s->video.h)); j ++)
            {
                switch(type)
                {
                    case 0:
                        unpack = smk_bigtree_lookup(bs,s->video.mclr);
                        s1 = (unpack & 0xFF00) >> 8;
                        s2 = (unpack & 0x00FF);
                        unpack = smk_bigtree_lookup(bs,s->video.mmap);
                        for (k = 0; k < 16; k ++)
                        {
                            if (unpack & (1 << k))
                                t[(row + (k / 4)) * s->video.w + col + (k % 4) ] = s1;
                            else
                                t[(row + (k / 4)) * s->video.w + col + (k % 4) ] = s2;
                        }
                        break;

                    case 1: /* FULL BLOCK */
                        for (k = 0; k < 4; k ++)
                        {
                          unpack = smk_bigtree_lookup(bs,s->video.full);
                          t[(row + k) * s->video.w + col + 3] = ((unpack & 0xFF00) >> 8);
                          t[(row + k) * s->video.w + col + 2] = (unpack & 0x00FF);
                          unpack = smk_bigtree_lookup(bs,s->video.full);
                          t[(row + k) * s->video.w + col + 1] = ((unpack & 0xFF00) >> 8);
                          t[(row + k) * s->video.w + col] = (unpack & 0x00FF);
                        }
                        break;
                    case 2: /* VOID BLOCK */
                        memcpy(&t[row * s->video.w + col], &s->video.buffer[row * s->video.w + col], 4);
                        memcpy(&t[(row + 1) * s->video.w + col], &s->video.buffer[(row + 1) * s->video.w + col], 4);
                        memcpy(&t[(row + 2) * s->video.w + col], &s->video.buffer[(row + 2) * s->video.w + col], 4);
                        memcpy(&t[(row + 3) * s->video.w + col], &s->video.buffer[(row + 3) * s->video.w + col], 4);
                        break;
                    case 3: /* SOLID BLOCK */
                        memset(&t[row * s->video.w + col],typedata,4);
                        memset(&t[(row + 1) * s->video.w + col],typedata,4);
                        memset(&t[(row + 2) * s->video.w + col],typedata,4);
                        memset(&t[(row + 3) * s->video.w + col],typedata,4);
                        break;
                    case 4: /* V4 DOUBLE BLOCK */
                        for (k = 0; k < 2; k ++)
                        {
                          unpack = smk_bigtree_lookup(bs,s->video.full);
                          t[(row + 2 * k) * s->video.w + col + 3] = ((unpack & 0xFF00) >> 8);
                          t[(row + 2 * k) * s->video.w + col + 2] = ((unpack & 0xFF00) >> 8);
                          t[(row + 2 * k + 1) * s->video.w + col + 3] = ((unpack & 0xFF00) >> 8);
                          t[(row + 2 * k + 1) * s->video.w + col + 2] = ((unpack & 0xFF00) >> 8);
                          t[(row + 2 * k) * s->video.w + col + 1] = (unpack & 0x00FF);
                          t[(row + 2 * k) * s->video.w + col] = (unpack & 0x00FF);
                          t[(row + 2 * k + 1) * s->video.w + col + 1] = (unpack & 0x00FF);
                          t[(row + 2 * k + 1) * s->video.w + col] = (unpack & 0x00FF);
                        }
                        break;
                    case 5: /* V4 HALF BLOCK */
                        for (k = 0; k < 2; k ++)
                        {
                          unpack = smk_bigtree_lookup(bs,s->video.full);
                          t[(row + 2 * k) * s->video.w + col + 3] = ((unpack & 0xFF00) >> 8);
                          t[(row + 2 * k) * s->video.w + col + 2] = (unpack & 0x00FF);
                          t[(row + 2 * k + 1) * s->video.w + col + 3] = ((unpack & 0xFF00) >> 8);
                          t[(row + 2 * k + 1) * s->video.w + col + 2] = (unpack & 0x00FF);
                          unpack = smk_bigtree_lookup(bs,s->video.full);
                          t[(row + 2 * k) * s->video.w + col + 1] = ((unpack & 0xFF00) >> 8);
                          t[(row + 2 * k) * s->video.w + col] = (unpack & 0x00FF);
                          t[(row + 2 * k + 1) * s->video.w + col + 1] = ((unpack & 0xFF00) >> 8);
                          t[(row + 2 * k + 1) * s->video.w + col] = (unpack & 0x00FF);
                        }
                        break;
                }
                col += 4; if (col >= s->video.w) { col = 0; row += 4; }
            }
        }

        p += (bs->byte_num + 1);
        size -= (bs->byte_num + 1);

        smk_free(bs);

        smk_free (s->video.buffer);
        s->video.buffer = t;
    } else {
        printf("There was video here, but I skipped it.\n");
    }

    /* advance any remaining unparsed distance */
    p += size;
    return p;
}

/* "Renders" (unpacks) the frame at cur_frame
   Preps all the image and audio pointers */
static void smk_render(smk s)
{
    unsigned int i;
    unsigned char *buffer, *p;

    if (s->mode == SMK_MODE_DISK)
    {
        /* In disk-streaming mode: make way for our incoming chunk buffer */
        smk_malloc(buffer,s->chunk_size[s->cur_frame]);

        /* Skip to frame in file */

        if (fseek(s->fp,s->chunk_offset[s->cur_frame],SEEK_SET))
        {
            fprintf(stderr,"libsmacker::smk_render - ERROR: fseek to frame %u (offset %u) failed.\n",s->cur_frame,s->chunk_offset[s->cur_frame]);
            return;
        }

        /* Read into buffer */
        if (s->chunk_size[s->cur_frame] != fread(buffer,1,s->chunk_size[s->cur_frame],s->fp))
        {
            fprintf(stderr,"short read\n");
        }
    } else {
        /* Just point buffer at the right place */
        buffer = s->chunk_data[s->cur_frame];
    }

    p = buffer;

    /* Palette record first */
    if (s->frame_type[s->cur_frame] & 0x01)
    {
        p = smk_render_palette(s,p);
    }

    /* Unpack audio chunks */
    for (i = 0; i < 7; i ++)
    {
        if (s->frame_type[s->cur_frame] & (0x02 << i))
        {
            p = smk_render_audio(s,i,p);
        }
    }

    /* Unpack video chunk */
    p = smk_render_video(s, (s->chunk_size[s->cur_frame] - (p - buffer)), p);

    if (s->mode == SMK_MODE_DISK)
    {
        /* Remember that buffer we allocated?  Trash it */
        smk_free(buffer);
    }
}

/* PUBLIC FUNCTIONS */
/* open an smk (from a file) */
smk smk_open_file(const char* fn, unsigned char m)
{
    /* sadly we must declare this volatile */
    volatile smk s;

    size_t retval;
    int temp_i;
    unsigned int temp_u;
    unsigned char b[3];
    unsigned char *hufftree_chunk;

    struct smk_bit_t *bs;

    /* make some temp arrays for holding things */
    unsigned int tree_size;

    /* unsigned int *chunk_size; */
    /* unsigned char     *s->frame_type; */

    FILE *fp = fopen(fn,"rb");
    if (fp == NULL)
    {
        fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: could not open file (errno: %d)\n",fn,errno);
        perror ("    Error reported was");
        return NULL;
    }

    smk_malloc (s,sizeof (struct smk_t));
    if (s == NULL)
    {
        fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: Unable to allocate %lu bytes for smk struct\n",fn,(unsigned long)sizeof(struct smk_t));
        fclose(fp);
        return NULL;
    }
    memset(s,0,sizeof(struct smk_t));

    /* Set the Mode value */
    s->mode = (m == SMK_MODE_DISK ? SMK_MODE_DISK : SMK_MODE_MEMORY);

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

        /* Read .smk file version */
        s->v = smk_read_uc(fp);
        if (s->v != '2' && s->v != '4')
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: invalid SMK version %u (expected: 2 or 4)\n",fn,s->v);
            longjmp(jb,0);
        }

        /* width, height, total num frames */
        s->video.w = smk_read_ui(fp);
        s->video.h = smk_read_ui(fp);

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
        s->video.flags = temp_u % 256;

        /* Skip over these "unpacked" sizes, they are specific to
           the official smackw32.dll usage */
        if (fseek(fp,28,SEEK_CUR))
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: audio-buff size not skipped OK.\n",fn);
            longjmp(jb,0);
        }

        /* for (temp_i = 0; temp_i < 7; temp_i ++)
        {
            s->audio[temp_i].max_buffer = smk_read_ui(fp);
        } */

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
            s->audio[temp_i].compress = ((temp_u & SMK_FLAG_AUDIO_COMPRESS) ? 1 : 0);
            s->audio[temp_i].exists = ((temp_u & SMK_FLAG_AUDIO_EXISTS) ? 1 : 0);
            s->audio[temp_i].bitdepth = ((temp_u & SMK_FLAG_AUDIO_BITRATE) ? 16 : 8);
            s->audio[temp_i].channels = ((temp_u & SMK_FLAG_AUDIO_STEREO) ? 2 : 1);
            if ((temp_u & SMK_FLAG_AUDIO_V2) != 0)
            {
                fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: audio track %d is compressed with Bink (perceptual) Audio Codec: this is currently unsupported by libsmacker\n",fn,temp_i);
                longjmp(jb,0);
            }
            s->audio[temp_i].rate = (temp_u & 0x00FFFFFF);
        }

        /* Skip over Dummy field */
        if (fseek(fp,4,SEEK_CUR))
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: dummy value not skipped OK.\n",fn);
            longjmp(jb,0);
        }

        /* Onto FrameSizes array */
        smk_malloc(s->frame_flags,s->f);

        smk_malloc(s->chunk_size,s->f * 4);
        for (temp_u = 0; temp_u < s->f; temp_u ++)
        {
            s->chunk_size[temp_u] = smk_read_ui(fp);

            if (s->chunk_size[temp_u] & SMK_FLAG_KEYFRAME)
                s->frame_flags[temp_u] = SMK_FLAG_KEYFRAME;
            else
                s->frame_flags[temp_u] = 0;
            s->chunk_size[temp_u] &= 0xFFFFFFFC;
        }

        /* That was easy... FrameTypes! */
        smk_malloc(s->frame_type,s->f);
        for (temp_u = 0; temp_u < s->f; temp_u ++)
        {
            s->frame_type[temp_u] = smk_read_uc(fp);
        }

        /* HuffmanTrees
           We know the sizes already: read and assemble into
           something actually parse-able at run-time */
        smk_malloc(hufftree_chunk,tree_size);
        retval = fread(hufftree_chunk,1,tree_size,fp);
        if (retval != tree_size)
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: short read on hufftree block (wanted %u, got %lu)\n",fn,tree_size,(unsigned long)retval);
            longjmp(jb,0);
        }

        /* set up a Bitstream */
        bs = smk_bs_init(hufftree_chunk, tree_size);
        /* create some tables */
        printf("Checking for MMAP tree...\n");
        s->video.mmap = smk_build_bigtree(bs);
        printf("Checking for MCLR tree...\n");
        s->video.mclr = smk_build_bigtree(bs);
        printf("Checking for FULL tree...\n");
        s->video.full = smk_build_bigtree(bs);
        printf("Checking for TYPE tree...\n");
        s->video.type = smk_build_bigtree(bs);

        /* Read in the rest of the data. */
        /*   For MODE_MEMORY, read the chunks and store */
        if (s->mode == SMK_MODE_MEMORY)
        {
            smk_malloc(s->chunk_data,s->f * sizeof(unsigned char*));
            for (temp_u = 0; temp_u < s->f; temp_u ++)
            {
                smk_malloc(s->chunk_data[temp_u],s->chunk_size[temp_u]);
                retval = fread(s->chunk_data[temp_u],1,s->chunk_size[temp_u],fp);
                if (retval != s->chunk_size[temp_u])
                {
                    fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: short read on chunk_data block[%u] (wanted %u, got %lu)\n",fn,temp_u,s->chunk_size[temp_u],(unsigned long)retval);
                    longjmp(jb,0);
                }
            }
        } else {
            /* MODE_DISK: don't read anything now, just precompute offsets. */
            /*   use fseek to verify that the file is "complete" */
            smk_malloc(s->chunk_offset,s->f * sizeof(unsigned int));
            for (temp_u = 0; temp_u < s->f; temp_u ++)
            {
                s->chunk_offset[temp_u] = ftell(fp);
                if (fseek(fp,s->chunk_size[temp_u],SEEK_CUR))
                {
                    fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: fseek to frame %u not OK.\n",fn,temp_u);
                    longjmp(jb,0);
                }
            }
        }

        /* guess that's it... try reading one more byte just to see if there was anything left to read */
        retval = fread(b,1,1,fp);
        if (retval != 0 || !feof(fp))
        {
            fprintf(stderr,"libsmacker::smk_open(%s) - ERROR: still data left to read at position %lu\n",fn,ftell(fp));
            longjmp(jb,0);
        }

        /* All done, do what you will with the file pointer */
        if (s->mode == SMK_MODE_DISK)
            s->fp = fp;
        else
            fclose(fp);

    } else {
        fprintf(stderr,"libsmacker::smk_open(%s) - Errors encountered, bailing out\n",fn);
        smk_close(s);
        s = NULL;

        fclose(fp);
    }

    return s;
}

/* close out an smk file and clean up memory */
void smk_close(smk s)
{
    unsigned int u;

    if (s != NULL)
    {
        smk_free(s->palette.buffer);
        smk_free (s->video.buffer);
        for (u=0; u<7; u++)
            smk_free (s->audio[u].buffer);

        /* Huffman trees */
        if (s->video.mmap != NULL)
        {
            smk_del_huffman(s->video.mmap->t);
            smk_free(s->video.mmap);
        }
        if (s->video.mclr != NULL)
        {
            smk_del_huffman(s->video.mclr->t);
            smk_free(s->video.mclr);
        }
        if (s->video.full != NULL)
        {
            smk_del_huffman(s->video.full->t);
            smk_free(s->video.full);
        }
        if (s->video.type != NULL)
        {
            smk_del_huffman(s->video.type->t);
            smk_free(s->video.type);
        }

        smk_free(s->frame_flags);
        smk_free(s->frame_type);

        /* disk-mode */
        if (s->fp != NULL) fclose(s->fp);
        smk_free(s->chunk_offset);

        /* mem-mode */
        if (s->chunk_data != NULL)
        {
            for (u=0; u<s->f; u++)
            {
                smk_free(s->chunk_data[u]);
            }
            smk_free(s->chunk_data);
        }

        smk_free(s->chunk_size);

        smk_free(s);
    }
}

/* enable/disable decode features */
void smk_enable_palette(smk s, unsigned char v) { s->palette.enable = (v == 0 ? 0 : 1); }
void smk_enable_video(smk s, unsigned char v) { s->video.enable = (v == 0 ? 0 : 1); }
void smk_enable_audio(smk s, unsigned char t, unsigned char v) { s->audio[t].enable = (v == 0 ? 0 : 1); }

/* tell some info about the file */
unsigned int smk_info_video_w(smk s) { return s->video.w; }
unsigned int smk_info_video_h(smk s) { return s->video.h; }
unsigned int smk_info_f(smk s) { return s->f; }
float        smk_info_fps(smk s) { return s->fps; }

unsigned int smk_info_cur_frame(smk s) { return s->cur_frame; }

/* get info about audio tracks */
/* returns a BYTE with bitfields set, indicating presence of
   audio for each of 7 tracks */
unsigned char smk_info_audio_tracks(smk s)
{
    return ( ( (s->audio[0].exists) << 0 ) |
             ( (s->audio[1].exists) << 1 ) |
             ( (s->audio[2].exists) << 2 ) |
             ( (s->audio[3].exists) << 3 ) |
             ( (s->audio[4].exists) << 4 ) |
             ( (s->audio[5].exists) << 5 ) |
             ( (s->audio[6].exists) << 6 ) );
}

unsigned char smk_info_audio_channels(smk s, unsigned char t) { return s->audio[t].channels; }
unsigned char smk_info_audio_bitdepth(smk s, unsigned char t) { return s->audio[t].bitdepth; }
unsigned int smk_info_audio_rate(smk s, unsigned char t) { return s->audio[t].rate; }

unsigned char * smk_get_palette(smk s) { return s->palette.buffer; }
unsigned char * smk_get_video(smk s) { return s->video.buffer; }
unsigned char * smk_get_audio(smk s, unsigned char t) { return s->audio[t].buffer; }
unsigned int smk_get_audio_size(smk s, unsigned char t) { return s->audio[t].buffer_size; }

/* rewind to first frame and unpack */
int smk_first(smk s)
{
    s->cur_frame = 0;
    smk_render(s);

    if (s->f == 1) return SMK_LAST;
    return SMK_MORE;
}

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

