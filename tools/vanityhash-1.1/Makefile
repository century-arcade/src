PREFIX := /usr/local

all: vanityhash

# Docs are shipped pre-compiled
doc: vanityhash.1 vanityhash.1.html

vanityhash.1: vanityhash
	pod2man -c '' -r '' -s 1 $< >$@

vanityhash.1.html: vanityhash
	pod2html $< >$@
	rm -f pod2htmd.tmp pod2htmi.tmp

test:
	@perl -MGetopt::Long -e 'print "Getopt::Long is installed.\n";'
	@perl -MDigest -e 'print "Digest is installed.\n";'
	@perl -MIO::Select -e 'print "IO::Select is installed.\n";'
	@perl -MIO::Handle -e 'print "IO::Handle is installed.\n";'
	@perl -MSocket -e 'print "Socket is installed.\n";'
	@perl -MPOSIX -e 'print "POSIX is installed.\n";'
	@perl -MTime::HiRes -e 'print "Time::HiRes is installed.\n";'
	@perl -MDigest::MD2 -e 'print "Digest::MD2 is installed.\n";' 2>/dev/null || echo 'Digest::MD2 is not installed (but optional).'
	@perl -MDigest::MD4 -e 'print "Digest::MD4 is installed.\n";' 2>/dev/null || echo 'Digest::MD4 is not installed (but optional).'
	@perl -MDigest::MD5 -e 'print "Digest::MD5 is installed.\n";' 2>/dev/null || echo 'Digest::MD5 is not installed (but optional).'
	@perl -MDigest::SHA -e 'print "Digest::SHA is installed.\n";' 2>/dev/null || echo 'Digest::SHA is not installed (but optional).'
	@perl -MDigest::SHA1 -e 'print "Digest::SHA1 is installed.\n";' 2>/dev/null || echo 'Digest::SHA1 is not installed (but optional).'
	@perl -MDigest::CRC -e 'print "Digest::CRC is installed.\n";' 2>/dev/null || echo 'Digest::CRC is not installed (but optional).'
	@echo 'All tests complete.'

install: all
	install -d -m 0755 $(DESTDIR)$(PREFIX)/bin
	install -d -m 0755 $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 0755 vanityhash $(DESTDIR)$(PREFIX)/bin
	install -m 0644 vanityhash.1 $(DESTDIR)$(PREFIX)/share/man/man1

distclean: clean

clean:

doc-clean:
	rm -f vanityhash.1 vanityhash.1.html
