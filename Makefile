FLAGS = -Wall -Wextra -Wpedantic -Iinclude

OBJECTS = input.o output.o

fire_seq: $(OBJECTS) src/fire_seq.c
	gcc src/fire_seq.c $(OBJECTS) $(FLAGS) -fopenmp -o $@

fire_omp: $(OBJECTS) src/fire_omp.c
	gcc src/fire_omp.c $(OBJECTS) $(FLAGS) -fopenmp -o $@

input.o:
	gcc $(FLAGS) -c src/input.c -o input.o

output.o:
	gcc $(FLAGS) -c src/output.c -o output.o

run: fire_seq fire_omp
	mkdir -p results/static
	mkdir -p results/dynamic
	mkdir -p results/guided
	mkdir -p results/sequential

	for input in tests/*.txt; do \
		name=$$(basename $$input .txt); \
		echo "Executando $$name..."; \
		\
		OMP_SCHEDULE="static" ./fire_omp $$input > results/static/$$name.txt; \
		OMP_SCHEDULE="dynamic" ./fire_omp $$input > results/dynamic/$$name.txt; \
		OMP_SCHEDULE="guided" ./fire_omp $$input > results/guided/$$name.txt; \
		\
		./fire_seq $$input > results/sequential/$$name.txt; \
	done

	echo "Todos os testes foram executados."

clean:
	rm -f fire_seq fire_omp *.o