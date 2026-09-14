#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "output.h"
#include "simulation.h"


static inline int _calc_ignition(Simulation *s, int linha, int coluna) {
    int L = s->config.L;
    int C = s->config.C;

    int vento_linha = s->config.vento_linha;
    int vento_coluna = s->config.vento_coluna;
    int intensidade = s->config.intensidade;

    int S = 0;

    // Percorre os 8 vizinhos de Moore.
    for (short dl = -1; dl < 2; dl++) {
        for (short dc = -1; dc < 2; dc++) {

            // Ignora a própria célula.
            if (dl == 0 && dc == 0)
                continue;

            int viz_linha = linha + dl;
            int viz_coluna = coluna + dc;

            // Ignora vizinhos fora da matriz.
            if (viz_coluna < 0 || viz_linha < 0 || viz_coluna >= C ||
                viz_linha >= L)
                continue;

            int indice = viz_linha * C + viz_coluna;

            // Somente vizinhos em chamas contribuem.
            if (s->estado_atual[indice] != ESTADO_CHAMAS)
                continue;

            /*
               Direção da propagação:
               célula atual - vizinho em chamas.
             */
            int prop_linha = linha - viz_linha;
            int prop_coluna = coluna - viz_coluna;

            // Peso básico: 10 ortogonal, 7 diagonal.
            int peso_basico;
            if (abs(prop_linha) + abs(prop_coluna) == 1)
                peso_basico = 10;
            else
                peso_basico = 7;

            // Alinhamento com o vento.
            int A = prop_linha * vento_linha + prop_coluna * vento_coluna;

            // Peso final influenciado pelo vento.
            int peso = peso_basico + intensidade * A;

            if (peso < 1)
                peso = 1;

            S += peso;
        }
    }
    // Características da célula que pode pegar fogo.
    int indice = linha * C + coluna;
    int cobertura = s->cells[indice].cobertura;
    int umidade = s->cells[indice].umidade;

    // Fator de combustível.
    int fator;
    if (cobertura == COBERTURA_VEGETACAO)
        fator = 8;
    else if (cobertura == COBERTURA_FLORESTA)
        fator = 12;
    else
        fator = 0;

    // Potencial de ignição.
    int potencial = (S * fator * (100 - umidade)) / 100;

    return potencial;
}

/* Atualiza `proximo_estado` e `proximo_tempo` para a célula.
 * Não altera os arrays atuais; escreve apenas nos arrays de próximo.
 */
# pragma omp declare simd uniform(s)
static inline void _update_map_cell(Simulation *s, int linha, int coluna) {
    int indice = linha * s->config.C + coluna;

    if (s->estado_atual[indice] == ESTADO_NAO_COMBUSTIVEL) {
        s->proximo_estado[indice] = ESTADO_NAO_COMBUSTIVEL;
        s->proximo_tempo[indice] = 0;

        return;
    }

    if (s->estado_atual[indice] == ESTADO_INTACTA) {
        int potencial = _calc_ignition(s, linha, coluna);

        if (potencial >= s->config.limiar) {
            s->proximo_estado[indice] = ESTADO_CHAMAS;

            if (s->cells[indice].cobertura == COBERTURA_VEGETACAO)
                s->proximo_tempo[indice] = 2;
            else
                s->proximo_tempo[indice] = 4;
        } else {
            s->proximo_estado[indice] = ESTADO_INTACTA;
            s->proximo_tempo[indice] = 0;
        }
        return;
    }

    if (s->estado_atual[indice] == ESTADO_CHAMAS) {
        s->proximo_tempo[indice] = s->tempo_atual[indice] - 1;

        if (s->proximo_tempo[indice] == 0)
            s->proximo_estado[indice] = ESTADO_QUEIMADA;
        else
            s->proximo_estado[indice] = ESTADO_CHAMAS;

        return;
    }

    if (s->estado_atual[indice] == ESTADO_QUEIMADA) {
        s->proximo_estado[indice] = ESTADO_QUEIMADA;
        s->proximo_tempo[indice] = 0;

        return;
    }

    if (s->estado_atual[indice] == ESTADO_CONTENCAO) {
        s->proximo_estado[indice] = ESTADO_CONTENCAO;
        s->proximo_tempo[indice] = 0;

        return;
    }
}

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

	#pragma omp parallel num_threads(sim->config.T) default(none) \
	    shared(sim, res, total_celulas, passo, ignicoes_passo, \
		nao_combustiveis, intactas, em_chamas, queimadas, contencao, \
		pico_passo, pico_quantidade)
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
            for (long i = 0; i < total_celulas; i++) {
                if (sim->ativacao[i] == passo &&
                    sim->estado_atual[i] == ESTADO_INTACTA) {
                    sim->estado_atual[i] = ESTADO_CONTENCAO;
                }
            }

			// Calcular prox passo
			// Usar collapse(2) gera um único for "flat"
			// Usando schedule(runtime), da pra testar difrentes schedules
			// dependendo da variavel env OMP_SCHEDULE
			#pragma omp for simd schedule(runtime) collapse(2) reduction(+ : ignicoes_passo, \
				nao_combustiveis, intactas, em_chamas, queimadas, contencao)
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
                        // Pra usar vetorização tirei da função
                        // e removi os "return"
                        for (short dl = -1; dl < 2; dl++) {
                            for (short dc = -1; dc < 2; dc++) {
                                int viz_l = i + dl;
                                int viz_c = j + dc;
				
                                if (!(dl == 0 && dc == 0) && viz_l >= 0 && viz_l < L && viz_c >= 0 && viz_c < C) {
                                    int viz_idx = viz_l * C + viz_c;
                                    if (sim->estado_atual[viz_idx] == ESTADO_CHAMAS) {
                                        int prop_l = i - viz_l;
                                        int prop_c = j - viz_c;
                                        int peso_b = (abs(prop_l) + abs(prop_c) == 1) ? 10 : 7;
                                        int A = prop_l * vento_l + prop_c * vento_c;
                                        int peso = peso_b + intens * A;
                                        if (peso < 1) peso = 1;
                                        S += peso;
                                    }
                                }
                            }
                        }
				
                        int cobertura = sim->cells[idx].cobertura;
                        int umidade = sim->cells[idx].umidade;
                        int fator = (cobertura == COBERTURA_VEGETACAO) ? 8 : ((cobertura == COBERTURA_FLORESTA) ? 12 : 0);
                        int potencial = (S * fator * (100 - umidade)) / 100;
				
                        if (potencial >= limiar) {
                            sim->proximo_estado[idx] = ESTADO_CHAMAS;
                            sim->proximo_tempo[idx] = (cobertura == COBERTURA_VEGETACAO) ? 2 : 4;
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
