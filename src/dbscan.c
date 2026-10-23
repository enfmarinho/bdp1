#include "dbscan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int calcular_vizinhos(const double *x, int n, int d, int inicio, int fim,
                      double eps, int *contagens, int **plano_out) {
  double eps2 = eps * eps;
  int local = fim - inicio;
  int *tmp = malloc(n * sizeof(int));

  size_t cap = local * 16;
  if (cap < 16) {
    cap = 16;
  }
  int *plano = malloc(cap * sizeof(int));
  int nplano = 0;

  for (int k = 0; k < local; k++) {
    int i = inicio + k;
    const double *pi = x + i * d;
    int c = 0;

    for (int j = 0; j < n; j++) {
      const double *pj = x + j * d;
      double s = 0.0;
      for (int t = 0; t < d; t++) {
        double df = pi[t] - pj[t];
        s += df * df;
      }
      if (s <= eps2) {
        tmp[c++] = j;
      }
    }

    if (nplano + c > cap) {
      while (nplano + c > cap) {
        cap *= 2;
      }
      plano = realloc(plano, cap * sizeof(int));
    }
    memcpy(plano + nplano, tmp, c * sizeof(int));
    nplano += c;
    contagens[k] = c;

    if (local >= 4000 && k > 0 && k % 4000 == 0) {
      printf("  vizinhos: %d/%d\n", k, local);
      fflush(stdout);
    }
  }

  free(tmp);
  *plano_out = plano;
  return nplano;
}

int expandir(const int *grau, const int *plano, int n, int min_pts,
             int *rotulo) {
  int *off = malloc((n + 1) * sizeof(int));
  off[0] = 0;
  for (int i = 0; i < n; i++) {
    off[i + 1] = off[i] + grau[i];
    rotulo[i] = -1;
  }

  int *fila = malloc(n * sizeof(int));
  unsigned char *viu = calloc(n, 1);

  int cluster = 0;
  for (int i = 0; i < n; i++) {
    if (rotulo[i] != -1) {
      continue;
    }
    if (grau[i] < min_pts) {
      continue;
    }

    rotulo[i] = cluster;
    viu[i] = 1;
    int head = 0;
    int tail = 0;

    for (int t = off[i]; t < off[i + 1]; t++) {
      int p = plano[t];
      if (!viu[p]) {
        viu[p] = 1;
        fila[tail++] = p;
      }
    }

    while (head < tail) {
      int q = fila[head++];
      if (rotulo[q] == -1) {
        rotulo[q] = cluster;
      }
      if (grau[q] < min_pts) {
        continue;
      }
      for (int t = off[q]; t < off[q + 1]; t++) {
        int p = plano[t];
        if (!viu[p]) {
          viu[p] = 1;
          fila[tail++] = p;
        }
      }
    }
    cluster++;
  }

  free(off);
  free(fila);
  free(viu);
  return cluster;
}
