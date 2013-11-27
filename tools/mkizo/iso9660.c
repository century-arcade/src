#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "iso9660.h"

const int PVD_SECTOR = 16;

FILE *fpizo = NULL;

size_t next_record = 1;
DirectoryRecord *root[32];
size_t rootsize = 2*sizeof(DirectoryRecord) + 2; // . and ..

#define MAX_ID_LEN 31

int g_num_sectors = 17; // after SystemArea and PVD
int alloc_sectors(size_t n)
{
    int r = g_num_sectors;
    g_num_sectors += n;
    return r;
}

#define SET16_LSBMSB(R, F, V)  \
    do { uint16_t val = (V); (R).F = val; (R).msb_##F = htons(val); } while (0)
#define SET32_LSBMSB(R, F, V)  \
    do { uint32_t val = (V); (R).F = val; (R).msb_##F = htonl(val); } while (0)

// returns pointer to remainder of src, or to null byte if none
const char *strncpypad(char *dest, const char *src, size_t destsize, char padch)
{
    while (destsize > 0 && *src) {
        *dest++ = *src++;
        destsize--;
    }

    while (destsize > 0) {
        *dest++ = padch;
        destsize--;
    }

    return src;
}

void
setdate(DecimalDateTime *ddt, const char *strdate)
{
    // dates must be like "2078123123595999+0800"
    //      or truncated (remainder are filled with '0's)
    // (Decimal Date Time verbatim + gmt_offset which is parsed)

    const char *leftover = strncpypad((char *) ddt, strdate, sizeof(*ddt)-1, '0');
    if (*leftover == '+' || *leftover == '-') {
        ddt->gmt_offset = atoi(leftover);
    } else {
        ddt->gmt_offset = 0;
    }
}

void izo_fill_dir_time(DirectoryRecord *dr, time_t t)
{
    if (t == 0) return;

    struct tm tm;
    gmtime_r(&t, &tm);
    dr->years_since_1900 = tm.tm_year - 1900;
    dr->month = tm.tm_mon + 1;
    dr->day = tm.tm_mday + 1;
    dr->hour = tm.tm_hour;
    dr->minute = tm.tm_min;
    dr->second = tm.tm_sec;
//    dr->gmt_offset = 0; // XXX: possible to set the timezone from stat info?
}

#define FSEEK fseek
#define STAT stat
#define FWRITE(FP, PTR, LEN) \
    if (fwrite(PTR, LEN, 1, FP) != 1) { perror("fwrite" #PTR); return -1; }

#define FWRITEAT(FP, POS, PTR, LEN) do { \
    FSEEK(FP, POS, SEEK_SET); \
    FWRITE(FP, PTR, LEN); \
} while (0)

static inline int sectors(int nbytes)
{
    int nsectors = nbytes / SECTOR_SIZE;
    if (nbytes % SECTOR_SIZE > 0) nsectors += 1;
    return nsectors;
}

int mkfile(const char *localfn, const char *isofn)
{
    struct stat st;

    STAT(localfn, &st);

    int nsectors = sectors(st.st_size);
    int sector = alloc_sectors(nsectors);

    FSEEK(fpizo, sector * SECTOR_SIZE, SEEK_SET);

    FILE *fp = fopen(localfn, "rb");
    if (fp == NULL) {
        perror(localfn);
        return -1;
    } 
   
    char buf[1024];
    size_t ncopied = 0;
    while (1) {
        size_t r = fread(buf, 1, sizeof(buf), fp);
        if (r <= 0) {
            if (!feof(fp)) {
                perror(localfn);
            }
            break;
        }
        FWRITE(fpizo, buf, r);
        ncopied += r;
    }
    assert(ncopied == st.st_size);

    size_t id_len = strlen(isofn);

    DirectoryRecord *r = (DirectoryRecord *) malloc(sizeof(DirectoryRecord) + id_len);

    int rlen=sizeof(DirectoryRecord) + id_len;
    memset(r, 0, rlen);

    // 6.8.1.1: "Each subsequent Directory Record recorded in that Logical
    // Sector shall begin at the byte immediately following the last byte
    // of the preceding Directory Record in that Logical Sector. Each
    // Directory Record shall end in the Logical Sector in which it
    // begins."
    int bytes_left_in_sector = SECTOR_SIZE - (rootsize % SECTOR_SIZE);
    if (sizeof(DirectoryRecord) + MAX_ID_LEN > bytes_left_in_sector) {
        rlen += bytes_left_in_sector;
    }

    r->record_len = rlen;

    SET32_LSBMSB(*r, data_sector, sector);
    SET32_LSBMSB(*r, data_len, st.st_size);

    // volume_seq_num/volume_set_size is always 1/1
    SET16_LSBMSB(*r, volume_seq_num, 1);

//    izo_fill_dir_time(sb.entry, time(NULL));

    r->id_len = id_len;
    strcpy(r->id, isofn);

    root[next_record++] = r;
    rootsize += r->record_len;
}

DirectoryRecord * mkroot()
{
    size_t id_len = 1;

    DirectoryRecord *r =
        (DirectoryRecord *) malloc(sizeof(DirectoryRecord) + id_len);

    // will also set id[0] to 0x0 if fn == NULL (as for root dir)
    memset(r, 0, sizeof(DirectoryRecord) + id_len);

    r->record_len = sizeof(DirectoryRecord) + id_len;

    // volume_seq_num/volume_set_size is always 1/1
    SET16_LSBMSB(*r, volume_seq_num, 1);
//    izo_fill_dir_time(r, time(NULL));

    r->flags |= ISO_DIRECTORY;
    r->id_len = id_len;

//    if (fn != NULL) { strcpy(r->id, fn); }

    return r;
}

#define E(X) #X // TODO: getenv/config
#define EINT(X) atoi(getenv(#X))

int
fill_pvd(PrimaryVolumeDescriptor *pvd)
{
    memset(pvd, 0, sizeof(*pvd));

    pvd->type = VDTYPE_PRIMARY;
    memcpy(pvd->id, "CD001", 5);
    pvd->version = 0x01;

    strncpypad(pvd->system_id, E(system_id), 32, ' ');
    strncpypad(pvd->volume_id, E(volume_id), 32, ' ');

    SET16_LSBMSB(*pvd, volume_set_size, 1); // EINT(volume_set_size));
    SET16_LSBMSB(*pvd, volume_sequence_number, 1); // EINT(volume_sequence_number));
    SET16_LSBMSB(*pvd, logical_block_size, 2048);

    // XXX: can only write out the PVD when we've gone over all files in the ISO already (and thus know how big the ISO and its path tables need to be).  Also where the root directory is, although that could just be at a fixed location (after boot catalog?)
    SET32_LSBMSB(*pvd, num_sectors, g_num_sectors);

    SET32_LSBMSB(*pvd, path_table_size, 0);

    pvd->lsb_path_table_sector = 0;
    pvd->msb_path_table_sector = 0;
    pvd->lsb_alt_path_table_sector = 0;
    pvd->msb_alt_path_table_sector = 0;

    strncpypad(pvd->volume_set_id, E(volume_set_id), 128, ' ');
    strncpypad(pvd->publisher_id, E(publisher_id), 128, ' ');
    strncpypad(pvd->preparer_id, E(preparer_id), 128, ' ');
    strncpypad(pvd->application_id, E(application_id), 128, ' ');
    strncpypad(pvd->copyright_file_id, E(copyright_file_id), 37, ' ');
    strncpypad(pvd->abstract_file_id, E(abstract_file_id), 37, ' ');
    strncpypad(pvd->bibliographical_file_id, E(bibliographical_file_id), 37, ' ');

    setdate(&pvd->creation_date, E(creation_date));
    setdate(&pvd->modification_date, E(modification_date));
    setdate(&pvd->expiration_date, E(expiration_date));
    setdate(&pvd->effective_date, E(effective_date));

    pvd->file_structure_version = 0x01;
}

int main(int argc, char **argv)
{
    const char *fn = argv[1];
    unlink(fn);

    fpizo = fopen(fn, "w+b");

    root[0] = mkroot();

    mkfile("iso9660.h", "ISO9660.H");

    // might need more than 1, but alloc_sectors won't be used after this
    int rootsector = alloc_sectors(1);

    root[0]->record_len = sizeof(DirectoryRecord) + 1;
    SET32_LSBMSB(*root[0], data_sector, rootsector);
    SET32_LSBMSB(*root[0], data_len, rootsize);

    // write the root/. entry
    FWRITEAT(fpizo, rootsector * SECTOR_SIZE, root[0], root[0]->record_len);

    root[0]->id[0] = 0x01; // root/..

    size_t r;
    for (r=0; r < next_record; ++r)
    {
        FWRITE(fpizo, root[r], root[r]->record_len);
    }

    PrimaryVolumeDescriptor pvd;
    fill_pvd(&pvd);
    memcpy(&pvd.root_directory_record, root[0], sizeof(DirectoryRecord));

    FWRITEAT(fpizo, PVD_SECTOR * SECTOR_SIZE, &pvd, sizeof(pvd));

    fclose(fpizo);
    return 0;
}

