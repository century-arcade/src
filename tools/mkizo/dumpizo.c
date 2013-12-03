#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "iso9660.h"

#define STAT stat
#define SECTOR(X) (&iso[X * SECTOR_SIZE])
#define LOG(FMTSTR, args...) fprintf(stderr, FMTSTR "\n", ## args)
#define PROBLEM_IF_NEQ(A, B) do { \
    if ((A) != (B)) { \
        LOG("!!!! %s (%d) != %s (%d)", #A, A, #B, B); \
    } else if (debug) { \
        LOG("(OK) %s == %s == %d", #A, #B, B); \
    } \
} while (0)

#define PROBLEM_IF_EQ(A, B) do { \
    if ((A) == (B)) { \
        LOG("!!!! %s (%d) should not == %s (%d)", #A, A, #B, B); \
    } else if (debug) { \
        LOG("(OK) %s (%d) != %s %d", #A, A, #B, B); \
    } \
} while (0)

static int debug = 0;
const char *iso = NULL;

void dump_record(const char *parentpath, const DirectoryRecord *r)
{
    char fn[256] = { 0 };

    if (parentpath) {
        strcpy(fn, parentpath);
        strcat(fn, "/");
    }

    PROBLEM_IF_EQ(r->id_len, 0);
    PROBLEM_IF_EQ(r->data_sector, 0);

    int dosubdir = (parentpath == NULL);

    if (r->id[0] == 0x00) {
        strcat(fn, ".");
    } else if (r->id[0] == 0x01) {
        strcat(fn, "..");
    } else {
        strncat(fn, r->id, r->id_len);
        dosubdir = 1;
    }

    LOG("%s: sector %u, size %u", fn, r->data_sector, r->data_len);

    if (dosubdir && (r->flags & ISO_DIRECTORY))
    {
        const char *dirbuf = SECTOR(r->data_sector);

        size_t i=0;
        while (i < r->data_len)
        {
            const DirectoryRecord *f = (const DirectoryRecord *) &dirbuf[i];

            dump_record(fn, f);

            i += f->record_len;
        }
    }
}

int main(int argc, char **argv)
{
    const char *fn = argv[1];
    struct stat st;

    STAT(fn, &st);

    int fd = open(fn, O_RDONLY);
    if (fd < 0) {
        perror(fn);
        return -1;
    } 

    iso = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    const PrimaryVolumeDescriptor *pvd = (const PrimaryVolumeDescriptor *) SECTOR(16);

    const DirectoryRecord *rootrec = &pvd->root_directory_record;

    PROBLEM_IF_NEQ(rootrec->record_len, (int) sizeof(DirectoryRecord) + 1);

    dump_record(NULL, rootrec);

    return 0;
}

