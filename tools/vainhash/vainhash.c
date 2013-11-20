
#include <assert.h>
#include <sys/param.h> // MIN
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h> // fprintf
#include <stdlib.h> // exit
#include <fcntl.h> // O_RDWR
#include <getopt.h>
#include <signal.h>

#include "sha1.h"

static volatile int done = 0;

static void child_exited(int signum)
{
    ++done;
}

static void hex_print(const unsigned char *output, size_t n)
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
    char vanityvalue[8] = { 0 };
    size_t vanitysize = 2;
    int num_workers = 1;
    int force = 0;

    while ((opt = getopt(argc, argv, "p:w:f")) != -1) {
        switch (opt) {
        case 'f': // force update even if junk data is non-zero
            force = 1;
            break;

        case 'p': // hash prefix in hex
        {
            char *endptr = NULL;
            uint64_t v = strtoull(optarg, &endptr, 16);
            vanitysize = (endptr - optarg) / 2;
            int i;
            for (i=vanitysize; i > 0; --i) {
                vanityvalue[i-1] = v & 0xff;
                v >>= 8;
            }
            fprintf(stderr, "searching for ");
            hex_print(vanityvalue, vanitysize);
            break;
        }

        case 'w': // num workers
            num_workers = atoi(optarg);
            break;

        default: /* '?' */
            fprintf(stderr, "Usage: %s [-p <hex_prefix>] [-w <num-workers>] filename\n",
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

    uint64_t junk = 0;

    size_t bytes_left = st.st_size - sizeof(junk);
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

    if (read(fd, &junk, sizeof(junk)) < sizeof(junk)) {
        perror("could not read pre-existing junk??");
        exit(EXIT_FAILURE);
    }

    if (!force && junk != 0) {
        fprintf(stderr, "existing junk is non-zero!  use -f to force\n");
        exit(EXIT_FAILURE);
    }

    // save a copy of the context
    // (to get SHA1 of original file, update with goldzero and finish)
 
    sha1_context *humblectx = (sha1_context *) malloc(sizeof(sha1_context));
    memcpy(humblectx, ctx, sizeof(sha1_context));

    // show original hash
    unsigned char hash[20];

    sha1_update(ctx, (const unsigned char *) &junk, sizeof(junk));
    sha1_finish(ctx, hash);

    fprintf(stderr, "original SHA1 of %s: ", fn);
    hex_print(hash, 20);

    // spawn additional workers
    signal(SIGCHLD, child_exited);
    pid_t children[1024] = { 0 };
    int w;
    for (w=1; w < num_workers; ++w) 
    {
        pid_t child = fork();
        if (child < 0) {
            perror("fork");
            break;
            // don't need to exit, just let the existing thread(s) work
        } else if (child == 0) { // worker #w
            break;
        } else { // parent
            children[w-1] = child;
            // continue spawning, then fall through to do our own work
        }
    }

    // save start time
    struct timeval tv_start;
    gettimeofday(&tv_start, NULL);

    // pre-populate junk (so restarted runs don't re-do the same junk)
    junk = (tv_start.tv_sec << 32) | tv_start.tv_usec;

    // give each worker its own 56-bit segment of the space.
    junk &= 0xff00000000000000ULL;
    junk |= ((uint64_t) w) << 56;

    unsigned long long num_tried = 0;

    while (! done) {
        // restore state
        memcpy(ctx, humblectx, sizeof(sha1_context));

        // try some junk
        sha1_update(ctx, (const unsigned char *) &junk, sizeof(junk));
        sha1_finish(ctx, hash);

        // if the hash begins with the vanity bytes, update the file and quit
        if (memcmp(hash, &vanityvalue, vanitysize) == 0) {
            fprintf(stderr, "\n[worker %d] writing junk for vanity hash: ", w);
            hex_print((const unsigned char *) &junk, sizeof(junk));

            fprintf(stderr, "\nfinal hash: ");
            hex_print(hash, 20);

            if (lseek(fd, -sizeof(junk), SEEK_END) < 0) {
                perror("lseek");
                exit(EXIT_FAILURE);
            }

            if (write(fd, &junk, sizeof(junk)) < sizeof(junk)) {
                perror("junk write");
                exit(EXIT_FAILURE);
            }

            break;
        }

        ++num_tried;
        ++junk;

        if ((junk & 0x00fffff) == (w << 19)) {
            struct timeval tv_now;
            gettimeofday(&tv_now, NULL);

            int elapsed_secs = tv_now.tv_sec - tv_start.tv_sec;
            int elapsed_usecs = tv_now.tv_usec - tv_start.tv_usec;
            if (elapsed_usecs < 0) {
                elapsed_secs -= 1;
                elapsed_usecs += 1000000;
            }
            // assume other workers are getting as many done as we are
            unsigned int rate = 0;
            
            if (elapsed_secs > 0)
                rate = num_tried * num_workers / elapsed_secs;

            fprintf(stderr, "[worker %d] %llu in %ds; %u/s\r", w, num_tried, elapsed_secs, rate);
        }
    }

    if (w == num_workers) { // if we're the parent
        for (w=0; w < num_workers-1; ++w) {
            kill(children[w], SIGCHLD);
        }
    }
  
    close(fd);

    return 0;
}
