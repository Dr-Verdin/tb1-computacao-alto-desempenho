#include <stdio.h>

#include "input.h"
#include "output.h"

int main(int argc, char *argv[])
{
    Simulation simulation;

    if (argc != 2) {
        printf("Uso: %s <arquivo_entrada>\n", argv[0]);
        return 1;
    }

    if (read_input(argv[1], &simulation) != 0) {
        printf("ERRO: falha ao ler o arquivo de entrada.\n");
        return 1;
    }

    printf("=== PARAMETROS ===\n");
    printf("L = %d\n", simulation.config.L);
    printf("C = %d\n", simulation.config.C);
    printf("P = %d\n", simulation.config.P);
    printf("T = %d\n", simulation.config.T);
    printf("Limiar = %d\n", simulation.config.limiar);
    printf("Vento = (%d, %d)\n",
           simulation.config.vento_linha,
           simulation.config.vento_coluna);
    printf("Intensidade = %d\n", simulation.config.intensidade);
    printf("F = %d\n", simulation.config.F);
    printf("Z = %d\n", simulation.config.Z);

    printf("\n=== CELULAS COMBUSTIVEIS ===\n");
    printf("Total = %lld\n",
           simulation.celulas_combustiveis_iniciais);

    printf("\n=== FOCOS ===\n");

    int focos_linha[] = {100, 200, 300};
    int focos_coluna[] = {83, 250, 416};

    for (int i = 0; i < simulation.config.F; i++) {
        long long indice =
            (long long)focos_linha[i] * simulation.config.C
            + focos_coluna[i];

        printf("Foco %d: (%d, %d) -> estado = %d, tempo = %d\n",
               i + 1,
               focos_linha[i],
               focos_coluna[i],
               simulation.estado_atual[indice],
               simulation.tempo_atual[indice]);
    }

    printf("\n=== ZONAS ===\n");

    for (int i = 0; i < simulation.config.Z; i++) {
        Zone *zona = &simulation.zonas[i];

        printf("Zona %d: passo = %d, "
               "linhas = %d-%d, "
               "colunas = %d-%d\n",
               i + 1,
               zona->passo_ativacao,
               zona->linha_inicial,
               zona->linha_final,
               zona->coluna_inicial,
               zona->coluna_final);
    }

    printf("\n=== ATIVACAO ===\n");

    int testes_linha[] = {70, 100, 130, 150, 200, 245, 300, 335, 355};
    int testes_coluna[] = {150, 100, 165, 160, 250, 320, 416, 335, 335};

    int quantidade_testes = 9;

    for (int i = 0; i < quantidade_testes; i++) {
        long long indice =
            (long long)testes_linha[i] * simulation.config.C
            + testes_coluna[i];

        printf("(%d, %d) -> ativacao = %d\n",
               testes_linha[i],
               testes_coluna[i],
               simulation.ativacao[indice]);
    }

    printf("\n=== TESTE OUTPUT ===\n");

    Result result = {0};

    calculate_checksum(&simulation, &result);
    calculate_percentages(&simulation, &result);

    result.passos = 0;
    result.nao_combustiveis = 0;
    result.intactas = 0;
    result.em_chamas = 0;
    result.queimadas = 0;
    result.contencao = 0;
    result.total_ignicoes = 0;
    result.pico_passo = -1;
    result.pico_quantidade = 0;
    result.tempo = 0.123456;

    print_results(&result);

    free_simulation(&simulation);

    printf("\nTeste concluido.\n");

    return 0;
}