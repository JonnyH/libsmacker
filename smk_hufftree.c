/* see smacker.h */
#include "smk_hufftree.h"

#include <stdio.h>

/* defines NULL... */
#include <stdlib.h>
/* memset */
#include <string.h>
/* error handling ("C Exceptions") */
#include <setjmp.h>

static struct smk_huff_t *smk_tree_rec(struct smk_bit_t *bs)
{
    struct smk_huff_t *ret = malloc(sizeof(struct smk_huff_t));

    if (smk_bs_1(bs))
    {
        ret->b0 = smk_tree_rec(bs);
        ret->b1 = smk_tree_rec(bs);
        ret->value = 0;
    } else {
        ret->b0 = NULL;
        ret->b1 = NULL;
        ret->value = smk_bs_8(bs);
    }
    return ret;
}

struct smk_huff_t *smk_build_tree(struct smk_bit_t* bs)
{
    struct smk_huff_t *ret = NULL;

    if (smk_bs_1(bs))
    {
        ret = smk_tree_rec(bs);
        if ( smk_bs_1(bs) ) printf("ERROR ERROR ERROR\n");
    }

    return ret;
}

unsigned char smk_tree_lookup (struct smk_bit_t *bs, struct smk_huff_t *t)
{
    if (t->b0 == NULL)
        return t->value;
    else if (smk_bs_1(bs))
        return smk_tree_lookup(bs,t->b1);
    else
        return smk_tree_lookup(bs,t->b0);
}

/* function to recursively delete a huffman tree */
void smk_del_huffman(struct smk_huff_t *t)
{
    if (t->b0 != NULL) smk_del_huffman(t->b0);
    if (t->b1 != NULL) smk_del_huffman(t->b1);
    free(t);
}
