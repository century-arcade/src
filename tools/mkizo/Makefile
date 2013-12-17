
CFLAGS=-ggdb

all: mkizo dumpizo

mkizo: mkizo.o zip_crc32.o
	$(CC) -o $@ $^

dumpizo: dumpizo.o
	$(CC) -o $@ $^

test: mkizo
	./mkizo -v -f -o test.izo .
	isovfy test.izo
	isoinfo -i test.izo -d -debug -f
	zip -Tv test.izo

clean:
	rm -f test.izo dumpizo mkizo *.o
