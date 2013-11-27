#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include <sys/mman.h> // mmap
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>    // O_RDONLY
#include <unistd.h>

#include "iso9660.h"
#include "ziphdr.h"

FILE *fpizo = NULL;

size_t nrootfiles = 1;  // root[0] is rootdir itself
DirectoryRecord *root[32];
size_t rootsize = 2*sizeof(DirectoryRecord) + 2; // . and ..

// include 8 byte 'comment' for vanityhashing
#define IZO_END_RESERVE 8
size_t zip_cdir_len = sizeof(ZipEndCentralDirRecord) + IZO_END_RESERVE;
size_t nzipfiles = 0;   // number of files in zip
ZipCentralDirFileHeader *ziphdrs[32];

static const int MB = 1024 * 1024;
#define MAX_ID_LEN 31

int g_num_sectors = 16; // after SystemArea
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
    int sector = alloc_sectors(nsectors+1);

    int fd = open(localfn, O_RDONLY);
    if (fd < 0) {
        perror(localfn);
        return -1;
    } 
  
    const char *contents =
            mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    // how hard to combine the copy and the crc?
    uint32_t crc = crc32(contents, st.st_size);

    FWRITEAT(fpizo, (sector+1) * SECTOR_SIZE, contents, st.st_size);

    munmap((void *) contents, st.st_size);
    close(fd);

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

    size_t fidx = nrootfiles++;
    root[fidx] = r;
    rootsize += r->record_len;

    // construct ZIP local and central dir headers

    size_t chdrlen = sizeof(ZipCentralDirFileHeader) + id_len;
    ZipCentralDirFileHeader * chdr = (ZipCentralDirFileHeader *) malloc(chdrlen);
    memset(chdr, 0, chdrlen);

    size_t lhdrlen = sizeof(ZipLocalFileHeader) + id_len;
    ZipLocalFileHeader * lhdr = (ZipLocalFileHeader *) alloca(lhdrlen);
    memset(lhdr, 0, lhdrlen);

    chdr->signature = 0x02014b50;
    lhdr->signature = 0x04034b50;
    chdr->date = lhdr->date = 0x00;
    chdr->time = lhdr->time = 0x00;
    chdr->crc32 = lhdr->crc32 = crc;
    chdr->comp_size = chdr->uncomp_size = 
        lhdr->comp_size = lhdr->uncomp_size = st.st_size;
    chdr->filename_len = lhdr->filename_len = id_len;

    chdr->local_header_ofs = (sector + 1) * SECTOR_SIZE - lhdrlen;
    strcpy(chdr->filename, isofn);
    strcpy(lhdr->filename, isofn);

    // write zip local header
    FWRITEAT(fpizo, chdr->local_header_ofs, lhdr, lhdrlen);

    // save off central dir header for later
    ziphdrs[nzipfiles++] = chdr;
    zip_cdir_len += chdrlen;

    return 0;
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

    SET16_LSBMSB(*pvd, volume_set_size, 1);
    SET16_LSBMSB(*pvd, volume_sequence_number, 1);
    SET16_LSBMSB(*pvd, logical_block_size, 2048);

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
    int pvd_sector = alloc_sectors(1);   // Primary Volume Descriptor
    assert(pvd_sector == 16);
//    int br_sector = alloc_sectors(1);   // Boot Record
    int vdst_sector = alloc_sectors(1); // Volume Descriptor Set Terminator

    root[0] = mkroot();

    mkfile("iso9660.h", "ISO9660.H");

    // might need more than 1, but alloc_sectors won't be used after this
    int rootsector = alloc_sectors(1);

    root[0]->record_len = sizeof(DirectoryRecord) + 1;
    SET32_LSBMSB(*root[0], data_sector, rootsector);
    SET32_LSBMSB(*root[0], data_len, rootsize);

    // write the PVD
 
    PrimaryVolumeDescriptor pvd;
    fill_pvd(&pvd);
    memcpy(&pvd.root_directory_record, root[0], sizeof(DirectoryRecord));

    FWRITEAT(fpizo, pvd_sector * SECTOR_SIZE, &pvd, sizeof(pvd));

    FWRITEAT(fpizo, vdst_sector * SECTOR_SIZE, VolumeDescriptorSetTerminator, sizeof(VolumeDescriptorSetTerminator));

    // write the root/. entry
    FWRITEAT(fpizo, rootsector * SECTOR_SIZE, root[0], root[0]->record_len);

    root[0]->id[0] = 0x01; // root/..

    size_t r;
    for (r=0; r < nrootfiles; ++r)
    {
        FWRITE(fpizo, root[r], root[r]->record_len);
    }

    long endpos = ftell(fpizo);
    long bytes_left_this_mb = MB - (endpos % MB);
    if (bytes_left_this_mb < zip_cdir_len) {
        endpos += MB;
    }
    endpos += bytes_left_this_mb;

    FSEEK(fpizo, endpos - zip_cdir_len, SEEK_SET);

    for (r=0; r < nzipfiles; ++r)
    {
        FWRITE(fpizo, ziphdrs[r], sizeof(ZipCentralDirFileHeader) + ziphdrs[r]->filename_len);
    }

    ZipEndCentralDirRecord endcdir;
    memset(&endcdir, 0, sizeof(endcdir));

    endcdir.signature = 0x06054b50;
    endcdir.central_dir_start = endpos - zip_cdir_len;
    endcdir.central_dir_bytes = zip_cdir_len - sizeof(endcdir) - IZO_END_RESERVE;
    endcdir.disk_num_records = endcdir.total_num_records = nzipfiles;
    endcdir.comment_len = IZO_END_RESERVE;

    char zerobuf[IZO_END_RESERVE] = { 0 };
    FWRITE(fpizo, &endcdir, sizeof(endcdir));
    FWRITE(fpizo, zerobuf, IZO_END_RESERVE);

    fclose(fpizo);
    return 0;
}

