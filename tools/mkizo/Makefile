
CFLAGS=-ggdb

all: mkizo dumpizo

mkizo: mkizo.o zip_crc32.o
	gcc -o $@ $^

dumpizo: dumpizo.o
	gcc -o $@ $^

test: mkizo
	./mkizo -f -o test.izo .
	isovfy test.izo
	isoinfo -i test.izo -d -debug -f

clean:
	rm -f test.izo dumpizo mkizo *.o
