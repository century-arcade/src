CFLAGS= -O3 -DPOLARSSL_HAVE_SSE2 -DPOLARSSL_SHA1_C

vainhash: vainhash.o sha1.o

test: vainhash
	cp test.orig testfile
	time ./vainhash -p decade -w 1 testfile

clean:
	rm -f *.o vainhash
