#include "bitio.h"

/*
 * MODULO DE ENTRADA/SAIDA DE BITS
 *
 * Por que existe: o C so le/grava bytes (8 bits), mas os codigos de Huffman
 * tem tamanho variavel (ex.: 01, 110101). Este modulo acumula bits num
 * "buffer" de 1 byte e so grava (ou le) quando o byte esta completo.
 *
 * Todas as funcoes abaixo sao O(1) em tempo e em espaco: so fazem um numero
 * fixo de operacoes com bits sobre um unico byte. As chamadas fputc/fgetc
 * usam o buffer interno da stdio (blocos de alguns KB), entao o disco nao
 * e acessado a cada byte; o custo medio por chamada continua O(1).
 */

/*
 * bw_init / br_init
 * O que faz: zera o buffer e guarda o arquivo.
 * Tempo: O(1). Espaco: O(1).
 * Por que: apenas 3 atribuicoes.
 */
void bw_init(BitWriter *bw, FILE *f)
{
    bw->f = f;
    bw->buffer = 0;
    bw->n = 0;
}

/*
 * bw_bit - grava 1 bit (algoritmo: acumulador de bits, MSB primeiro)
 * O que faz: desloca o buffer 1 posicao para a esquerda e coloca o novo bit
 *            na ponta direita. Quando junta 8 bits, grava o byte no arquivo.
 * Tempo: O(1). Espaco: O(1).
 * Por que: um deslocamento, um OR e um contador; a gravacao do byte
 *          acontece 1 vez a cada 8 chamadas (custo amortizado O(1)).
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
 * O que faz: se sobraram bits (n < 8), desloca-os para o inicio do byte,
 *            preenche o resto com zeros (padding) e grava.
 * Tempo: O(1). Espaco: O(1).
 * Por que: grava no maximo 1 byte. Os zeros extras sao ignorados na
 *          descompressao porque o cabecalho informa quantos bytes originais
 *          existem (o decodificador para depois de n simbolos).
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
 * O que faz: se o buffer esgotou, le o proximo byte do arquivo. Depois
 *            devolve o proximo bit, do mais significativo para o menos.
 *            Retorna -1 no fim do arquivo (usado para detectar truncamento).
 * Tempo: O(1). Espaco: O(1).
 * Por que: um deslocamento e uma mascara; o fgetc ocorre 1 vez a cada 8
 *          chamadas (custo amortizado O(1)).
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
