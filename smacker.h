/*
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2013 Greg Kennedy

	libsmacker is a cross-platform C library which can be used for
	decoding Smacker Video files produced by RAD Game Tools.

	This software is released under the following license:
	Creative Commons Attribution-NonCommercial 2.0 (CC BY-NC 2.0)

	You are free:
	* to Share - to copy, distribute and transmit the work
	* to Remix - to adapt the work

	Under the following conditions:

	* Attribution - You must attribute the work in the manner specified by
		the author or licensor (but not in any way that suggests that
		they endorse you or your use of the work).
	* Noncommercial - You may not use this work for commercial purposes.

	This is a human-readable summary of the Legal Code (the full license).

	You should have received a copy of the full license
		along with libsmacker.  If not, see
		<http://creativecommons.org/licenses/by-nc/2.0>.
*/

#ifndef SMACKER_H
#define SMACKER_H

/* forward-declaration for an struct */
typedef struct smk_t *smk;

/* a few defines as return codes from smk_next() */
#define SMK_DONE 0x00
#define SMK_MORE 0x01
#define SMK_LAST 0x02

/* file-processing mode, pass to smk_open_file */
#define SMK_MODE_MEMORY 0x00
#define SMK_MODE_DISK   0x01

/* data handling mode, pass to smk_open_memory */
#define SMK_MODE_REF          0x00
#define SMK_MODE_REF_NOFREE   0x01
#define SMK_MODE_COPY         0x02

/* PUBLIC FUNCTIONS */
/* open an smk (from a file) */
smk smk_open_file(const char*, unsigned char);
/* read an smk (from a memory buffer) */
smk smk_open_memory(unsigned char*, unsigned char);

/* close out an smk file and clean up memory */
void smk_close(smk);

/* enable/disable decode features */
void smk_enable_palette(smk, unsigned char);
void smk_enable_video(smk, unsigned char);
void smk_enable_audio(smk, unsigned char, unsigned char);

/* tell some info about the file */
unsigned int smk_info_video_w(smk);
unsigned int smk_info_video_h(smk);
unsigned int smk_info_f(smk);
float        smk_info_fps(smk);

/* get current frame number */
unsigned int smk_info_cur_frame(smk);

/* get info about audio tracks */
/* returns a BYTE with bitfields set, indicating presence of
   audio for each of 7 tracks */
unsigned char smk_info_audio_tracks(smk);
/* query for info about a specific track */
unsigned char smk_info_audio_channels(smk, unsigned char);
unsigned char smk_info_audio_bitdepth(smk, unsigned char);
unsigned int  smk_info_audio_rate(smk, unsigned char);

/* Retrieve palette */
unsigned char * smk_get_palette(smk);
unsigned char * smk_get_video(smk);
unsigned char * smk_get_audio(smk, unsigned char);
unsigned int smk_get_audio_size(smk, unsigned char);

/* rewind to first frame and unpack */
int smk_first(smk);
/* advance to next frame and unpack */
int smk_next(smk);

/* seek to a keyframe in an smk */
int smk_seek_keyframe(smk, unsigned int);

#endif
