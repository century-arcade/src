
#include <assert.h>
#include <sys/param.h> // MIN
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h> // fprintf
#include <stdlib.h> // exit
#include <fcntl.h> // O_RDWR
#include <getopt.h>

#include "sha1.h"

#define END_JUNK_SIZE sizeof(uint64_t)

static void hex_print(unsigned char *output, size_t n)
{
    size_t i;
    for (i=0; i < n; ++i)
    {
        fprintf(stderr, "%02x", output[i]);
    }
    fprintf(stderr, "\n");
}

int
main(int argc, char *const argv[])
{
    int opt;
    uint64_t vanityvalue = 0;
    size_t vanitysize = 2;
    int nworkers = 1;

    while ((opt = getopt(argc, argv, "p:w:")) != -1) {
        switch (opt) {
        case 'p': // hash prefix in hex
        {
            char *endptr = NULL;
            vanityvalue = strtoull(optarg, &endptr, 16);
            vanitysize = (endptr - optarg) / 2;
            break;
        }

        case 'w': // num workers
            nworkers = atoi(optarg);
            break;

        default: /* '?' */
            fprintf(stderr, "Usage: %s [-t nsecs] [-n] name\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        };
    };

    const char *fn = argv[optind];

    struct stat st;
    int fd = open(fn, O_RDWR | O_EXCL);
    if (fd < 0) {
        perror(fn);
        exit(EXIT_FAILURE);
    }

    if (fstat(fd, &st) < 0) {
        perror("fstat");
        exit(EXIT_FAILURE);
    }

    sha1_context *ctx = (sha1_context *) malloc(sizeof(sha1_context));
    sha1_starts(ctx);

    size_t bytes_left = st.st_size - END_JUNK_SIZE;
    while (bytes_left > 0)
    {
        char buf[4096];
        size_t maxreadsize = MIN(4096, bytes_left);
        ssize_t r = read(fd, buf, maxreadsize);
        if (r <= 0) {
//            fprintf(stderr, "read %u/%u bytes (%u bytes_left)", 
            perror("read()");
            exit(EXIT_FAILURE);
        }
        sha1_update(ctx, buf, r);
        bytes_left -= r;
    }

    char junk[END_JUNK_SIZE];

    if (read(fd, junk, END_JUNK_SIZE) < END_JUNK_SIZE) {
        perror("could not read junk??");
        exit(EXIT_FAILURE);
    }

    const char goldzero[END_JUNK_SIZE] = { 0 };
    assert(memcmp(goldzero, junk, END_JUNK_SIZE) == 0); // must be 0s to start

    // and rewind the junk to eventually overwrite it
    if (lseek(fd, -END_JUNK_SIZE, SEEK_END) < 0) {
        perror("lseek");
        exit(EXIT_FAILURE);
    }

    // save a copy of the context
    // (to get SHA1 of original file, update with goldzero and finish)
 
    sha1_context *humblectx = (sha1_context *) malloc(sizeof(sha1_context));
    memcpy(humblectx, ctx, sizeof(sha1_context));

    // show original hash
    unsigned char hash[20];

    sha1_update(ctx, goldzero, END_JUNK_SIZE);
    sha1_finish(ctx, hash);

    fprintf(stderr, "original SHA1 of %s: ", fn);
    hex_print(hash, 20);

    while (1) {
        // restore state
        memcpy(ctx, humblectx, sizeof(sha1_context));

        // try some junk
        sha1_update(ctx, junk, END_JUNK_SIZE);
        sha1_finish(ctx, hash);

        // if the hash begins with the vanity bytes, update the file and quit
        if (memcmp(hash, &vanityvalue, vanitysize) == 0) {
            fprintf(stderr, "\nwriting junk for vanity hash: ");
            hex_print(junk, END_JUNK_SIZE);

            fprintf(stderr, "\nfinal hash: ");
            hex_print(hash, 20);

            if (write(fd, junk, END_JUNK_SIZE) < END_JUNK_SIZE) {
                perror("junk write");
                exit(EXIT_FAILURE);
            }

            break;
        }

        unsigned long long iteration = ++(*(uint64_t *) junk);

        if (iteration % 1000000 == 0) {
            fprintf(stderr, "\r%llu", iteration);
        }
    }
  
    close(fd);

    return 0;
}
