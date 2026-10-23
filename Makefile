CC = mpicc
CFLAGS = -O2 -Wall -Wextra -std=c11

dbscan_mpi: src/dbscan.c src/dbscan_mpi.c src/dbscan.h
	$(CC) $(CFLAGS) -o dbscan_mpi src/dbscan.c src/dbscan_mpi.c -lm

clean:
	rm -f dbscan_mpi
