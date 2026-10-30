#!/bin/bash
#SBATCH --job-name=dbscan_mpi
#SBATCH --output=slurm_dbscan_%j.out
#SBATCH --error=slurm_dbscan_%j.err
#SBATCH --time=0:30:0
#SBATCH --cpus-per-task=1
#SBATCH --nodes=16
#SBATCH --tasks-per-node=32
#SBATCH --partition=intel-128

export OMPI_MCA_btl=^openib

mkdir -p resultados

echo "=== Execucao Principal e Benchmark de Escalabilidade ==="
ARQUIVO="dados/amostra.bin"
EPS="0.035"
MIN_PTS="30"
REPETICOES=3

# testa a escalabilidade com diferentes números de processos
for P in 1 2 4 8 16 32 64 128 256 512; do
    echo "Iniciando testes com $P processos..."
    
    for REP in $(seq 1 $REPETICOES); do
        echo "  Repeticao $REP/$REPETICOES"

        srun -n $P ./dbscan_mpi --arquivo "$ARQUIVO" --eps "$EPS" --min-pts "$MIN_PTS" --rep "$REP" --salvar-rotulos
    done
done

echo "Benchmark finalizado!"
