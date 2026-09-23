FLAGS = -Wall -Wextra -Wpedantic -Iinclude

OBJECTS = input.o output.o

all: fire_seq fire_omp

fire_seq: $(OBJECTS) src/fire_seq.c
	gcc src/fire_seq.c $(OBJECTS) $(FLAGS) -fopenmp -o $@

fire_omp: $(OBJECTS) src/fire_omp.c
	gcc src/fire_omp.c $(OBJECTS) $(FLAGS) -fopenmp -o $@

input.o: src/input.c
	gcc $(FLAGS) -c src/input.c -o input.o

output.o: src/output.c
	gcc $(FLAGS) -c src/output.c -o output.o

clean:
	rm -f fire_seq fire_omp *.o