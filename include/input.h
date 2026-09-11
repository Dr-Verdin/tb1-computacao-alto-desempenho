#ifndef INPUT_H
#define INPUT_H

#include "simulation.h"

// Lê, valida e inicializa os dados de entrada da simulação a partir do arquivo especificado. 
int read_input(const char *filename, Simulation *simulation);

// Libera a memória alocada para a simulação. 
void free_simulation(Simulation *simulation);

#endif