#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#include "input.h"
#include "output.h"
#include "simulation.h"

/*
 A versão paralela deve estar no arquivo fire_omp.c. A versão paralela deverá utilizar T threads,
 empregar uma região paralela persistente, paralelizar a ativação das zonas, paralelizar a
 atualização da matriz, utilizar omp for, simd, utilizar reduções para os contadores, evitar
 condições de corrida, trocar as matrizes de forma segura, compartilhar corretamente a condição
 de parada, evitar critical e atomic dentro do laço principal quando reduções puderem ser
 usadas, produzir resultados independentes do número de threads, comparar pelo menos dois
 schedules, e utilizar default(none) nas regiões relevantes. O uso de task, paralelismo aninhado
 e visualização não é obrigatório.
 */

int main(int argc, char *argv[]) {
	// Mapa e resultados
	Simulation sim;
	Result res;
	
	if(argc != 2){
        fprintf(stderr, "Uso: %s <arquivo_entrada>\n", argv[0]);
        return 1;
    }

	// Leitura do arquivo de dados
    if (read_input(argv[1], &sim) != 0) {
        printf("ERRO: falha ao ler o arquivo de entrada.\n");
        return 1;
    }

	// Inicialização
	int passo = 0;
    int total_ignicoes = 0;
    int pico_quantidade = 0;
    int pico_passo = -1;
    
    int total_celulas = sim.config.L * sim.config.C;

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

	# pragma omp parallel num_threads(sim.config.T) default(none) \
		shared(total_celulas, total_ignicoes, pico_quantidade, pico_passo)
	{
		while (passo < sim.config.P) {

			// Ativar contenção
			// ativacao[i] = passo && estado = INTACTO
			# pragma omp for schedule(static) simd
			for (long i = 0; i < total_celulas; i++) {
				if (sim.ativacao[i] == passo && sim.estado_atual[i] == ESTADO_INTACTA) {
					sim.estado_atual[i] = ESTADO_CONTENCAO;
				}
			}

			// Cálculos finais
			// Troca de mapa
			// Feito por uma thread só (não precisa ser master)
			# pragma omp single
			{
				// Troca mapa
				int *temp;
				temp = sim.estado_atual;
			    sim.estado_atual = sim.proximo_estado;
			    sim.proximo_estado = temp;
			
			    temp = sim.tempo_atual;
			    sim.tempo_atual = sim.proximo_tempo;
			    sim.proximo_tempo = temp;
	
				passo++;
			}
		}
	}


	// Fim simulação
	double end = omp_get_wtime();
	
	free_simulation(&sim);
	return 0;
}