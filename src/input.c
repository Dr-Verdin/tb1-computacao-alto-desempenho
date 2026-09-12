#include <stdio.h>
#include <stdlib.h>

#include "input.h"

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
    
    // Lê e valida os parâmetros da simulação
    if(fscanf(file, "%d %d %d %d %u %d", 
            &simulation->config.L, 
            &simulation->config.C, 
            &simulation->config.P, 
            &simulation->config.T, 
            &simulation->config.seed, 
            &simulation->config.limiar) != 6) {
        fclose(file);
        return 1;
    }

    if(simulation->config.L <= 0 || 
        simulation->config.C <= 0 || 
        simulation->config.P < 0 || 
        simulation->config.T <= 0 ||
        simulation->config.limiar <= 0) {
        fclose(file);
        return 1;
    }

    if(fscanf(file, "%d %d %d", 
            &simulation->config.vento_linha, 
            &simulation->config.vento_coluna, 
            &simulation->config.intensidade) != 3) {
        fclose(file);
        return 1;
    }

    if(simulation->config.vento_coluna < -1 || simulation->config.vento_coluna > 1 || 
        simulation->config.vento_linha < -1 || simulation->config.vento_linha > 1 || 
        (simulation->config.vento_coluna == 0 && simulation->config.vento_linha == 0) ||
        simulation->config.intensidade < 0 || simulation->config.intensidade > 5) {
        fclose(file);
        return 1;
    }

    if(fscanf(file, "%d %d", 
            &simulation->config.F, 
            &simulation->config.Z) != 2) {
        fclose(file);
        return 1;
    }

    if(simulation->config.F < 0 || 
        simulation->config.Z < 0) {
        fclose(file);
        return 1;
    }

    // Aloca memória para as células, estados e tempos
    long long total_cells = (long long)simulation->config.L * simulation->config.C;
    
    simulation->cells = malloc(total_cells * sizeof(Cell));

    simulation->estado_atual = malloc(total_cells * sizeof(int));
    simulation->proximo_estado = malloc(total_cells * sizeof(int));

    simulation->tempo_atual = malloc(total_cells * sizeof(int));
    simulation->proximo_tempo = malloc(total_cells * sizeof(int));

    simulation->ativacao = malloc(total_cells * sizeof(int));

    if (simulation->config.Z > 0) {
        simulation->zonas = malloc(simulation->config.Z * sizeof(Zone));
    } else {
        simulation->zonas = NULL;
    }

    if (simulation->cells == NULL ||
        simulation->estado_atual == NULL ||
        simulation->proximo_estado == NULL ||
        simulation->tempo_atual == NULL ||
        simulation->proximo_tempo == NULL ||
        simulation->ativacao == NULL ||
        (simulation->config.Z > 0 && simulation->zonas == NULL)) {
        fclose(file);
        free_simulation(simulation);
        return 1;
    }

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

    // Lê e valida os focos iniciais
    for (int i = 0; i < simulation->config.F; i++) {
        int linha;
        int coluna;

        if (fscanf(file, "%d %d", &linha, &coluna) != 2) {
            fclose(file);
            free_simulation(simulation);
            return 1;
        }

        if (linha < 0 || linha >= simulation->config.L ||
            coluna < 0 || coluna >= simulation->config.C) {
            fclose(file);
            free_simulation(simulation);
            return 1;
        }

        long long indice = (long long)linha * simulation->config.C + coluna;

        if (simulation->cells[indice].cobertura != COBERTURA_VEGETACAO &&
            simulation->cells[indice].cobertura != COBERTURA_FLORESTA) {
            fclose(file);
            free_simulation(simulation);
            return 1;
        }

        if (simulation->estado_atual[indice] == ESTADO_CHAMAS) {
            fclose(file);
            free_simulation(simulation);
            return 1;
        }

        if (simulation->cells[indice].cobertura == COBERTURA_VEGETACAO) {
            simulation->estado_atual[indice] = ESTADO_CHAMAS;
            simulation->tempo_atual[indice] = 2;

            simulation->proximo_estado[indice] = ESTADO_CHAMAS;
            simulation->proximo_tempo[indice] = 2;
        } else {
            simulation->estado_atual[indice] = ESTADO_CHAMAS;
            simulation->tempo_atual[indice] = 4;

            simulation->proximo_estado[indice] = ESTADO_CHAMAS;
            simulation->proximo_tempo[indice] = 4;
        }
    }

    // Lê e valida as zonas de contenção
    for (int i = 0; i < simulation->config.Z; i++) {
        Zone *zona = &simulation->zonas[i];

        if (fscanf(file, "%d %d %d %d %d",
                &zona->passo_ativacao,
                &zona->linha_inicial,
                &zona->coluna_inicial,
                &zona->linha_final,
                &zona->coluna_final) != 5) {
            fclose(file);
            free_simulation(simulation);
            return 1;
        }

        if (zona->passo_ativacao < 0 ||
            zona->passo_ativacao >= simulation->config.P ||
            zona->linha_inicial < 0 ||
            zona->linha_final >= simulation->config.L ||
            zona->coluna_inicial < 0 ||
            zona->coluna_final >= simulation->config.C ||
            zona->linha_inicial > zona->linha_final ||
            zona->coluna_inicial > zona->coluna_final) {
            fclose(file);
            free_simulation(simulation);
            return 1;
        }
    }

    // Inicializa o vetor de ativação das zonas com -1 
    for (long long indice = 0; indice < total_cells; indice++) {
        simulation->ativacao[indice] = -1;
    }

    // Preenche o vetor de ativação com as zonas
    for (int i = 0; i < simulation->config.Z; i++) {
        Zone *zona = &simulation->zonas[i];

        for (int linha = zona->linha_inicial;
            linha <= zona->linha_final;
            linha++) {

            for (int coluna = zona->coluna_inicial; coluna <= zona->coluna_final; coluna++) {
                long long indice = (long long)linha * simulation->config.C + coluna;

                if (simulation->ativacao[indice] == -1 ||
                    zona->passo_ativacao < simulation->ativacao[indice]) {
                    simulation->ativacao[indice] = zona->passo_ativacao;
                }
            }
        }
    }

    fclose(file);
    return 0;
}

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