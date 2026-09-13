#include <stdio.h>
#include <stdlib.h>

/* Programa principal (runner) para a versão sequencial.
 * Fluxo resumido: ler entrada, executar simulação, calcular resultados finais
 * (checksum e percentuais) e imprimir saída.
 */
#include "input.h"
#include "output.h"
#include "simulation.h"
#include "simulation_seq.c"

int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stderr, "Uso: %s <arquivo_entrada>\n", argv[0]);
        return 1;
    }

    // Aloca estruturas de simulação e resultados
    Simulation* s = (Simulation*)malloc(sizeof(Simulation));
    Result* res = (Result*)malloc(sizeof(Result));

    // Lê e valida o arquivo de entrada (preenche `s`)
    if (read_input(argv[1], s) != 0) {
        fprintf(stderr, "Erro ao ler o arquivo de entrada\n");
        return 1;
    }

    // Executa a simulação sequencial
    Simulate_sequential(s, res);

    // Pós-processamento e impressão dos resultados finais
    calculate_checksum(s, res);
    calculate_percentages(s, res);
    print_results(res);

    // Libera memória alocada
    free_simulation(s);
    free(s);
    free(res);
    
    return 0;
}
