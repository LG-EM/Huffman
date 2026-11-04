#include "bitio.h"

/*
 * MODULO DE ENTRADA/SAIDA DE BITS
 *
 * Por que existe: o C so le/grava bytes (8 bits), mas os codigos de Huffman
 * tem tamanho variavel (ex.: 01, 110101). Este modulo acumula bits num
 * "buffer" de 1 byte e so grava (ou le) quando o byte esta completo.
 */

/*
 * bw_init / br_init
 */
void bw_init(BitWriter *bw, FILE *f)
{
    bw->f = f;
    bw->buffer = 0;
    bw->n = 0;
}

/*
 * bw_bit - grava 1 bit (algoritmo: acumulador de bits, MSB primeiro)
 */
int bw_bit(BitWriter *bw, int bit)
{
    bw->buffer = (unsigned char)((bw->buffer << 1) | (bit & 1));
    bw->n++;
    if (bw->n == 8) {
        if (fputc(bw->buffer, bw->f) == EOF)
            return 0;
        bw->buffer = 0;
        bw->n = 0;
    }
    return 1;
}

/*
 * bw_flush - descarrega o ultimo byte incompleto
 */
int bw_flush(BitWriter *bw)
{
    if (bw->n > 0) {
        bw->buffer = (unsigned char)(bw->buffer << (8 - bw->n));
        if (fputc(bw->buffer, bw->f) == EOF)
            return 0;
        bw->buffer = 0;
        bw->n = 0;
    }
    return 1;
}

void br_init(BitReader *br, FILE *f)
{
    br->f = f;
    br->buffer = 0;
    br->n = 0;
}

/*
 * br_bit - le 1 bit (operacao inversa de bw_bit, MSB primeiro)
 */
int br_bit(BitReader *br)
{
    int c;

    if (br->n == 0) {
        c = fgetc(br->f);
        if (c == EOF)
            return -1;
        br->buffer = (unsigned char)c;
        br->n = 8;
    }
    br->n--;
    return (br->buffer >> br->n) & 1;
}
