# vainhash: a SHA1 vanity hasher in C

Searches for some 'junk' data to replace 8 bytes of zeroed data at the end of
the given file, so the SHA1 hash starts with a given hex value.  In essence,
creating a 'vanity hash'.

## Usage

`vainhash [-p <hash_prefix> ] [ -w <num_workers> ] [-f] <file>`

* -p specifies the desired prefix (in hex)
* -w spawns additional processes to search in parallel
* -f forces the file to be modified even if the last 8 bytes aren't zeroes.

## Credits

* `vainhash` originally written by Saul Pwanson (2013)

## See Also

* [`vanityhash`](http://www.finnie.org/software/vanityhash/) by [Ryan Finnie](http://www.finnie.org).  Written in Perl; appends instead of modifying in-place.

# The SHA1 hash for this README starts with `ADDEDFEE`.

wPr5FLDYpV

