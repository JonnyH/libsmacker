#ifndef SMK_BITSTREAM_H
#define SMK_BITSTREAM_H

struct smk_bit_t
{
    unsigned char *bitstream;
    unsigned int size;

    unsigned int byte_num;
    char bit_num;
};

/* BITSTREAM Functions */
struct smk_bit_t *smk_bs_init(unsigned char *, unsigned int);

void smk_bs_align(struct smk_bit_t *);

unsigned char smk_bs_1(struct smk_bit_t *);

unsigned char smk_bs_8(struct smk_bit_t *);

unsigned char smk_bits_left(struct smk_bit_t *);
unsigned char smk_bytes_left(struct smk_bit_t *);

#endif
