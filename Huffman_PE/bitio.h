#ifndef BITIO_H
#define BITIO_H

#include <stdio.h>

/* TAD: escrita de bits em arquivo (MSB primeiro) */
typedef struct {
    FILE *f;
    unsigned char buffer;
    int n; /* bits ja acumulados no buffer */
} BitWriter;

/* TAD: leitura de bits de arquivo (MSB primeiro) */
typedef struct {
    FILE *f;
    unsigned char buffer;
    int n; /* bits ainda disponiveis no buffer */
} BitReader;

void bw_init(BitWriter *bw, FILE *f);
int bw_bit(BitWriter *bw, int bit); /* 1 = ok, 0 = erro de escrita */
int bw_flush(BitWriter *bw);        /* completa o ultimo byte com zeros */

void br_init(BitReader *br, FILE *f);
int br_bit(BitReader *br);          /* 0 ou 1; -1 se fim do arquivo */

#endif
