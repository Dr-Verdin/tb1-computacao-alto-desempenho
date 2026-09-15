#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "output.h"
#include "simulation.h"

int main(int argc, char *argv[]) {
    // Mapa e resultados
    Simulation *sim = calloc(1, sizeof(Simulation));
    Result *res = calloc(1, sizeof(Result));

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo_entrada>\n", argv[0]);
        return 1;
    }

    // Leitura do arquivo de dados
    if (read_input(argv[1], sim) != 0) {
        printf("ERRO: falha ao ler o arquivo de entrada.\n");
        return 1;
    }

    // Inicialização das variáveis
    int passo = 0;
    int ignicoes_passo = 0;
    int nao_combustiveis = 0;
    int intactas = 0;
    int em_chamas = 0;
    int queimadas = 0;
    int contencao = 0;

    int pico_passo = -1;
    int pico_quantidade = 0;

    int total_celulas = sim->config.L * sim->config.C;

    // Cálculo do valor incial
    for (int i = 0; i < total_celulas; i++) {
        if (sim->estado_atual[i] == ESTADO_CHAMAS) {
            res->em_chamas++;
        }
    }

    res->pico_passo = -1;

    // Simulação com medidade de tempo
    double start = omp_get_wtime();

    /*
    Para cada passo p, a ordem será:
        1. ativar as zonas programadas para p;
        2. calcular o próximo estado de todas as células;
        3. calcular as estatísticas do próximo estado;
        4. trocar as matrizes;
        5. verificar a condição de parada.
    */

	#pragma omp parallel num_threads(sim->config.T) default(none)                  \
	    shared(sim, res, total_celulas, passo, ignicoes_passo, nao_combustiveis,   \
	               intactas, em_chamas, queimadas, contencao, pico_passo,          \
	               pico_quantidade)
    {
        int L = sim->config.L;
        int C = sim->config.C;
        int limiar = sim->config.limiar;
        int vento_l = sim->config.vento_linha;
        int vento_c = sim->config.vento_coluna;
        int intens = sim->config.intensidade;

        while (passo < sim->config.P && res->em_chamas > 0) {

			// Ativar contenção
			// ativacao[i] = passo && estado = INTACTO
			#pragma omp for simd schedule(static)
            for (int k = 0; k < sim->config.Z; k++) {
                Zone *z = &sim->zonas[k];

                if (z->passo_ativacao == passo) {
                    for (int i = z->linha_inicial; i <= z->linha_final; i++) {
                        for (int j = z->coluna_inicial; j <= z->coluna_final; j++) {
                            int idx = i * C + j;
                            if (sim->estado_atual[idx] == ESTADO_INTACTA) {
                            	sim->estado_atual[idx] = ESTADO_CONTENCAO;
                            }
                        }
                    }
                }
            }

			// Calcular prox passo
			// Usar collapse(2) gera um único for "flat"
			// Usando schedule(runtime), da pra testar difrentes schedules
			// dependendo da variavel env OMP_SCHEDULE
			#pragma omp for schedule(runtime) collapse(2)                                  \
			    reduction(+ : ignicoes_passo, nao_combustiveis, intactas, em_chamas,       \
			                  queimadas, contencao)
            for (int i = 0; i < L; i++) {
                for (int j = 0; j < C; j++) {
                    int idx = i * C + j;
                    int estado = sim->estado_atual[idx];

                    // Nao combustivel
                    if (estado == ESTADO_NAO_COMBUSTIVEL) {
                        sim->proximo_estado[idx] = ESTADO_NAO_COMBUSTIVEL;
                        sim->proximo_tempo[idx] = 0;
                        nao_combustiveis++;

                        // Intacto
                    } else if (estado == ESTADO_INTACTA) {
                        int S = 0;

                        // Cálculo de ignição dos 8 vizinhos
                        for (short dl = -1; dl < 2; dl++) {
                            for (short dc = -1; dc < 2; dc++) {
                                int viz_l = i + dl;
                                int viz_c = j + dc;

                                if (!(dl == 0 && dc == 0) && viz_l >= 0 &&
                                    viz_l < L && viz_c >= 0 && viz_c < C) {
                                    int viz_idx = viz_l * C + viz_c;
                                    if (sim->estado_atual[viz_idx] ==
                                        ESTADO_CHAMAS) {
                                        int prop_l = i - viz_l;
                                        int prop_c = j - viz_c;
                                        int peso_b =
                                            (abs(prop_l) + abs(prop_c) == 1)
                                                ? 10
                                                : 7;
                                        int A =
                                            prop_l * vento_l + prop_c * vento_c;
                                        int peso = peso_b + intens * A;
                                        if (peso < 1) {
                                        	peso = 1;
                                        }
                                           
                                        S += peso;
                                    }
                                }
                            }
                        }

                        int cobertura = sim->cells[idx].cobertura;
                        int umidade = sim->cells[idx].umidade;
                        int fator =
                            (cobertura == COBERTURA_VEGETACAO)
                                ? 8
                                : ((cobertura == COBERTURA_FLORESTA) ? 12 : 0);
                        int potencial = (S * fator * (100 - umidade)) / 100;

                        if (potencial >= limiar) {
                            sim->proximo_estado[idx] = ESTADO_CHAMAS;
                            sim->proximo_tempo[idx] =
                                (cobertura == COBERTURA_VEGETACAO) ? 2 : 4;
                            ignicoes_passo++;
                            em_chamas++;
                        } else {
                            sim->proximo_estado[idx] = ESTADO_INTACTA;
                            sim->proximo_tempo[idx] = 0;
                            intactas++;
                        }

                        // Em cahmas
                    } else if (estado == ESTADO_CHAMAS) {
                        int novo_tempo = sim->tempo_atual[idx] - 1;
                        sim->proximo_tempo[idx] = novo_tempo;
                        if (novo_tempo == 0) {
                            sim->proximo_estado[idx] = ESTADO_QUEIMADA;
                            queimadas++;
                        } else {
                            sim->proximo_estado[idx] = ESTADO_CHAMAS;
                            em_chamas++;
                        }

                        // Queimado
                    } else if (estado == ESTADO_QUEIMADA) {
                        sim->proximo_estado[idx] = ESTADO_QUEIMADA;
                        sim->proximo_tempo[idx] = 0;
                        queimadas++;

                        // Contencao
                    } else if (estado == ESTADO_CONTENCAO) {
                        sim->proximo_estado[idx] = ESTADO_CONTENCAO;
                        sim->proximo_tempo[idx] = 0;
                        contencao++;
                    }
                }
            }

			// Cálculos finais
			// Troca de mapa
			// Feito por uma thread só (não precisa ser master)
			// Outras threads esperam antes do prox passo
			#pragma omp single
            {
                // Troca mapa
                int *temp;
                temp = sim->estado_atual;
                sim->estado_atual = sim->proximo_estado;
                sim->proximo_estado = temp;

                temp = sim->tempo_atual;
                sim->tempo_atual = sim->proximo_tempo;
                sim->proximo_tempo = temp;

                if (ignicoes_passo > pico_quantidade) {
                    pico_quantidade = ignicoes_passo;
                    pico_passo = passo;
                }

                // Atualizar resultado
                res->total_ignicoes += ignicoes_passo;
                res->contencao = contencao;
                res->intactas = intactas;
                res->nao_combustiveis = nao_combustiveis;
                res->em_chamas = em_chamas;
                res->queimadas = queimadas;
                res->pico_passo = pico_passo;
                res->pico_quantidade = pico_quantidade;

                // Reinicar estados do passo
                ignicoes_passo = 0;
                nao_combustiveis = 0;
                intactas = 0;
                em_chamas = 0;
                queimadas = 0;
                contencao = 0;

                passo++;
            }
        }
    }

    // Fim simulação
    double end = omp_get_wtime();
    res->passos = passo;
    res->tempo = end - start;

    // Resultados
    calculate_checksum(sim, res);
    calculate_percentages(sim, res);
    print_results(res);

    // Liberar memória
    free_simulation(sim);
    free(sim);
    free(res);

    return 0;
}
