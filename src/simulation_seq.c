#include "../include/simulation.h"
#include <stdlib.h>

static int Calcular_ignicao(Simulation* s, int linha, int coluna){
    int L = s->config.L;
    int C = s->config.c;

    int vento_linha = s->config.vento_linha;
    int vento_coluna = s->config.vento_coluna;
    int intensidade = s->config.V;

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

static void Atualizar_celula(Simulation* s, int linha, int coluna){
    int indice = linha*s->config.c + coluna;

    // Não combustível.
    if (s->estado_atual[indice] == ESTADO_NAO_COMBUSTIVEL) {
        s->proximo_estado[indice] = ESTADO_NAO_COMBUSTIVEL;
        s->proximo_tempo[indice] = 0;

        return;
    }

    // Intacta.
    if(s->estado_atual[indice] == ESTADO_INTACTA){
        int potencial = Calcular_ignicao(s, linha, coluna);

        if(potencial >= s->config.LIMIAR){
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

static void Ativar_zonas(Simulation* s, int passo){
    int C = s->config.c;

    for(int z = 0; z < s->config.Z; z++){
        Zone* zona =  s->zonas[z];

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

static int Calcular_proximo_estado(Simulation* s){
    int L = s->config.L;
    int C = s->config.c;

    int ignicoes = 0;

    // Zera as estatísticas do passo
    s->results.nao_combustiveis = 0;
    s->results.intactas = 0;
    s->results.em_chamas = 0;
    s->results.queimadas = 0;
    s->results.contencao = 0;

    for(int i = 0; i < L; i++){
        for(int j = 0; j < C; j++){

            // Calcula o próximo estado
            Atualizar_celula(s, i, j);

            indice = i*C + j;

            // Nova ignição: intacta -> chamas
            if (s->estado_atual[indice] == ESTADO_INTACTA && s->proximo_estado[indice] == ESTADO_CHAMAS) {
                ignicoes++;
            }

            // Conta o estado no próximo passo
            switch (s->proximo_estado[indice]) {
                case ESTADO_NAO_COMBUSTIVEL:
                    s->results.nao_combustiveis++;
                    break;

                case ESTADO_INTACTA:
                    s->results.intactas++;
                    break;

                case ESTADO_CHAMAS:
                    s->results.em_chamas++;
                    break;

                case ESTADO_QUEIMADA:
                    s->results.queimadas++;
                    break;

                case ESTADO_CONTENCAO:
                    s->results.contencao++;
                    break;
            }
        }
    }
    return ignicoes;
}

static void trocar_arrays(Simulation* s){
    int* temp;

    temp = s->estado_atual;
    s->estado_atual = s->proximo_estado;
    s->proximo_estado = temp;

    temp = s->tempo_atual;
    s->tempo_atual = s->proximo_tempo;
    s->proximo_tempo = temp;
}

void simular_sequencial(Simulation* s){
    int passo = 0;

    // Inicializa os resultados acumulativos
    s->results.total_ignicoes = 0;
    s->results.pico_ignicoes = 0;
    s->results.passo_pico = -1;

    // F é a quantidade de focos iniciais
    s->results.em_chamas = s->config.F;

    /*
     Verifica se existe alguma célula em chamas
     após a inicialização.
    */
    if (s->results.em_chamas == 0)
        return;

    double start = omp_get_wtime();

    while(passo < s->config.P){
        // Ativa as zonas de contenção no passo atual.
        Ativar_zonas(s, passo);

        // Calcula o próximo estado da simulação.
        int ignicoes = Calcular_proximo_estado(s);

        // Atualiza a variavel acumulativa de ignições.
        s->results.total_ignicoes += ignicoes;
        
        // Atualiza o pico de ignições.
        if (ignicoes > s->results.pico_ignicoes) {
            s->results.pico_ignicoes = ignicoes;
            s->results.passo_pico = passo;
        }

        // Troca os arrays atual <-> próximo.
        trocar_arrays(s);

        passo++;

        // Se não há mais células em chamas, termina.
        if(s->results.em_chamas == 0)
            break;
    }
    double end = omp_get_wtime();

    s->results.passos = passo;
    s->results.tempo = fim - inicio;
}