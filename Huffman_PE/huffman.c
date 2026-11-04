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
 *
 * Como k <= 256 e limitado por uma constante, tudo que depende so de k
 * (arvore, heap, tabela de codigos) e O(1) em relacao a n. Mesmo assim
 * as formulas mostram k para deixar claro de onde cada custo vem.
 *
 * Resumo do programa:
 *   Compressao:    Tempo O(n)  |  Espaco O(k) = O(1) extra (nao carrega o arquivo)
 *   Descompressao: Tempo O(n)  |  Espaco O(k) = O(1) extra
 * ====================================================================
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
 * Tempo: O(1). Espaco: O(1) (um unico no).
 * Por que: um malloc e 5 atribuicoes. Devolve NULL se faltar memoria.
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
 * O que faz: libera filho esquerdo, filho direito e so entao o proprio no.
 * Tempo: O(k). Espaco: O(h) de pilha de recursao.
 * Por que: cada no e visitado exatamente 1 vez, e a arvore de Huffman com
 *          k folhas tem 2k-1 nos (todo no interno tem exatamente 2 filhos).
 *          A recursao empilha no maximo h chamadas ao mesmo tempo.
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
 * Arvore binaria COMPLETA guardada num vetor: o pai de i e (i-1)/2 e os
 * filhos de i sao 2i+1 e 2i+2. Propriedade: o pai e sempre "menor" que os
 * filhos, entao o menor elemento fica em v[0].
 * Como e completa, sua altura e floor(log2(n)): e isso que torna inserir e
 * remover O(log n). Capacidade 256 basta: nunca ha mais que k <= 256 nos
 * na fila ao mesmo tempo (cada fusao tira 2 e coloca 1).
 */
typedef struct {
    NoHuff *v[NSIMB];
    int n;
} Heap;

/*
 * menor - criterio de comparacao do heap
 * O que faz: compara por frequencia; se empatar, compara por 'ordem'.
 * Tempo: O(1). Espaco: O(1).
 * Por que o desempate: sem ele, nos de mesma frequencia poderiam sair em
 *          ordem diferente no compressor e no descompressor, gerando
 *          arvores diferentes e quebrando a decodificacao.
 */
static int menor(const NoHuff *a, const NoHuff *b)
{
    if (a->freq != b->freq)
        return a->freq < b->freq;
    return a->ordem < b->ordem;
}

/*
 * heap_sobe (sift-up) - restaura o heap depois de inserir no fim
 * O que faz: enquanto o elemento for menor que o pai, o pai desce uma
 *            posicao e o elemento sobe.
 * Tempo: O(log k). Espaco: O(1).
 * Por que: cada iteracao sobe 1 nivel com trabalho constante, e a altura do
 *          heap e floor(log2 n). No pior caso sobe da folha ate a raiz.
 *          Nao usa recursao nem vetor auxiliar (so a variavel x).
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
 * O que faz: compara o elemento com o MENOR dos dois filhos; se o filho for
 *            menor, ele sobe e o elemento desce uma posicao. Repete.
 * Tempo: O(log k). Espaco: O(1).
 * Por que: cada iteracao desce 1 nivel (2 comparacoes = trabalho constante)
 *          e no maximo floor(log2 n) niveis existem. Iterativo, sem pilha.
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
 * Tempo: O(log k). Espaco: O(1).
 * Por que: colocar no fim e O(1); heap_sobe e O(log k).
 */
static void heap_insere(Heap *h, NoHuff *x)
{
    h->v[h->n] = x;
    h->n++;
    heap_sobe(h, h->n - 1);
}

/*
 * heap_remove - retira o menor elemento (a raiz)
 * O que faz: guarda a raiz, move o ultimo elemento para o topo e chama
 *            heap_desce.
 * Tempo: O(log k). Espaco: O(1).
 * Por que: trocar e O(1); heap_desce e O(log k).
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
 * O que faz: libera as arvores que ainda estao dentro do heap.
 * Tempo: O(total de nos restantes) <= O(k). Espaco: O(h) (recursao).
 * Por que: cada no restante e liberado 1 vez por arvore_liberar.
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
 * O que faz:
 *   1. cria uma folha para cada byte com freq > 0 e poe no heap;
 *   2. enquanto houver 2 ou mais nos: retira os 2 de menor frequencia,
 *      cria um pai com a soma delas (1o retirado = filho esquerdo = bit 0,
 *      2o retirado = filho direito = bit 1) e devolve o pai ao heap;
 *   3. o ultimo no que sobra e a raiz.
 * Por que gera codigo otimo: juntar sempre os 2 menores faz os simbolos
 *   raros ficarem mais fundo (codigos longos) e os frequentes mais perto
 *   da raiz (codigos curtos), minimizando o total de bits.
 * Desempate: folhas usam ordem = byte (0..255); nos internos usam 256, 257,
 *   ... na ordem de criacao (variavel 'prox'). Assim o resultado e sempre
 *   o mesmo para as mesmas frequencias.
 *
 * Tempo: O(k log k).
 *   Por que: passo 1 faz k insercoes de custo O(log k); passo 2 repete
 *   k-1 vezes (cada fusao reduz o numero de nos em 1) 2 remocoes + 1
 *   insercao, cada uma O(log k).
 * Espaco: O(k).
 *   Por que: a arvore final tem 2k-1 nos; o heap e um vetor fixo de 256
 *   ponteiros.
 *
 * Requer pelo menos um simbolo com freq > 0. NULL = falta de memoria.
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
 * O que faz: percorre a arvore da raiz as folhas carregando o caminho em
 *            prefixo[]: descer a esquerda grava 0, descer a direita grava 1.
 *            Ao chegar numa folha, copia o caminho para tab[simbolo].
 *            Resultado: codigos prefixados (nenhum e inicio de outro, pois
 *            simbolos ficam so nas folhas), por isso a decodificacao e
 *            nao ambigua.
 *
 * Tempo: O(k * h) no pior caso.
 *   Por que: a DFS visita os 2k-1 nos (O(k)) e em cada uma das k folhas
 *   copia 'prof' bits (o tamanho do codigo). Soma = soma das profundidades
 *   das folhas <= k * h.
 *     - arvore balanceada (h ~ log k): O(k log k);
 *     - arvore "em pente" (h = k-1, frequencias tipo Fibonacci): O(k^2).
 *   Com k <= 256 sao no maximo ~32 mil operacoes: irrelevante frente a n.
 * Espaco: O(h) de pilha de recursao; prefixo[] e tab[] sao fixos
 *   (256 + 256*260 bytes).
 * Por que h <= 255: uma arvore com k <= 256 folhas tem altura <= k-1, por
 *   isso 256 posicoes bastam em prefixo[] e em bits[].
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
 * O que faz: extrai cada byte com deslocamento (v >> 8*i) e mascara 0xFF,
 *            do mais significativo para o menos.
 * Tempo: O(nbytes) = O(1) (nbytes e 2 ou 4). Espaco: O(1).
 * Por que byte a byte: o arquivo fica igual em qualquer maquina (nao
 *            depende de endianness nem do tamanho de unsigned long).
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
 * Tempo: O(nbytes) = O(1). Espaco: O(1).
 * Por que: a cada byte lido, desloca o acumulado 8 bits e soma o byte.
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
 * O que faz: abre o arquivo, vai ate o fim com fseek e le a posicao (ftell).
 * Tempo: O(1) em relacao a n. Espaco: O(1).
 * Por que: nao le o conteudo do arquivo; so posiciona o cursor no fim.
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
 * Tempo: O(1). Espaco: O(1). Por que: um switch com poucos casos.
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
 *
 * Tempo total: O(n + k*h) = O(n), porque k*h <= 256*255 e uma constante.
 *   O custo domina nas etapas 1 e 5 (ler o arquivo 2 vezes e gravar os bits).
 * Espaco: O(k) = O(1) em relacao a n.
 *   Por que: o arquivo NUNCA e carregado inteiro; e lido em blocos de 4096
 *   bytes (buf). Alem do bloco, ha freq[256], a arvore (2k-1 nos), prefixo[256]
 *   e tab[256] (estatica para nao ocupar ~64 KB da pilha). Nada cresce com n.
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
     * Cada byte do arquivo e lido uma vez e freq[byte]++ e acesso direto a
     * vetor (O(1)). Ler em blocos de 4096 com fread evita 1 chamada por byte.
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
     * Tamanho em disco: 10 + 5*k bytes.
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
         * Para cada byte do arquivo, grava os bits do seu codigo (tab[byte]).
         * O total de bits gravados e a soma de freq[b] * len(b) = n * L.
         * Como L < 9 (menos que H + 1, com H <= 8), isso e linear em n.
         * Cada bw_bit e O(1).
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
 *
 * Tempo total: O(n + k log k) = O(n), pois k <= 256.
 * Espaco: O(k) = O(1) em relacao a n.
 *   Por que: os bits sao lidos um a um (BitReader) e cada byte restaurado
 *   e gravado na hora; o arquivo nunca e carregado inteiro. So existem
 *   freq[256] e a arvore de 2k-1 nos.
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
     * Valida: assinatura "ABCZ", nsimb <= 256, nenhuma frequencia zero, nenhum
     * simbolo repetido (freq[c] != 0 detecta em O(1) pois freq e indexado
     * pelo byte) e soma das frequencias == tamanho total.
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
     * Mesmo algoritmo e mesmo desempate do compressor => arvore identica.
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
         * Para cada byte a restaurar, parte da raiz e le bits: 0 = esquerda,
         * 1 = direita, ate chegar numa folha. Cada passo e 1 br_bit O(1).
         * Os passos de um byte = tamanho do seu codigo; somando os n bytes,
         * o total de passos e igual ao total de bits gravados = n * L.
         * Se o arquivo acabar no meio (br_bit = -1), esta truncado.
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
