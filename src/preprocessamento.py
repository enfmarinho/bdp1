"""
Le o twitter.csv inteiro e grava uma amostra normalizada.

O arquivo tem 14 milhoes de tweets. O nosso DBSCAN compara
pares de pontos, entao rodar nos 14 milhoes é inviavel. 
Pegamos uma amostra aleatoria

"""

import random
import csv
from pathlib import Path

import numpy as np

RAIZ = Path(__file__).resolve().parents[1]
CSV = RAIZ / "dados" / "twitter.csv"
N_AMOSTRA = 25000
SEMENTE = 1234

def timestamp_para_segundos(valor):
    """
    converte YYYYMMDDHHMMSS para segundos desde 2013-01-12 00:00:00.
    """

    seg = valor % 100
    valor //= 100
    minuto = valor % 100
    valor //= 100
    hora = valor % 100
    valor //= 100
    dia = valor % 100
    return (dia - 12) * 86400 + hora * 3600 + minuto * 60 + seg


def main():
    random.seed(SEMENTE)
    amostra = []
    amostra_texto = []
    mins = np.array([np.inf, np.inf, np.inf])
    maxs = np.array([-np.inf, -np.inf, -np.inf])
    
    print(f"lendo {CSV}", flush=True)
    with open(CSV) as f:
        f.readline()
        total = 0
        for linha in f:
            partes = linha.split(",")
            if len(partes) < 4:
                continue
            lon = float(partes[0])
            lat = float(partes[1])
            seg = float(timestamp_para_segundos(int(partes[2])))
            tz = int(partes[3])

            ponto = np.array([lon, lat, seg], dtype=np.float64)
            linha_txt = [partes[0], partes[1], partes[2], partes[3].strip()]

            np.minimum(mins, ponto, out=mins)
            np.maximum(maxs, ponto, out=maxs)

            total += 1
            if len(amostra) < N_AMOSTRA:
                amostra.append(ponto)
                amostra_texto.append(linha_txt)
            else:
                j = random.randrange(total)
                if j < N_AMOSTRA:
                    amostra[j] = ponto
                    amostra_texto[j] = linha_txt

    dados = np.vstack(amostra)
    escala = maxs - mins
    escala[escala == 0] = 1.0
    norm = (dados - mins) / escala

    pasta = RAIZ / "dados"
    np.save(pasta / "amostra_norm.npy", norm)
    
    with open(pasta / "amostra.bin", "wb") as f:
        np.array([norm.shape[0], norm.shape[1]], dtype=np.int32).tofile(f)
        np.ascontiguousarray(norm, dtype=np.float64).tofile(f)

    # Exporta o CSV original que será usado pelo script do GIF
    caminho_csv = pasta / "amostra_original.csv"
    with open(caminho_csv, "w", newline="") as f_csv:
        writer = csv.writer(f_csv)
        writer.writerow(["Longitude", "Latitude", "Timestamp", "Timezone"])
        writer.writerows(amostra_texto)
    
    print(f"Pré-processamento concluído! CSV salvo em: {caminho_csv}")

    info = pasta / "info.txt"
    with open(info, "w") as f:
        f.write(f"total={total}\n")
        f.write(f"amostra={N_AMOSTRA}\n")
        f.write(f"semente={SEMENTE}\n")
        f.write(f"min_lon={mins[0]:.8f}\n")
        f.write(f"min_lat={mins[1]:.8f}\n")
        f.write(f"min_seg={mins[2]:.1f}\n")
        f.write(f"max_lon={maxs[0]:.8f}\n")
        f.write(f"max_lat={maxs[1]:.8f}\n")
        f.write(f"max_seg={maxs[2]:.1f}\n")

    print(f"total de tweets: {total}")
    print(f"amostra: {N_AMOSTRA}")
    print(f"min {mins}")
    print(f"max {maxs}")
    print(f"gravado em {pasta}")


if __name__ == "__main__":
    main()
