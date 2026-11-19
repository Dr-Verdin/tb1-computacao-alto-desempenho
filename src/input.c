#include <stdio.h>
#include <stdlib.h>

#include "input.h"


// Lê e valida os parâmetros da simulação.
static int read_config(FILE* file, Config* config){

    // Lê as dimensões, os passos, a quantidade de threads, a semente e o limiar.
    if(fscanf(file, "%d %d %d %d %u %d", 
            &config->L, 
            &config->C, 
            &config->P, 
            &config->T, 
            &config->seed, 
            &config->limiar) != 6) {
        return 1;
    }

    // Verifica os limites básicos dos parâmetros da simulação.
    if(config->L <= 0 || config->C <= 0 || config->P < 0 || config->T <= 0 ||config->limiar <= 0) {
        return 1;
    }

    // Lê a direção e a intensidade do vento.
    if(fscanf(file, "%d %d %d", &config->vento_linha, &config->vento_coluna, &config->intensidade) != 3) {
        return 1;
    }

    // O vento deve apontar para alguma direção válida e ter intensidade entre 0 e 5.
    if(config->vento_coluna < -1 || config->vento_coluna > 1 || 
        config->vento_linha < -1 || config->vento_linha > 1 || 
        (config->vento_coluna == 0 && config->vento_linha == 0) ||
        config->intensidade < 0 || config->intensidade > 5) {
        return 1;
    }

    // Lê a quantidade de focos iniciais e de zonas de contenção.
    if(fscanf(file, "%d %d", &config->F, &config->Z) != 2) {
        return 1;
    }

    // Não são permitidas quantidades negativas de focos ou zonas.
    if(config->F < 0 || config->Z < 0) {
        return 1;
    }

    return 0;
}

// Aloca os vetores da simulação.
static int allocate_simulation(Simulation* simulation, long long total_cells){
    
    // Aloca os dados da grade e os vetores de estado e tempo.
    simulation->cells = malloc(total_cells * sizeof(Cell));
    simulation->estado_atual = malloc(total_cells * sizeof(int));
    simulation->proximo_estado = malloc(total_cells * sizeof(int));
    simulation->tempo_atual = malloc(total_cells * sizeof(int));
    simulation->proximo_tempo = malloc(total_cells * sizeof(int));
    simulation->ativacao = malloc(total_cells * sizeof(int));

    // Só aloca o vetor de zonas quando há zonas na entrada.
    if (simulation->config.Z > 0){
        simulation->zonas = malloc(simulation->config.Z * sizeof(Zone));
    }

    // Se alguma alocação falhou, libera as que tiveram sucesso.
    if (simulation->cells == NULL ||
        simulation->estado_atual == NULL ||
        simulation->proximo_estado == NULL ||
        simulation->tempo_atual == NULL ||
        simulation->proximo_tempo == NULL ||
        simulation->ativacao == NULL ||
        (simulation->config.Z > 0 && simulation->zonas == NULL)) {
        free_simulation(simulation);
        return 1;
    }

    return 0;
}

// Sorteia as caracteristicas das células e define seus estados iniciais.
static void initialize_grid(Simulation* simulation, long long total_cells){

    // Inicializa as células com coberturas e umidades aleatórias
    for (int linha = 0; linha < simulation->config.L; linha++) {
        for (int coluna = 0; coluna < simulation->config.C; coluna++) {
            long long indice = (long long)linha * simulation->config.C + coluna;

            int valor = rand_r(&simulation->config.seed) % 100;
            int umidade = rand_r(&simulation->config.seed) % 101;

            if (valor < 10) {
                simulation->cells[indice].cobertura = COBERTURA_AGUA;
            } else if (valor < 20) {
                simulation->cells[indice].cobertura = COBERTURA_SOLO;
            } else if (valor < 55) {
                simulation->cells[indice].cobertura = COBERTURA_VEGETACAO;
            } else {
                simulation->cells[indice].cobertura = COBERTURA_FLORESTA;
            }

            simulation->cells[indice].umidade = umidade;
        }
    }

    // Inicializa os estados e tempos das células
    for (long long indice = 0; indice < total_cells; indice++) {
        if (simulation->cells[indice].cobertura == COBERTURA_AGUA ||
            simulation->cells[indice].cobertura == COBERTURA_SOLO) {
            simulation->estado_atual[indice] = ESTADO_NAO_COMBUSTIVEL;
        } else {
            simulation->estado_atual[indice] = ESTADO_INTACTA;
            simulation->celulas_combustiveis_iniciais++;
        }

        simulation->tempo_atual[indice] = 0;
        simulation->proximo_estado[indice] = simulation->estado_atual[indice];
        simulation->proximo_tempo[indice] = simulation->tempo_atual[indice];
    }
}

// Lê os focos iniciais e marca as células correspondentes como fogo.
static int read_initial_fires(FILE* file, Simulation* simulation){

    for(int i = 0; i < simulation->config.F; i++){
        int linha, coluna;

        // Cada foco precisa fornecer uma linha e uma coluna.
        if (fscanf(file, "%d %d", &linha, &coluna) != 2) {
            return 1;
        }

        // O foco precisa estar dentro dos limites da grade.
        if (linha < 0 || linha >= simulation->config.L || coluna < 0 || coluna >= simulation->config.C) {
            return 1;
        }

        long long indice = (long long) linha * simulation->config.C + coluna;

        // O foco deve estar em uma célula combustível ainda intacta.
        if (simulation->cells[indice].cobertura != COBERTURA_VEGETACAO &&
            simulation->cells[indice].cobertura != COBERTURA_FLORESTA) {
            return 1;
        }

        if(simulation->estado_atual[indice] == ESTADO_CHAMAS){
            return 1;
        }

        // A duração inicial do fogo depende do tipo de cobertura.
        int duracao = (simulation->cells[indice].cobertura == COBERTURA_VEGETACAO) ? 2 : 4;

        // Atualiza os buffers atual e próximo para iniciar a célula em chamas.
        simulation->estado_atual[indice] = ESTADO_CHAMAS;
        simulation->tempo_atual[indice] = duracao;
        simulation->proximo_estado[indice] = ESTADO_CHAMAS;
        simulation->proximo_tempo[indice] = duracao;
    }

    return 0;
}

static int read_zones(FILE* file, Simulation* simulation){

    // Lê e valida as zonas de contenção
    for (int i = 0; i < simulation->config.Z; i++) {
        Zone *zona = &simulation->zonas[i];

        // Lê o passo de ativação e os limites da zona.
        if (fscanf(file, "%d %d %d %d %d",
                &zona->passo_ativacao,
                &zona->linha_inicial,
                &zona->coluna_inicial,
                &zona->linha_final,
                &zona->coluna_final) != 5) {
            return 1;
        }

        // Verifica se o passo e o retângulo estão dentro dos limites válidos.
        if (zona->passo_ativacao < 0 ||
            zona->passo_ativacao >= simulation->config.P ||
            zona->linha_inicial < 0 ||
            zona->linha_final >= simulation->config.L ||
            zona->coluna_inicial < 0 ||
            zona->coluna_final >= simulation->config.C ||
            zona->linha_inicial > zona->linha_final ||
            zona->coluna_inicial > zona->coluna_final) {
            return 1;
        }
    }

    return 0;
}

static void initialize_activation(Simulation* simulation, long long total_cells){

    // Marca como -1 as células que não pertencem a nenhuma zona.
    for (long long indice = 0; indice < total_cells; indice++) {
        simulation->ativacao[indice] = -1;
    }

    // Percorre cada zona e atualiza as células cobertas por ela.
    for (int i = 0; i < simulation->config.Z; i++) {
        Zone *zona = &simulation->zonas[i];

        for (int linha = zona->linha_inicial; linha <= zona->linha_final; linha++) {
            for (int coluna = zona->coluna_inicial; coluna <= zona->coluna_final; coluna++) {
                long long indice = (long long)linha * simulation->config.C + coluna;

                // Se zonas se sobrepõem, mantém o passo de ativação mais cedo.
                if (simulation->ativacao[indice] == -1 ||
                    zona->passo_ativacao < simulation->ativacao[indice]) {
                    simulation->ativacao[indice] = zona->passo_ativacao;
                }
            }
        }
    }
}

// Lê o arquivo de entrada, valida os dados e inicializa a simulação a partir do arquivo passado.
int read_input(const char *filename, Simulation *simulation){
    // Abre o arquivo de entrada
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return 1;
    }

    // Inicializa os ponteiros da simulação 
    simulation->cells = NULL;
    simulation->estado_atual = NULL;
    simulation->proximo_estado = NULL;
    simulation->tempo_atual = NULL;
    simulation->proximo_tempo = NULL;
    simulation->ativacao = NULL;
    simulation->zonas = NULL;
    simulation->celulas_combustiveis_iniciais = 0;
    
    // Faz a leitura e validação dos parâmetros da simulação
    if(read_config(file, &simulation->config) != 0){
        fclose(file);
        return 1;
    }

    // Calcula o total de células para dimensionar os vetores.
    long long total_cells = (long long)simulation->config.L * simulation->config.C;

    // Aloca memória para as células, estados e tempos
    if (allocate_simulation(simulation, total_cells) != 0){
        fclose(file);
        return 1;
    }

    // Inicializa a cobertura, a umidade e os estados da grade.
    initialize_grid(simulation, total_cells);

    // Lê os focos e atualiza os estados iniciais da grade.
    if (read_initial_fires(file, simulation) != 0) {
        fclose(file);
        free_simulation(simulation);
        return 1;
    }

    // Lê e valida as zonas de contenção.
    if (read_zones(file, simulation) != 0) {
        fclose(file);
        free_simulation(simulation);
        return 1;
    }

    // Monta o vetor de ativação a partir das zonas lidas.
    initialize_activation(simulation, total_cells);

    fclose(file);
    return 0;
}

// Libera a memória alocada para os dados da simulação.
void free_simulation(Simulation *simulation){
    free(simulation->cells);
    free(simulation->estado_atual);
    free(simulation->proximo_estado);
    free(simulation->tempo_atual);
    free(simulation->proximo_tempo);
    free(simulation->ativacao);
    free(simulation->zonas);

    simulation->cells = NULL;
    simulation->estado_atual = NULL;
    simulation->proximo_estado = NULL;
    simulation->tempo_atual = NULL;
    simulation->proximo_tempo = NULL;
    simulation->ativacao = NULL;
    simulation->zonas = NULL;
}