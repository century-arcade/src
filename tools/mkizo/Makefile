
CFLAGS=-ggdb

test: mkizo
	./mkizo -o test.izo .
	isovfy test.izo
	isoinfo -i test.izo -d -debug -f

mkizo: mkizo.o zip_crc32.o
	gcc -o $@ $^

clean:
	rm -f test.izo mkizo *.o
