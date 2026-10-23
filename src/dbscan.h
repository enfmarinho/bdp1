#ifndef DBSCAN_H
#define DBSCAN_H

/*
 * Busca de vizinhos e expansao do DBSCAN.
 */

/*
 * Vizinhos dos pontos [inicio, fim).
 */
int calcular_vizinhos(const double *X, int n, int d, int inicio, int fim,
                      double eps, int *contagens, int **plano);

/*
 * grau[i] é o tamanho da lista do ponto i.
 * plano é  a concatenacao dessas listas, na ordem 0..n-1.
 * Retorna a quantidade de clusters ou -1 se ruido.
 */
int expandir(const int *grau, const int *plano, int n, int min_pts,
             int *rotulo);

#endif
