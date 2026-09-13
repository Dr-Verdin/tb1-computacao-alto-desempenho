FLAGS = -Wall -Wextra -Wpedantic -Iinclude -std=c11

OBJECTS = input.o output.o

fire_seq: $(OBJECTS) src/fire_seq.c
	gcc src/fire_seq.c $(OBJECTS) $(FLAGS) -fopenmp -o $@

fire_omp: $(OBJECTS) src/fire_omp.c
	gcc src/fire_omp.c $(OBJECTS) $(FLAGS) -fopenmp -o $@

input.o:
	gcc $(FLAGS) src/input.c -c input.o

output.o:
	gcc $(FLAGS) src/output.c -c output.o

clean:
	rm -f fire_seq fire_omp *.o