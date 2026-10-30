# Parallel DBSCAN: Spatio-Temporal Analysis of Tweets

This project implements a parallelized DBSCAN algorithm in C using **MPI** to cluster spatio-temporal data (Longitude, Latitude, and Time) from a Twitter dataset. The workflow includes data sampling, distributed processing on a cluster via **Slurm**, and result visualization through GIF animations.

## Prerequisites
Ensure you have the following packages installed in your environment:
- C Compiler and OpenMPI (`mpicc`)
- Slurm workload manager
- Python 3.10+
- Python Libraries: `numpy`, `pandas`, `matplotlib`

---

## Step-by-Step Execution

### 1. Get the Dataset

*The file should be located at: `dados/twitter.csv`*

### 2. Pre-processing

```bash
python3 src/preprocessamento.py
```

**Outputs generated in the `dados/` folder:**

* `amostra.bin`: Normalized 3D matrix for the C algorithm.
* `amostra_original.csv`: Exact copy of the data with raw text and timezones for plotting.
* `info.txt`: Diagnostic log.

### 3. MPI Code Compilation

Compile the C source code using

```bash
make
```

### 4. Cluster Execution (Slurm)

Submit the job to the cluster. 

```bash
sbatch job_dbscan.sh
```

### 5. Animation Generation (Plotting)

```bash
# Replace rotulos_p8.bin with the file corresponding to the number of processes used
python3 analise/plot_tweet_analisys.py dados/amostra_original.csv --rotulos resultados/rotulos_p8.bin -o resultados/animacao_dbscan.gif
```