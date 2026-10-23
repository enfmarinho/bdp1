/*
 * DBSCAN com MPI.
 *
 * Etapas:
 * 1. O processo '0' le a amostra e envia a matriz para todos (MPI_Bcast).
 * 2. Os pontos sao divididos em fatias e cada processo calcula os vizinhos so
 *    da sua fatia.
 * 3. As listas voltam para o processo '0' (MPI_Gatherv).
 * 4. O processo '0' expande os clusters
 */

#include "dbscan.h"

#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int tem_flag(int argc, char **argv, const char *nome) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], nome) == 0) {
      return 1;
    }
  }
  return 0;
}

double arg_double(int argc, char **argv, const char *nome, double padrao) {
  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], nome) == 0) {
      return atof(argv[i + 1]);
    }
  }
  return padrao;
}

int arg_int(int argc, char **argv, const char *nome, int padrao) {
  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], nome) == 0) {
      return atoi(argv[i + 1]);
    }
  }
  return padrao;
}

const char *arg_str(int argc, char **argv, const char *nome,
                    const char *padrao) {
  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], nome) == 0) {
      return argv[i + 1];
    }
  }
  return padrao;
}

// Intervalo [inicio, fim) dos pontos deste processo.
void fatia(int n, int nprocs, int rank, int *inicio, int *fim) {
  int base = n / nprocs;
  int resto = n % nprocs;
  if (rank < resto) {
    *inicio = rank * (base + 1);
    *fim = *inicio + base + 1;
  } else {
    *inicio = resto * (base + 1) + (rank - resto) * base;
    *fim = *inicio + base;
  }
}

void deslocamentos(const int *contagens, int n, int *d) {
  d[0] = 0;
  for (int i = 1; i < n; i++) {
    d[i] = d[i - 1] + contagens[i - 1];
  }
}

double *ler_amostra(const char *caminho, int *n, int *d) {
  FILE *f = fopen(caminho, "rb");
  if (!f) {
    fprintf(stderr, "nao abri %s\n", caminho);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  int cab[2];
  if (fread(cab, sizeof(int), 2, f) != 2) {
    fprintf(stderr, "cabecalho invalido em %s\n", caminho);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  *n = cab[0];
  *d = cab[1];
  double *x = malloc((*n) * (*d) * sizeof(double));
  size_t falta = (*n) * (*d);
  if (fread(x, sizeof(double), falta, f) != falta) {
    fprintf(stderr, "arquivo %s incompleto\n", caminho);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  fclose(f);
  return x;
}

// Tres grupos num circulo pequeno e quatro pontos longe.
double *dados_teste(int *n, int *d) {
  *d = 2;
  *n = 154;
  double *X = malloc(154 * 2 * sizeof(double));
  double centros[3][2] = {{0, 0}, {5, 0}, {0, 5}};
  int i = 0;
  for (int g = 0; g < 3; g++) {
    for (int k = 0; k < 50; k++) {
      double ang = k * (2.0 * 3.141592653589793 / 50.0);
      X[i * 2] = centros[g][0] + 0.04 * cos(ang);
      X[i * 2 + 1] = centros[g][1] + 0.04 * sin(ang);
      i++;
    }
  }
  double ruido[4][2] = {{2.5, 2.5}, {-2, 2}, {3, -2}, {8, 8}};
  for (int k = 0; k < 4; k++) {
    X[i * 2] = ruido[k][0];
    X[i * 2 + 1] = ruido[k][1];
    i++;
  }
  return X;
}

void gravar_resumo(int n, double eps, int min_pts, double tempo, int n_clusters,
                   int n_ruido, const int *rotulo) {
  int *tam = calloc(n_clusters, sizeof(int));
  for (int i = 0; i < n; i++) {
    if (rotulo[i] >= 0) {
      tam[rotulo[i]]++;
    }
  }
  // os 10 maiores, selecao simples
  int top[10];
  int ntop = n_clusters < 10 ? n_clusters : 10;
  for (int k = 0; k < ntop; k++) {
    int melhor = -1;
    for (int c = 0; c < n_clusters; c++) {
      int usado = 0;
      for (int j = 0; j < k; j++) {
        if (top[j] == c) {
          usado = 1;
        }
      }
      if (!usado && (melhor < 0 || tam[c] > tam[melhor])) {
        melhor = c;
      }
    }
    top[k] = melhor;
  }

  FILE *f = fopen("resultados/resumo.json", "w");
  if (!f) {
    return;
  }
  fprintf(f, "{\n");
  fprintf(f, "  \"n\": %d,\n", n);
  fprintf(f, "  \"eps\": %.4f,\n", eps);
  fprintf(f, "  \"min_pts\": %d,\n", min_pts);
  fprintf(f, "  \"clusters\": %d,\n", n_clusters);
  fprintf(f, "  \"ruido\": %d,\n", n_ruido);
  fprintf(f, "  \"maiores\": [");
  for (int k = 0; k < ntop; k++) {
    fprintf(f, "%s%d", k ? ", " : "", tam[top[k]]);
  }
  fprintf(f, "],\n");
  fprintf(f, "  \"tempo_1proc_desta_execucao\": %.6f\n", tempo);
  fprintf(f, "}\n");
  fclose(f);
  free(tam);
}

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);

  int rank, nprocs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

  double eps = arg_double(argc, argv, "--eps", 0.022);
  int min_pts = arg_int(argc, argv, "--min-pts", 8);
  int rep = arg_int(argc, argv, "--rep", 1);
  int teste = tem_flag(argc, argv, "--teste");
  int salvar = tem_flag(argc, argv, "--salvar-rotulos");
  const char *arquivo = arg_str(argc, argv, "--arquivo", "dados/amostra.bin");

  int n = 0, d = 0;
  double *x = NULL;
  if (rank == 0) {
    if (teste) {
      x = dados_teste(&n, &d);
    } else {
      x = ler_amostra(arquivo, &n, &d);
    }
  }

  // Relogio depois da leitura. Entram Bcast, busca, juncao e expansao.
  MPI_Barrier(MPI_COMM_WORLD);
  double t0 = MPI_Wtime();

  MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&d, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if (rank != 0) {
    x = malloc(n * d * sizeof(double));
  }
  MPI_Bcast(X, n * d, MPI_DOUBLE, 0, MPI_COMM_WORLD);

  int inicio, fim;
  fatia(n, nprocs, rank, &inicio, &fim);
  int local = fim - inicio;

  int *contagens = NULL;
  int *plano = NULL;
  int nplano = 0;
  if (local > 0) {
    contagens = malloc(local * sizeof(int));
  }

  double t_busca0 = MPI_Wtime();
  if (local > 0) {
    nplano = calcular_vizinhos(x, n, d, inicio, fim, eps, contagens, &plano);
  }
  double t_busca = MPI_Wtime() - t_busca0;

  int *recv_pts = malloc(nprocs * sizeof(int));
  int *recv_plano = malloc(nprocs * sizeof(int));
  int *displs_pts = malloc(nprocs * sizeof(int));
  int *displs_plano = malloc(nprocs * sizeof(int));
  for (int r = 0; r < nprocs; r++) {
    int a, b;
    fatia(n, nprocs, r, &a, &b);
    recv_pts[r] = b - a;
  }
  deslocamentos(recv_pts, nprocs, displs_pts);
  MPI_Allgather(&nplano, 1, MPI_INT, recv_plano, 1, MPI_INT, MPI_COMM_WORLD);
  deslocamentos(recv_plano, nprocs, displs_plano);

  int total_plano = displs_plano[nprocs - 1] + recv_plano[nprocs - 1];
  int *grau = NULL;
  int *plano_todo = NULL;
  if (rank == 0) {
    grau = malloc(n * sizeof(int));
    plano_todo = malloc((total_plano > 0 ? total_plano : 1) * sizeof(int));
  }

  int dummy = 0;
  double t_junta0 = MPI_Wtime();
  MPI_Gatherv(local > 0 ? contagens : &dummy, local, MPI_INT, grau, recv_pts,
              displs_pts, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Gatherv(nplano > 0 ? plano : &dummy, nplano, MPI_INT, plano_todo,
              recv_plano, displs_plano, MPI_INT, 0, MPI_COMM_WORLD);
  double t_junta = MPI_Wtime() - t_junta0;

  int *rotulo = NULL;
  int n_clusters = 0;
  double t_exp = 0.0;
  if (rank == 0) {
    rotulo = malloc(n * sizeof(int));
    double t_exp0 = MPI_Wtime();
    n_clusters = expandir(grau, plano_todo, n, min_pts, rotulo);
    t_exp = MPI_Wtime() - t_exp0;
  }

  MPI_Barrier(MPI_COMM_WORLD);
  double tempo = MPI_Wtime() - t0;

  double t_busca_max = 0.0;
  MPI_Reduce(&t_busca, &t_busca_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

  if (rank == 0) {
    int n_ruido = 0;
    for (int i = 0; i < n; i++) {
      if (rotulo[i] < 0) {
        n_ruido++;
      }
    }
    printf("processos=%d rep=%d tempo=%.3fs busca=%.3fs juncao=%.3fs "
           "expansao=%.3fs clusters=%d ruido=%d\n",
           nprocs, rep, tempo, t_busca_max, t_junta, t_exp, n_clusters,
           n_ruido);

    if (teste) {
      if (n_clusters != 3 || n_ruido != 4) {
        fprintf(stderr, "teste paralelo falhou\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
      }
      printf("teste paralelo ok\n");
    } else {
      FILE *csv = fopen("resultados/tempos.csv", "a");
      if (csv) {
        fprintf(csv, "%d,%d,%.6f,%.6f,%.6f,%.6f,%d,%d\n", nprocs, rep, tempo,
                t_busca_max, t_junta, t_exp, n_clusters, n_ruido);
        fclose(csv);
      }
      if (salvar) {
        char nome[128];
        snprintf(nome, sizeof nome, "resultados/rotulos_p%d.bin", nprocs);
        FILE *out = fopen(nome, "wb");
        if (out) {
          fwrite(rotulo, sizeof(int), (size_t)n, out);
          fclose(out);
        }
        if (nprocs == 1) {
          gravar_resumo(n, eps, min_pts, tempo, n_clusters, n_ruido, rotulo);
        }
      }
    }
  }

  free(x);
  free(contagens);
  free(plano);
  free(recv_pts);
  free(recv_plano);
  free(displs_pts);
  free(displs_plano);
  free(grau);
  free(plano_todo);
  free(rotulo);

  MPI_Finalize();
  return 0;
}
