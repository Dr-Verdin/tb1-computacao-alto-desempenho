#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "output.h"
#include "simulation.h"

int main(int argc, char *argv[]) {
	Simulation sim;

    if (argc != 2) {
        printf("Uso: %s <arquivo_entrada>\n", argv[0]);
        return 1;
    }

    if (read_input(argv[1], &sim) != 0) {
        printf("ERRO: falha ao ler o arquivo de entrada.\n");
        return 1;
    }

	Result res;
	calculate_percentages(&sim, &res);

	printf("%lld\n", res.intactas);

	free_simulation(&sim);
	return 0;
}