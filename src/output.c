#include <stdio.h>
#include <stdlib.h>

#include "output.h"

void calculate_checksum(const Simulation *simulation, Result *result){
    unsigned long long checksum = 0;
    long long total_cells = (long long)simulation->config.L * simulation->config.C;

    for (long long i = 0; i < total_cells; i++) {
        checksum = checksum * 31ULL + (unsigned long long)simulation->estado_atual[i];
        checksum = checksum * 31ULL + (unsigned long long)simulation->tempo_atual[i];
    }

    result->checksum = checksum;
}

void calculate_percentages(const Simulation *simulation, Result *result)
{
    long long total_cells =
        (long long)simulation->config.L * simulation->config.C;

    long long queimadas = 0;
    long long em_chamas = 0;
    long long contencao = 0;

    for (long long i = 0; i < total_cells; i++) {
        if (simulation->estado_atual[i] == ESTADO_QUEIMADA) {
            queimadas++;
        } else if (simulation->estado_atual[i] == ESTADO_CHAMAS) {
            em_chamas++;
        } else if (simulation->estado_atual[i] == ESTADO_CONTENCAO) {
            contencao++;
        }
    }

    if (simulation->celulas_combustiveis_iniciais == 0) {
        result->percentual_queimado = 0.0;
        result->percentual_protegido = 0.0;
        return;
    }

    result->percentual_queimado =
        100.0 * (queimadas + em_chamas)
        / simulation->celulas_combustiveis_iniciais;

    result->percentual_protegido =
        100.0 * contencao
        / simulation->celulas_combustiveis_iniciais;
}