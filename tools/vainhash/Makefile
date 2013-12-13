CFLAGS= -ggdb -O3 -DPOLARSSL_HAVE_SSE2 -DPOLARSSL_SHA1_C

vainhash: vainhash.o sha1.o

test: vainhash
	uname -a > testfile
	truncate --size=+8 testfile
	time ./vainhash -p beef -w 8 testfile

clean:
	rm -f *.o vainhash
