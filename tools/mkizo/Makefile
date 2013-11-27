
CFLAGS=-ggdb

test: genizo
	./genizo test.izo


genizo: iso9660.o
	gcc -o $@ $^

mkizo: mkizo.o iso9660.o
	gcc -o $@ $^

clean:
	rm -f test.izo genizo mkizo *.o
