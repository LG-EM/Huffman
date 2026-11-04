#ifndef HUFFMAN_H
#define HUFFMAN_H

/* Codigos de retorno */
#define HUF_OK          0
#define HUF_ERR_ABRIR   1  /* nao foi possivel abrir/ler arquivo */
#define HUF_ERR_FORMATO 2  /* arquivo comprimido invalido/corrompido */
#define HUF_ERR_MEM     3  /* falha de alocacao */
#define HUF_ERR_ESCRITA 4  /* falha ao gravar saida */

/* Comprime 'entrada' em 'saida' (Huffman de caracteres/bytes). */
int huf_comprimir(const char *entrada, const char *saida);

/* Restaura 'entrada' (.abcz) em 'saida'. */
int huf_descomprimir(const char *entrada, const char *saida);

/* Tamanho do arquivo em bytes; -1 em caso de erro. */
long huf_tamanho_arquivo(const char *caminho);

/* Mensagem de texto para um codigo de retorno. */
const char *huf_mensagem(int codigo);

#endif
