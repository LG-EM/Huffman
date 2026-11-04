#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "huffman.h"

#define MAX_CAM 512
#define EXT ".abcz"


/*
 * ler_linha - le uma linha do teclado e remove o '\n' final
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
