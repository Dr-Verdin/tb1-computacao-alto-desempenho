#include "../include/simulation.h"
#include <stdlib.h>

/* Implementação sequencial do núcleo da simulação.
 * Contém funções para calcular ignição, atualizar uma célula,
 * ativar zonas de contenção, calcular o próximo estado global
 * e executar o laço principal sequencial.
 */

/* Calcula o potencial de ignição da célula (linha,coluna).
 * Retorna o valor inteiro do potencial conforme a especificação.
 */
static int Calculate_ignition(Simulation* s, int linha, int coluna){
    int L = s->config.L;
    int C = s->config.C;

    int vento_linha = s->config.vento_linha;
    int vento_coluna = s->config.vento_coluna;
    int intensidade = s->config.intensidade;

    int S = 0;

    // Percorre os 8 vizinhos de Moore.
    for(short dl = -1; dl<2; dl++){
        for(short dc = -1; dc<2; dc++){

            // Ignora a própria célula.
            if(dl == 0 && dc == 0) continue;

            int viz_linha = linha + dl;
            int viz_coluna = coluna + dc;

            // Ignora vizinhos fora da matriz.
            if(viz_coluna < 0 || viz_linha < 0 || viz_coluna >= C || viz_linha >= L) continue;

            int indice = viz_linha*C + viz_coluna;

            // Somente vizinhos em chamas contribuem.
            if(s->estado_atual[indice] != ESTADO_CHAMAS) continue;

            /*
               Direção da propagação:
               célula atual - vizinho em chamas.
             */
            int prop_linha = linha - viz_linha;
            int prop_coluna = coluna - viz_coluna;

            // Peso básico: 10 ortogonal, 7 diagonal.
            int peso_basico;
            if(abs(prop_linha) + abs(prop_coluna) == 1) 
                peso_basico = 10;
            else 
                peso_basico = 7;

             // Alinhamento com o vento.
             int A = prop_linha*vento_linha + prop_coluna*vento_coluna;

             // Peso final influenciado pelo vento.
             int peso = peso_basico + intensidade*A;

             if(peso < 1) peso = 1;

            S+=peso;
        }
    }
    // Características da célula que pode pegar fogo.
    int indice = linha*C + coluna;
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
static void Update_cell(Simulation* s, int linha, int coluna){
    int indice = linha*s->config.C + coluna;

    // Não combustível.
    if (s->estado_atual[indice] == ESTADO_NAO_COMBUSTIVEL) {
        s->proximo_estado[indice] = ESTADO_NAO_COMBUSTIVEL;
        s->proximo_tempo[indice] = 0;

        return;
    }

    // Intacta.
    if(s->estado_atual[indice] == ESTADO_INTACTA){
        int potencial = Calculate_ignition(s, linha, coluna);

        if(potencial >= s->config.limiar){
            s->proximo_estado[indice] = ESTADO_CHAMAS;

            if(s->cells[indice].cobertura == COBERTURA_VEGETACAO)
                s->proximo_tempo[indice] = 2;
            else
                s->proximo_tempo[indice] = 4;
        }else{
            s->proximo_estado[indice] = ESTADO_INTACTA;
            s->proximo_tempo[indice] = 0;
        }
        return;
    }

    // Em chamas.
    if(s->estado_atual[indice] == ESTADO_CHAMAS){
        s->proximo_tempo[indice] = s->tempo_atual[indice] - 1;

        if(s->proximo_tempo[indice] == 0)
            s->proximo_estado[indice] = ESTADO_QUEIMADA;
        else 
            s->proximo_estado[indice] = ESTADO_CHAMAS;

        return;
    }

    /* Queimada */
    if (s->estado_atual[indice] == ESTADO_QUEIMADA) {
        s->proximo_estado[indice] = ESTADO_QUEIMADA;
        s->proximo_tempo[indice] = 0;

        return;
    }

    /* Contenção */
    if (s->estado_atual[indice] == ESTADO_CONTENCAO) {
        s->proximo_estado[indice] = ESTADO_CONTENCAO;
        s->proximo_tempo[indice] = 0;

        return;
    }
}

/* Ativa todas as zonas cuja ativação == passo.
 * Células intactas tornam-se contenção; zonas não apagam fogo.
 */
static void Activate_zones(Simulation* s, int passo){
    int C = s->config.C;

    for(int z = 0; z < s->config.Z; z++){
        Zone* zona = &s->zonas[z];

        if(zona->passo_ativacao != passo) continue;

        for(int i = zona->linha_inicial; i <= zona->linha_final; i++){
            for(int j = zona->coluna_inicial; j <= zona->coluna_final; j++){
                int indice = i*C + j;

                if(s->estado_atual[indice] == ESTADO_INTACTA)
                    s->estado_atual[indice] = ESTADO_CONTENCAO;
            }
        }
    }
}

/* Calcula o próximo estado para todas as células e atualiza os
 * contadores em `res` relativos ao próximo passo.
 * Retorna o número de novas ignições (intacta -> chamas).
 */
static int Calculate_next_state(Simulation* s, Result *res){
    int L = s->config.L;
    int C = s->config.C;

    int ignicoes = 0;

    // Zera as estatísticas do passo
    res->nao_combustiveis = 0;
    res->intactas = 0;
    res->em_chamas = 0;
    res->queimadas = 0;
    res->contencao = 0;

    for(int i = 0; i < L; i++){
        for(int j = 0; j < C; j++){

            // Calcula o próximo estado
            Update_cell(s, i, j);

            int indice = i*C + j;

            // Nova ignição: intacta -> chamas
            if (s->estado_atual[indice] == ESTADO_INTACTA && s->proximo_estado[indice] == ESTADO_CHAMAS) {
                ignicoes++;
            }

            // Conta o estado no próximo passo
            switch (s->proximo_estado[indice]) {
                case ESTADO_NAO_COMBUSTIVEL:
                    res->nao_combustiveis++;
                    break;

                case ESTADO_INTACTA:
                    res->intactas++;
                    break;

                case ESTADO_CHAMAS:
                    res->em_chamas++;
                    break;

                case ESTADO_QUEIMADA:
                    res->queimadas++;
                    break;

                case ESTADO_CONTENCAO:
                    res->contencao++;
                    break;
            }
        }
    }
    return ignicoes;
}

/* Troca os ponteiros dos arrays atual <-> próximo (estado e tempo).
 * Operação em tempo constante.
 */
static void Swap_arrays(Simulation* s){
    int* temp;

    temp = s->estado_atual;
    s->estado_atual = s->proximo_estado;
    s->proximo_estado = temp;

    temp = s->tempo_atual;
    s->tempo_atual = s->proximo_tempo;
    s->proximo_tempo = temp;
}

/* Laço principal sequencial.
 * Ordem por passo: ativar zonas, calcular próximo estado, atualizar
 * estatísticas, trocar buffers e verificar condição de parada.
 */
void Simulate_sequential(Simulation* s, Result *res){
    int passo = 0;

    // Inicializa os resultados acumulativos
    res->total_ignicoes = 0;
    res->pico_quantidade = 0;
    res->pico_passo = -1;

    // Conta quantas células estão em chamas inicialmente
    long long total_cells = (long long)s->config.L * s->config.C;
    res->em_chamas = 0;
    for (long long i = 0; i < total_cells; i++) {
        if (s->estado_atual[i] == ESTADO_CHAMAS)
            res->em_chamas++;
    }

    // Se não há chamas após inicialização, termina.
    if (res->em_chamas == 0) {
        res->passos = 0;
        res->tempo = 0.0;
        return;
    }

    double start = omp_get_wtime();

    while(passo < s->config.P){
        // Ativa as zonas de contenção no passo atual.
        Activate_zones(s, passo);

        // Calcula o próximo estado da simulação.
        int ignicoes = Calculate_next_state(s, res);

        // Atualiza a variavel acumulativa de ignições.
        res->total_ignicoes += ignicoes;
        
        // Atualiza o pico de ignições.
        if (ignicoes > res->pico_quantidade) {
            res->pico_quantidade = ignicoes;
            res->pico_passo = passo;
        }

        // Troca os arrays atual <-> próximo.
        Swap_arrays(s);

        passo++;

        // Se não há mais células em chamas, termina.
        if(res->em_chamas == 0)
            break;
    }
    double end = omp_get_wtime();

    res->passos = passo;
    res->tempo = end - start;
}