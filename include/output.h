#ifndef OUTPUT_H
#define OUTPUT_H

#include "simulation.h"

// Calcula o checksum do estado final da simulação.
void calculate_checksum(const Simulation *simulation, Result *result);

// Calcula os percentuais de células queimadas e protegidas.
void calculate_percentages(const Simulation *simulation, Result *result);

// Imprime os resultados da simulação no formato especificado.
void print_results(const Result *result);

#endif