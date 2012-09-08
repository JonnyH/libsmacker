#ifndef SMK_HUFFTREE_H
#define SMK_HUFFTREE_H

#include "smk_bitstream.h"

struct smk_huff_t
{
    struct smk_huff_t *b0;
    struct smk_huff_t *b1;
    unsigned short value;
    unsigned char escapecode;
};

struct smk_huff_big_t
{
    struct smk_huff_t *t;
    unsigned short s16[3];
};


/* function to recursively delete a huffman tree */
void smk_del_huffman(struct smk_huff_t *);

struct smk_huff_t *smk_build_tree(struct smk_bit_t*);

unsigned short smk_tree_lookup (struct smk_bit_t *, struct smk_huff_t *);

struct smk_huff_big_t *smk_build_bigtree(struct smk_bit_t *);

unsigned short smk_bigtree_lookup (struct smk_bit_t *, struct smk_huff_big_t *);

void smk_bigtree_reset (struct smk_huff_big_t *);

#endif
