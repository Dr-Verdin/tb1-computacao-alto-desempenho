// Config → parâmetros da simulação;
// Cell → cobertura e umidade;
// Zone → informações das zonas;
// Simulation → todas as estruturas necessárias durante a simulação;
// Results → resultados que serão impressos no final;
// constantes para os códigos de cobertura e estado.

#ifndef SIMULATION_H
#define SIMULATION_H

// Códigos das coberturas:
#define COBERTURA_AGUA 0
#define COBERTURA_SOLO 1
#define COBERTURA_VEGETACAO 2
#define COBERTURA_FLORESTA 3

// Códigos dos estados:
#define ESTADO_NAO_COMBUSTIVEL 0
#define ESTADO_INTACTA 1
#define ESTADO_CHAMAS 2
#define ESTADO_QUEIMADA 3
#define ESTADO_CONTENCAO 4

#endif // SIMULATION_H

typedef struct Config
{
    // Primeira linha:
    int L;             // número de linhas da matriz
    int C;             // número de colunas da matriz
    int P;             // número de passos da simulação
    int T;             // número de threads
    unsigned int seed; // semente para geração de números aleatórios
    int limiar;      // limiar de propagação do fogo

    // Segunda linha:
    int vento_linha;  // direção do vento na linha
    int vento_coluna; // direção do vento na coluna
    int intensidade;            // intensidade do vento

    // Terceira linha:
    int F; // número de focos de incêndio
    int Z; // número de zonas de contenção
} Config;

typedef struct Cell
{
    int cobertura; // código da cobertura: valoro nome da variavel?
    int umidade;   // umidade do terreno
} Cell;

typedef struct
{
    int passo_ativacao; // passo em que a zona foi ativada
    int linha_inicial;  // linha inicial da zona
    int coluna_inicial; // coluna inicial da zona
    int linha_final;    // linha final da zona
    int coluna_final;   // coluna final da zona
} Zone;

typedef struct
{
    Config config;  // parâmetros da simulação
    Cell *cells;    // informações de cada célula (cobertura e umidade)
    Zone *zonas;    // vetor contendo as zonas de contenção

    int *estado_atual;   // estado atual de cada célula
    int *proximo_estado; // próximo estado de cada célula

    int *tempo_atual;   // tempo de queima restante no passo atual
    int *proximo_tempo; // tempo de queima que será calculado para o próximo passo

    int *ativacao;                              // passo de ativação da zona que cobre cada célula; -1 indica que nenhuma zona cobre a célula
    long long celulas_combustiveis_iniciais;    // número de células inicialmente combustíveis (vegetação + floresta)
} Simulation;

typedef struct {
    long long passos;   // número de passos da simulação

    long long nao_combustiveis; // número de células não combustíveis (água + solo)
    long long intactas;         // número de células intactas (vegetação + floresta)
    long long em_chamas;        // número de células em chamas
    long long queimadas;        // número de células queimadas
    long long contencao;        // número de células em contenção

    long long total_ignicoes;   // número total de ignições por propagação durante a simulação

    long long pico_passo;       // passo com maior número de novas ignições
long long pico_quantidade;      // número de novas ignições nesse passo

    double percentual_queimado;     // percentual de células queimadas em relação ao total de células inicialmente combustíveis
    double percentual_protegido;    // percentual de células protegidas em relação ao total de células inicialmente combustíveis

    unsigned long long checksum;    // soma de verificação (checksum) do estado final da simulação

    double tempo;   // tempo total de execução da simulação em segundos
} Result;