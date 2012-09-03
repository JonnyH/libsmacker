/* libsmacker
   C library for processing .smk files */

#ifndef SMACKER_H
#define SMACKER_H

/* forward-declaration for an smktruct */
typedef struct smk_t *smk;

/* a few defines as return codes from smk_next() */
#define SMK_DONE 0x00
#define SMK_MORE 0x01
#define SMK_LAST 0x02

/* open an smk (from a file) */
smk smk_open(const char*);

/* close out an smk file and clean up memory */
void smk_close(smk);

/* tell some info about the file */
unsigned int smk_info_h(smk);
unsigned int smk_info_w(smk);
unsigned int smk_info_f(smk);
float        smk_info_fps(smk);

unsigned int smk_info_cur_frame(smk);

/* get info about audio tracks */
/* returns a BYTE with bitfields set, indicating presence of
   audio for each of 7 tracks */
unsigned char smk_info_audio_tracks(smk);
/* query for info about a specific track */
unsigned char smk_info_audio_channels(smk, unsigned char);
unsigned char smk_info_audio_bitdepth(smk, unsigned char);
unsigned int smk_info_audio_rate(smk, unsigned char);

unsigned char * smk_get_palette(smk);
unsigned char * smk_get_frame(smk);
unsigned char * smk_get_audio(smk, unsigned char);

/* advance to next frame */
int smk_next(smk);

/* seek to a keyframe in an smk */
int smk_seek_keyframe(smk, unsigned int);

/* seek to an exact frame in an smk */
int smk_seek_exact(smk, unsigned int);

#endif
