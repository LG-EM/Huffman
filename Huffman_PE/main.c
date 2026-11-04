#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "huffman.h"

#define MAX_CAM 512
#define EXT ".abcz"

/*
 * Nesta parte, m = tamanho do texto digitado (no maximo MAX_CAM - 1 = 511).
 * n = tamanho do arquivo, como em huffman.c.
 */

/*
 * ler_linha - le uma linha do teclado e remove o '\n' final
 * O que faz: fgets le ate MAX_CAM-1 caracteres (nunca estoura o vetor);
 *            depois um laco troca o primeiro '\n' ou '\r' por '\0'.
 * Tempo: O(m). Espaco: O(1).
 * Por que: um unico percurso pelo texto lido, no maximo 511 posicoes.
 * Retorna 0 em EOF.
 */
static int ler_linha(char *dest, int max)
{
    int i;

    if (fgets(dest, max, stdin) == NULL)
        return 0;
    for (i = 0; dest[i] != '\0'; i++) {
        if (dest[i] == '\n' || dest[i] == '\r') {
            dest[i] = '\0';
            break;
        }
    }
    return 1;
}

/*
 * iguais - substitui strcmp (string.h e proibida no enunciado)
 * O que faz: avanca enquanto os caracteres forem iguais e nao acabar o texto;
 *            no fim, os textos sao iguais se ambos terminaram juntos.
 * Tempo: O(m) no pior caso (textos iguais). Espaco: O(1).
 * Por que: para no primeiro caractere diferente; no maximo percorre o menor
 *          dos dois textos.
 */
static int iguais(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/*
 * concatenar - substitui strcat (sem string.h)
 * O que faz: copia 'base' e depois 'suf' para 'dest', verificando o limite.
 * Tempo: O(|base| + |suf|). Espaco: O(1) (escreve direto em dest).
 * Por que: cada caractere e copiado exatamente 1 vez.
 * Retorna 0 se nao couber.
 */
static int concatenar(char *dest, int max, const char *base, const char *suf)
{
    int i = 0;

    while (*base != '\0') {
        if (i >= max - 1)
            return 0;
        dest[i++] = *base++;
    }
    while (*suf != '\0') {
        if (i >= max - 1)
            return 0;
        dest[i++] = *suf++;
    }
    dest[i] = '\0';
    return 1;
}

/*
 * comprimir - opcao 1 do menu
 * O que faz: pede o arquivo, gera o nome de saida (entrada + ".abcz", o
 *            original nunca e alterado), chama huf_comprimir medindo o tempo
 *            com clock() e mostra tamanhos, razao R = Sc / So e tempo.
 * Tempo: O(n), dominado por huf_comprimir (o resto e O(m)).
 * Espaco: O(1) (dois vetores de 512 caracteres).
 * Obs.: clock() mede tempo de CPU do programa, nao tempo de relogio.
 */
static void comprimir(void)
{
    char entrada[MAX_CAM], saida[MAX_CAM];
    long so, sc;
    clock_t t0, t1;
    int r;

    printf("Arquivo a comprimir: ");
    if (!ler_linha(entrada, MAX_CAM) || entrada[0] == '\0')
        return;
    if (!concatenar(saida, MAX_CAM, entrada, EXT)) {
        printf("Caminho muito longo.\n");
        return;
    }

    t0 = clock();
    r = huf_comprimir(entrada, saida);
    t1 = clock();
    if (r != HUF_OK) {
        printf("Erro: %s\n", huf_mensagem(r));
        return;
    }

    so = huf_tamanho_arquivo(entrada);
    sc = huf_tamanho_arquivo(saida);
    printf("\nArquivo original: %s Tamanho: %ld Bytes\n", entrada, so);
    printf("Arquivo comprimido: %s Tamanho: %ld Bytes\n", saida, sc);
    if (so > 0)
        printf("Razao de compressao: %.4f\n", (double)sc / (double)so);
    else
        printf("Razao de compressao: indefinida (arquivo vazio)\n");
    printf("Tempo de compressao: %.3f s\n", (double)(t1 - t0) / CLOCKS_PER_SEC);
}

/*
 * descomprimir - opcao 2 do menu
 * O que faz: pede o .abcz e o nome do arquivo restaurado, recusa nomes
 *            iguais, chama huf_descomprimir medindo o tempo e mostra tamanhos.
 * Tempo: O(n), dominado por huf_descomprimir. Espaco: O(1).
 */
static void descomprimir(void)
{
    char entrada[MAX_CAM], saida[MAX_CAM];
    long sc, so;
    clock_t t0, t1;
    int r;

    printf("Arquivo comprimido (%s): ", EXT);
    if (!ler_linha(entrada, MAX_CAM) || entrada[0] == '\0')
        return;
    printf("Nome do arquivo restaurado: ");
    if (!ler_linha(saida, MAX_CAM) || saida[0] == '\0')
        return;
    if (iguais(entrada, saida)) {
        printf("A saida nao pode ter o mesmo nome da entrada.\n");
        return;
    }

    t0 = clock();
    r = huf_descomprimir(entrada, saida);
    t1 = clock();
    if (r != HUF_OK) {
        printf("Erro: %s\n", huf_mensagem(r));
        return;
    }

    sc = huf_tamanho_arquivo(entrada);
    so = huf_tamanho_arquivo(saida);
    printf("\nArquivo comprimido: %s Tamanho: %ld Bytes\n", entrada, sc);
    printf("Arquivo restaurado: %s Tamanho: %ld Bytes\n", saida, so);
    printf("Tempo de descompressao: %.3f s\n", (double)(t1 - t0) / CLOCKS_PER_SEC);
}

/*
 * main - laco do menu
 * Tempo: O(1) por opcao escolhida (alem do custo da operacao chamada).
 * Espaco: O(1).
 */
int main(void)
{
    char op[MAX_CAM];

    for (;;) {
        printf("\nTrabalho de implementacao - Huffman\n");
        printf("1) Comprimir\n");
        printf("2) Descomprimir\n");
        printf("3) Sair\n");
        printf("Opcao: ");
        if (!ler_linha(op, MAX_CAM))
            break;
        if (op[0] == '1')
            comprimir();
        else if (op[0] == '2')
            descomprimir();
        else if (op[0] == '3')
            break;
        else
            printf("Opcao invalida.\n");
    }
    return 0;
}
