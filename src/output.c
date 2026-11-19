#include <stdio.h>
#include <stdlib.h>

#include "output.h"

// Calcula o checksum da configuração final, percorrendo as células em ordem linear.
void calculate_checksum(const Simulation *simulation, Result *result){
    unsigned long long checksum = 0;
    long long total_cells = (long long)simulation->config.L * simulation->config.C;

    // Incorpora o estado e o tempo de cada célula, na ordem linear da matriz.
    for (long long i = 0; i < total_cells; i++) {
        checksum = checksum * 31ULL + (unsigned long long)simulation->estado_atual[i];
        checksum = checksum * 31ULL + (unsigned long long)simulation->tempo_atual[i];
    }

    result->checksum = checksum;
}

// Calcula os percentuais da área inicialmente combustível que foi queimada e protegida.
void calculate_percentages(const Simulation *simulation, Result *result){
    long long total_cells = (long long)simulation->config.L * simulation->config.C;

    long long queimadas = 0;
    long long em_chamas = 0;
    long long contencao = 0;

    // Conta as células atingidas pelo fogo e as que estão em contenção.
    for (long long i = 0; i < total_cells; i++) {
        if (simulation->estado_atual[i] == ESTADO_QUEIMADA) {
            queimadas++;
        } else if (simulation->estado_atual[i] == ESTADO_CHAMAS) {
            em_chamas++;
        } else if (simulation->estado_atual[i] == ESTADO_CONTENCAO) {
            contencao++;
        }
    }

    // Evita divisão por zero quando a grade não tem células combustíveis.
    if (simulation->celulas_combustiveis_iniciais == 0) {
        result->percentual_queimado = 0.0;
        result->percentual_protegido = 0.0;
        return;
    }

    // Calcula os percentuais em relação às células inicialmente combustíveis.
    result->percentual_queimado = 100.0 * (queimadas + em_chamas) / simulation->celulas_combustiveis_iniciais;
    result->percentual_protegido = 100.0 * contencao / simulation->celulas_combustiveis_iniciais;
}

// Imprime os resultados da simulação no formato definido pelo enunciado.
void print_results(const Result *result){

    // Imprime os resultados no formato definido pelo enunciado.
    printf("passos: %lld\n", result->passos);
    printf("nao_combustiveis: %lld\n", result->nao_combustiveis);
    printf("intactas: %lld\n", result->intactas);
    printf("em_chamas: %lld\n", result->em_chamas);
    printf("queimadas: %lld\n", result->queimadas);
    printf("contencao: %lld\n", result->contencao);
    printf("total_ignicoes: %lld\n", result->total_ignicoes);
    printf("pico_ignicoes: %lld %lld\n", result->pico_passo, result->pico_quantidade);
    printf("percentual_queimado: %.2f\n", result->percentual_queimado);
    printf("percentual_protegido: %.2f\n", result->percentual_protegido);
    printf("checksum: %llu\n", result->checksum);
    printf("tempo: %.6f\n", result->tempo);
}