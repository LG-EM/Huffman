#include <stdio.h>
#include <stdlib.h>
#include "bitio.h"
#include "huffman.h"

#define NSIMB 256
#define TAM_BUF 4096

/*
 * ====================================================================
 * NOTACAO USADA NAS COMPLEXIDADES
 *
 *   n = tamanho do arquivo original, em bytes
 *   k = quantidade de bytes DISTINTOS que aparecem no arquivo (k <= 256)
 *   h = altura da arvore de Huffman (h <= k - 1 <= 255)
 *   L = comprimento medio do codigo, em bits por byte original
 *       (media ponderada pela frequencia). Em Huffman, L < H + 1, onde
 *       H e a entropia do arquivo, e H <= 8. Logo L < 9 sempre.
 */

/* ------------------------------------------------------------------ */
/* TAD Arvore de Codificacao                                          */
/* ------------------------------------------------------------------ */

typedef struct NoHuff NoHuff;
struct NoHuff {
    unsigned long freq;
    int simbolo;  /* byte (0..255) se folha; -1 se no interno */
    int ordem;    /* desempate deterministico (compressor == descompressor) */
    NoHuff *esq;
    NoHuff *dir;
};

/*
 * no_novo - aloca e inicializa um no da arvore
 */
static NoHuff *no_novo(unsigned long freq, int simbolo, int ordem,
                       NoHuff *esq, NoHuff *dir)
{
    NoHuff *n = (NoHuff *)malloc(sizeof(NoHuff));
    if (n == NULL)
        return NULL;
    n->freq = freq;
    n->simbolo = simbolo;
    n->ordem = ordem;
    n->esq = esq;
    n->dir = dir;
    return n;
}

/*
 * arvore_liberar - libera a arvore inteira (percurso em pos-ordem)
 */
static void arvore_liberar(NoHuff *n)
{
    if (n == NULL)
        return;
    arvore_liberar(n->esq);
    arvore_liberar(n->dir);
    free(n);
}

/* ------------------------------------------------------------------ */
/* TAD Fila de Prioridade (min-heap implementado manualmente)         */
/* ------------------------------------------------------------------ */

/*
 * HEAP MINIMO
 */
typedef struct {
    NoHuff *v[NSIMB];
    int n;
} Heap;

/*
 * menor - criterio de comparacao do heap
 */
static int menor(const NoHuff *a, const NoHuff *b)
{
    if (a->freq != b->freq)
        return a->freq < b->freq;
    return a->ordem < b->ordem;
}

/*
 * heap_sobe (sift-up) - restaura o heap depois de inserir no fim
 */
static void heap_sobe(Heap *h, int i)
{
    NoHuff *x = h->v[i];
    int p;

    while (i > 0) {
        p = (i - 1) / 2;
        if (!menor(x, h->v[p]))
            break;
        h->v[i] = h->v[p];
        i = p;
    }
    h->v[i] = x;
}

/*
 * heap_desce (sift-down) - restaura o heap depois de trocar a raiz
 */
static void heap_desce(Heap *h, int i)
{
    NoHuff *x = h->v[i];
    int f;

    while ((f = 2 * i + 1) < h->n) {
        if (f + 1 < h->n && menor(h->v[f + 1], h->v[f]))
            f++;
        if (!menor(h->v[f], x))
            break;
        h->v[i] = h->v[f];
        i = f;
    }
    h->v[i] = x;
}

/*
 * heap_insere - coloca o elemento no fim e chama heap_sobe
 */
static void heap_insere(Heap *h, NoHuff *x)
{
    h->v[h->n] = x;
    h->n++;
    heap_sobe(h, h->n - 1);
}

/*
 * heap_remove - retira o menor elemento (a raiz)
 */
static NoHuff *heap_remove(Heap *h)
{
    NoHuff *topo = h->v[0];

    h->n--;
    if (h->n > 0) {
        h->v[0] = h->v[h->n];
        heap_desce(h, 0);
    }
    return topo;
}

/*
 * heap_liberar - so e usada em caso de erro de memoria
 */
static void heap_liberar(Heap *h)
{
    int i;
    for (i = 0; i < h->n; i++)
        arvore_liberar(h->v[i]);
    h->n = 0;
}

/* ------------------------------------------------------------------ */
/* Construcao da arvore e geracao dos codigos                         */
/* ------------------------------------------------------------------ */

/*
 * construir_arvore - ALGORITMO DE HUFFMAN (guloso)
 */
static NoHuff *construir_arvore(const unsigned long *freq)
{
    Heap h;
    NoHuff *a, *b, *p;
    int i, prox = NSIMB;

    h.n = 0;
    for (i = 0; i < NSIMB; i++) {
        if (freq[i] > 0) {
            p = no_novo(freq[i], i, i, NULL, NULL);
            if (p == NULL) {
                heap_liberar(&h);
                return NULL;
            }
            heap_insere(&h, p);
        }
    }
    while (h.n > 1) {
        a = heap_remove(&h);
        b = heap_remove(&h);
        p = no_novo(a->freq + b->freq, -1, prox++, a, b);
        if (p == NULL) {
            arvore_liberar(a);
            arvore_liberar(b);
            heap_liberar(&h);
            return NULL;
        }
        heap_insere(&h, p);
    }
    return heap_remove(&h);
}

typedef struct {
    unsigned char bits[NSIMB]; /* profundidade maxima = 255 */
    int len;
} Codigo;

/*
 * gerar_codigos - GERACAO DOS CODIGOS (busca em profundidade)
 */
static void gerar_codigos(const NoHuff *n, unsigned char *prefixo, int prof,
                          Codigo *tab)
{
    int i;

    if (n->esq == NULL) { /* folha */
        for (i = 0; i < prof; i++)
            tab[n->simbolo].bits[i] = prefixo[i];
        tab[n->simbolo].len = prof;
        return;
    }
    prefixo[prof] = 0;
    gerar_codigos(n->esq, prefixo, prof + 1, tab);
    prefixo[prof] = 1;
    gerar_codigos(n->dir, prefixo, prof + 1, tab);
}

/* ------------------------------------------------------------------ */
/* Cabecalho: "ABCZ" | tamanho original (4 bytes) | n simbolos (2     */
/* bytes) | n x { simbolo (1 byte), frequencia (4 bytes) }            */
/* Inteiros gravados em big-endian, byte a byte (portavel).           */
/* ------------------------------------------------------------------ */

/*
 * escrever_uint - grava um inteiro sem sinal em 'nbytes' bytes, big-endian
 */
static int escrever_uint(FILE *f, unsigned long v, int nbytes)
{
    int i;
    for (i = nbytes - 1; i >= 0; i--)
        if (fputc((int)((v >> (8 * i)) & 0xFF), f) == EOF)
            return 0;
    return 1;
}

/*
 * ler_uint - operacao inversa: monta o inteiro lendo 'nbytes' bytes
 */
static int ler_uint(FILE *f, unsigned long *v, int nbytes)
{
    unsigned long r = 0;
    int i, c;

    for (i = 0; i < nbytes; i++) {
        c = fgetc(f);
        if (c == EOF)
            return 0;
        r = (r << 8) | (unsigned long)c;
    }
    *v = r;
    return 1;
}

static const unsigned char MAGIC[4] = { 'A', 'B', 'C', 'Z' };

/* ------------------------------------------------------------------ */
/* API publica                                                        */
/* ------------------------------------------------------------------ */

/*
 * huf_tamanho_arquivo - tamanho em bytes
 */
long huf_tamanho_arquivo(const char *caminho)
{
    FILE *f = fopen(caminho, "rb");
    long t;

    if (f == NULL)
        return -1;
    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    t = ftell(f);
    fclose(f);
    return t;
}

/*
 * huf_mensagem - texto do codigo de erro
 */
const char *huf_mensagem(int codigo)
{
    switch (codigo) {
    case HUF_OK:          return "Sucesso.";
    case HUF_ERR_ABRIR:   return "Nao foi possivel abrir/ler o arquivo.";
    case HUF_ERR_FORMATO: return "Arquivo comprimido invalido ou corrompido.";
    case HUF_ERR_MEM:     return "Memoria insuficiente.";
    case HUF_ERR_ESCRITA: return "Falha ao gravar o arquivo de saida.";
    }
    return "Erro desconhecido.";
}

/*
 * huf_comprimir - COMPRESSAO (Huffman de caracteres, 2 leituras do arquivo)
 *
 * Etapas e custos:
 *   1. Analise de frequencia ........ O(n)
 *   2. Cabecalho .................... O(k)
 *   3. Arvore (construir_arvore) .... O(k log k)
 *   4. Codigos (gerar_codigos) ...... O(k * h)
 *   5. Codificacao .................. O(n * L)  = O(n), pois L < 9
 */
int huf_comprimir(const char *entrada, const char *saida)
{
    FILE *in, *out;
    unsigned long freq[NSIMB];
    unsigned long total = 0;
    unsigned char buf[TAM_BUF];
    unsigned char prefixo[NSIMB];
    static Codigo tab[NSIMB];
    NoHuff *raiz;
    BitWriter bw;
    size_t lidos, k;
    int i, j, nsimb = 0, ret = HUF_OK;

    in = fopen(entrada, "rb");
    if (in == NULL)
        return HUF_ERR_ABRIR;

    /*
     * Etapa 1: analise de frequencia - O(n)
     */
    for (i = 0; i < NSIMB; i++)
        freq[i] = 0;
    while ((lidos = fread(buf, 1, TAM_BUF, in)) > 0) {
        for (k = 0; k < lidos; k++)
            freq[buf[k]]++;
        total += (unsigned long)lidos;
    }
    if (ferror(in)) {
        fclose(in);
        return HUF_ERR_ABRIR;
    }
    for (i = 0; i < NSIMB; i++)
        if (freq[i] > 0)
            nsimb++;

    out = fopen(saida, "wb");
    if (out == NULL) {
        fclose(in);
        return HUF_ERR_ESCRITA;
    }

    /*
     * Cabecalho - O(k) (percorre 256 posicoes e grava k entradas de 5 bytes)
     */
    for (i = 0; i < 4; i++)
        if (fputc(MAGIC[i], out) == EOF)
            ret = HUF_ERR_ESCRITA;
    if (!escrever_uint(out, total, 4) || !escrever_uint(out, (unsigned long)nsimb, 2))
        ret = HUF_ERR_ESCRITA;
    for (i = 0; i < NSIMB && ret == HUF_OK; i++) {
        if (freq[i] > 0) {
            if (fputc(i, out) == EOF || !escrever_uint(out, freq[i], 4))
                ret = HUF_ERR_ESCRITA;
        }
    }
    if (ret != HUF_OK || total == 0) { /* arquivo vazio: so cabecalho */
        fclose(in);
        if (fclose(out) != 0 && ret == HUF_OK)
            ret = HUF_ERR_ESCRITA;
        return ret;
    }

    /* Etapas 2 e 3: arvore O(k log k) e codigos O(k*h) */
    raiz = construir_arvore(freq);
    if (raiz == NULL) {
        fclose(in);
        fclose(out);
        return HUF_ERR_MEM;
    }

    /* Um unico simbolo distinto: o cabecalho ja basta (nenhum bit gravado) */
    if (raiz->esq != NULL) {
        for (i = 0; i < NSIMB; i++)
            tab[i].len = 0;
        gerar_codigos(raiz, prefixo, 0, tab);

        /*
         * Etapa 4: segunda leitura e codificacao - O(n * L) = O(n)
         */
        rewind(in);
        bw_init(&bw, out);
        while (ret == HUF_OK && (lidos = fread(buf, 1, TAM_BUF, in)) > 0) {
            for (k = 0; k < lidos && ret == HUF_OK; k++) {
                const Codigo *c = &tab[buf[k]];
                for (j = 0; j < c->len; j++)
                    if (!bw_bit(&bw, c->bits[j])) {
                        ret = HUF_ERR_ESCRITA;
                        break;
                    }
            }
        }
        if (ret == HUF_OK && !bw_flush(&bw))
            ret = HUF_ERR_ESCRITA;
    }

    arvore_liberar(raiz);
    fclose(in);
    if (fclose(out) != 0 && ret == HUF_OK)
        ret = HUF_ERR_ESCRITA;
    return ret;
}

/*
 * huf_descomprimir - DESCOMPRESSAO (le o cabecalho, refaz a arvore, decodifica)
 *
 * Etapas e custos:
 *   1. Ler e validar cabecalho ...... O(k)
 *   2. Reconstruir a arvore ......... O(k log k)
 *   3. Decodificar os bits .......... O(n * L) = O(n)
 */
int huf_descomprimir(const char *entrada, const char *saida)
{
    FILE *in, *out;
    unsigned long freq[NSIMB];
    unsigned long total, nsimb, f, soma = 0, i;
    int c, k, ret = HUF_OK, s = 0, b;
    NoHuff *raiz, *n;
    BitReader br;

    in = fopen(entrada, "rb");
    if (in == NULL)
        return HUF_ERR_ABRIR;

    /*
     * Etapa 1: cabecalho - O(k)
     */
    for (k = 0; k < 4; k++) {
        c = fgetc(in);
        if (c != (int)MAGIC[k]) {
            fclose(in);
            return HUF_ERR_FORMATO;
        }
    }
    if (!ler_uint(in, &total, 4) || !ler_uint(in, &nsimb, 2) || nsimb > NSIMB) {
        fclose(in);
        return HUF_ERR_FORMATO;
    }
    for (k = 0; k < NSIMB; k++)
        freq[k] = 0;
    for (i = 0; i < nsimb; i++) {
        c = fgetc(in);
        if (c == EOF || !ler_uint(in, &f, 4) || f == 0 || freq[c] != 0) {
            fclose(in);
            return HUF_ERR_FORMATO;
        }
        freq[c] = f;
        soma += f;
        s = c;
    }
    if (soma != total || (total > 0 && nsimb == 0)) {
        fclose(in);
        return HUF_ERR_FORMATO;
    }

    out = fopen(saida, "wb");
    if (out == NULL) {
        fclose(in);
        return HUF_ERR_ESCRITA;
    }
    if (total == 0) {
        fclose(in);
        if (fclose(out) != 0)
            return HUF_ERR_ESCRITA;
        return HUF_OK;
    }

    /*
     * Etapa 2: reconstrucao do modelo a partir do cabecalho - O(k log k)
     */
    raiz = construir_arvore(freq);
    if (raiz == NULL) {
        fclose(in);
        fclose(out);
        return HUF_ERR_MEM;
    }

    if (raiz->esq == NULL) { /* um unico simbolo repetido 'total' vezes */
        for (i = 0; i < total && ret == HUF_OK; i++)
            if (fputc(s, out) == EOF)
                ret = HUF_ERR_ESCRITA;
    } else {
        /*
         * Etapa 3: decodificacao - O(n * L) = O(n)
         */
        br_init(&br, in);
        for (i = 0; i < total && ret == HUF_OK; i++) {
            n = raiz;
            while (n->esq != NULL) {
                b = br_bit(&br);
                if (b < 0) {
                    ret = HUF_ERR_FORMATO; /* arquivo truncado */
                    break;
                }
                n = b ? n->dir : n->esq;
            }
            if (ret == HUF_OK && fputc(n->simbolo, out) == EOF)
                ret = HUF_ERR_ESCRITA;
        }
    }

    arvore_liberar(raiz);
    fclose(in);
    if (fclose(out) != 0 && ret == HUF_OK)
        ret = HUF_ERR_ESCRITA;
    return ret;
}
