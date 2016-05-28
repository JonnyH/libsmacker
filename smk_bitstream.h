/*
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2013 Greg Kennedy

	See smacker.h for more information.

	smk_bitstream.h
		SMK bitstream structure. Presents a block of raw bytes one
		bit at a time, and protects against over-read.
*/

#ifndef SMK_BITSTREAM_H
#define SMK_BITSTREAM_H

/* Bitstream structure, Forward declaration */
struct smk_bit_t;

/* BITSTREAM Functions */
/* Initialize a bitstream */
struct smk_bit_t* smk_bs_init(const unsigned char* b, unsigned long size);

/* Align the bitstream to next-byte boundary. */
/* As it turns out, this isn't needed. */
/* void smk_bs_align(struct smk_bit_t *); */

/* This macro interrogates return code from bs_read_1 and
	jumps to error label if problems occur. */
#define smk_bs_safe_read_1(t,uc) \
{ \
	if ((char)(uc = smk_bs_read_1(t)) < 0) \
	{ \
		fprintf(stderr, "libsmacker::smk_bs_safe_read_1(" #t ", " #uc ") - ERROR (file: %s, line: %lu)\n", __FILE__, (unsigned long)__LINE__); \
		goto error; \
	} \
}
/* Read a single bit from the bitstream, and advance.
	Returns -1 on error. */
char smk_bs_read_1(struct smk_bit_t* bs);

#define smk_bs_safe_read_8(t,s) \
{ \
	if ((short)(s = smk_bs_read_8(t)) < 0) \
	{ \
		fprintf(stderr, "libsmacker::smk_bs_safe_read_8(" #t ", " #s ") - ERROR (file: %s, line: %lu)\n", __FILE__, (unsigned long)__LINE__); \
		goto error; \
	} \
}
/* Read eight bits from the bitstream (one byte), and advance.
	Returns -1 on error. */
short smk_bs_read_8(struct smk_bit_t* bs);

#endif
