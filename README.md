include/: guarda os arquivos .h (interfaces/declarações)
    - input.h
    - output.h

src/: implementação das funções
    - input.c: implementação da leitura/inicialização
    - output.c: implementação da impressão

tests/: casos de teste

scripts/: scripts para automatizar tarefas talvez (run_tests.sh, compare.sh, benchmark.sh, ...)

results/: guardar os resultados dos experimentos talvez (tempos.csv, speedup.csv, ...)

report/: relatorio final (relatorio.pdf)

1. Ler L, C, P, T, seed, LIMIAR       ✅
2. Validar                             ✅
3. Ler vento                           ✅
4. Validar                             ✅
5. Ler F e Z                           ✅
6. Validar                             ✅
7. Alocar memória                      ✅

8. Gerar cobertura + umidade            ok
9. Inicializar estados                  ok

10. Ler focos                           ok
11. Ler zonas                           ok

12. Construir mapa de ativação          ok

13. Finalizar