/* The MIT License (MIT)

Copyright (c) 2013 The Century Arcade.  All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.
*/

#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <libgen.h> // dirname, basename
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/param.h> // MIN
#include <sys/mman.h> // mmap
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>    // O_RDONLY
#include <ftw.h>

#include "iso9660.h"
#include "ziphdr.h"

FILE *fpizo = NULL;
const char *sourcepath = NULL;
int verbose = 0; // -v bumps verbosity (only LOG and VERBOSE for now)
int force = 0;   // -f unlinks an existing file

#define MAX_FILES 256

typedef struct ISODir {
    const char *name_in_parent;    // just the basename
    struct ISODir *parent;
    DirectoryRecord *realrecord;   // same ptr as in parent.records[]

    DirectoryRecord *records[MAX_FILES]; // [0] is self, [1] is parent
    size_t nrecords;

    struct ISODir *subdirs[MAX_FILES];
    size_t nsubdirs;

    int pte_num;
} ISODir;

ISODir root;

u32 MSDOSDateTime(time_t t)
{
    struct tm *tm = localtime(&t);

    return (((tm->tm_year - (1980 - 1900)) & 0x7f) << 25)
         | ((tm->tm_mon+1) << 21) 
         | (tm->tm_mday << 16) 
         | (tm->tm_hour << 11) 
         | (tm->tm_min << 5) 
         | (tm->tm_sec);
}

#define LOG(FMTSTR, args...) fprintf(stderr, FMTSTR "\n", ## args)
#define VERBOSE(FMTSTR, args...) if (verbose) { LOG("%s: " FMTSTR, __FUNCTION__, ##args); }

size_t zip_cdir_len = sizeof(ZipEndCentralDirRecord);
size_t nzipfiles = 0;   // number of files in zip
ZipCentralDirFileHeader *ziphdrs[MAX_FILES];

PathTableEntry *paths[MAX_FILES];
size_t npaths = 0;
size_t pathtablesize = 0; // keep a running tally
int64_t blockdevlen = -1; // default is regular file

const char* bootfn = NULL;
int bootloader_sector = 0; // where the bootfn is stored

static const int MB = 1024 * 1024;
#define MAX_ID_LEN 31

static int g_num_sectors = 16; // after SystemArea
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

u16 checksum(const u8 *data, size_t nbytes)
{
    const u16 *words = (const u16 *) data;
    u16 sum = 0;

    size_t i;
    for (i=0; i < nbytes/sizeof(u16); ++i) {
        sum += words[i];
    }
    return sum;
}

u32 checksum32(const u8 *data, size_t nbytes)
{
    const u32 *words = (const u32 *) data;
    u32 sum = 0;

    size_t i;
    for (i=0; i < nbytes/sizeof(u32); ++i) {
        sum += words[i];
    }
    assert(i * sizeof(u32) == nbytes);
    return sum;
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

// generates a strdate from a time_t for use with setdate
char * strdate(time_t t) {
    static char buf[256];
    struct tm *tm = localtime(&t);
    strftime(buf, sizeof(buf), "%Y%m%d%H%M%S00%z", tm);
    return buf;
}

void izo_fill_dir_time(DirectoryRecord *dr, time_t t)
{
    if (t == 0) { 
        return;
    }

    struct tm tm;
    gmtime_r(&t, &tm);
    dr->years_since_1900 = tm.tm_year;
    dr->month = tm.tm_mon + 1;
    dr->day = tm.tm_mday;
    dr->hour = tm.tm_hour;
    dr->minute = tm.tm_min;
    dr->second = tm.tm_sec;
//    dr->gmt_offset = 0; // XXX: can't set the timezone from stat info
}

#define FSEEK fseek
#define STAT stat
#define FWRITE(FP, PTR, LEN) \
    if (fwrite(PTR, LEN, 1, FP) != 1) { perror("fwrite(" #PTR ")"); return -1; }

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

// modifies string in-place
char *strtoupper(char *src)
{
    char *c;

    if (src == NULL) {
        return;
    }

    for (c = src; *c; ++c) {
        *c = toupper(*c);
    }

    return src;
}

// caller responsible to free() dirfn and leaffn
static void parsepath(const char *path, char **dirfn, char **leaffn)
{
    char *duppath = strdup(path);
    const char *dn = dirname(duppath);
    if (strcmp(dn, ".") == 0) {
        *dirfn = NULL;
    } else {
        *dirfn = strdup(dn);
    }
    free(duppath);

    duppath = strdup(path);
    *leaffn = strdup(basename(duppath));
    free(duppath);
}

ISODir * find_isodir(const char *dirpath)
{
    if (dirpath == NULL || strcmp(dirpath, ".") == 0) {
        return &root;
    }

    char *parentdir = NULL;
    char *dirfn = NULL;
    parsepath(dirpath, &parentdir, &dirfn);

    ISODir * parent = find_isodir(parentdir);

    if (parent == NULL) {
        LOG("directory '%s' could not be found (searching for '%s')", parentdir, dirpath);
        return NULL;
    }

    size_t i;
    for (i=0; i < parent->nsubdirs; ++i) {
        if (strcmp(dirfn, parent->subdirs[i]->name_in_parent) == 0) {
            return parent->subdirs[i];
        }
    }

    LOG("'%s' could not be found in parent '%s' (looking for %s)", dirfn, parentdir, dirpath);

    // XXX: other return paths leak these
    free(dirfn);
    free(parentdir);
    return NULL;
}

DirectoryRecord *newrecord(const char *fn)
{
    size_t id_len = strlen(fn);
    if (id_len == 0) {
        id_len = 1;  // 'self', copy the null as the id
    }

    DirectoryRecord *r =
        (DirectoryRecord *) malloc(sizeof(DirectoryRecord) + id_len + 1);

    // will also set id[0] to 0x0 if fn == NULL (as for root dir)
    memset(r, 0, sizeof(DirectoryRecord) + id_len);

    r->record_len = sizeof(DirectoryRecord) + id_len;

    // volume_seq_num/volume_set_size is always 1/1
    SET16_LSBMSB(*r, volume_seq_num, 1);

    r->id_len = id_len;

    if (fn != NULL) { 
        strcpy(r->id, fn);
    }

    return r;
}

DirectoryRecord *newdirrecord(const char *dirname)
{
    DirectoryRecord *d = newrecord(dirname);
    d->flags |= ISO_DIRECTORY;
    return d;
}

ISODir * mkisodir(const char *dirname, const char *basename)
{
    ISODir *d; 
    if (strcmp(basename, ".") == 0) {
        d = &root;
        memset(d, 0, sizeof(ISODir));
        d->parent = &root;
        d->name_in_parent = "";
        d->records[0] = newdirrecord("\x00"); // .
        d->realrecord = d->records[0];
    } else { 
        d = (ISODir *) malloc(sizeof(ISODir));
        memset(d, 0, sizeof(ISODir));
        d->name_in_parent = strdup(basename);
        d->parent = find_isodir(dirname);
        // make sure parent dir gets an entry with the right name
        char uppername[256];
        strcpy(uppername, basename);
        strtoupper(uppername);
        DirectoryRecord *r = newdirrecord(uppername);
        d->parent->records[d->parent->nrecords++] = r;
        d->parent->subdirs[d->parent->nsubdirs++] = d; 

        assert(d->parent->nrecords < MAX_FILES);
        assert(d->parent->nsubdirs < MAX_FILES);

        d->realrecord = r;
        d->records[0] = newdirrecord("\x00"); // .
    }

    d->records[1] = newdirrecord("\x01"); // .. allocated now but overwritten during finalization
    d->nrecords = 2;
    d->nsubdirs = 0;

    VERBOSE("'%s' into '%s'[%d]", basename, dirname, (int)d->parent->nrecords-1);
 
    return d;
}

size_t dirsize(const ISODir *d)
{
    size_t dirsize = 0;
    size_t i;
    for (i=0; i < d->nrecords; ++i) {
        DirectoryRecord *f = d->records[i];
        dirsize += f->record_len;
    }
    return dirsize;
}

int openmapfile(const char *fn, void **outptr, struct stat *outstat)
{
    if (fn == NULL)
        return -1;

    STAT(fn, outstat);

    int fd = open(fn, O_RDONLY);
    if (fd < 0) {
        perror(fn);
        return fd;
    } 
 
    // allow private writes for the boot-info-table
    *outptr = mmap(NULL, outstat->st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);

    return fd;
}

int mkfile(const char *localfn, const char *isodirname, const char *isofn)
{
    ISODir *parent = find_isodir(isodirname);

    char *contents = NULL;

    struct stat st;
    int fd = openmapfile(localfn, (void **) &contents, &st);
    if (fd < 0) {
        return -1;    
    }

    const size_t filesize = st.st_size;

    // combine the crc and copy
    uint32_t zipcrc = crc32(contents, filesize);

    int ziphdr_sector = alloc_sectors(sectors(filesize) + 1);
    int data_sector = (filesize == 0) ? ziphdr_sector : (ziphdr_sector + 1);

    // reconstruct the full path/fn
    char fullfn[256] = { 0 };
    if (isodirname) {
        snprintf(fullfn, sizeof(fullfn), "%s/%s", isodirname, isofn);
    } else {
        strcpy(fullfn, isofn);
    }

    // check if this is the bootloader
    if (bootfn && strcmp(fullfn, bootfn) == 0) {
        bootloader_sector = data_sector;

        // fixup the boot image with the -boot-info-table

        uint32_t *bootinfotbl = (uint32_t *) &contents[8];
        bootinfotbl[0] = 16; // pvd_sector;
        bootinfotbl[1] = bootloader_sector;
        bootinfotbl[2] = filesize;
        bootinfotbl[3] = checksum32(&contents[64], filesize - 64);
        memset(&bootinfotbl[4], 0, 40);
    }

    if (filesize > 0) {
        FWRITEAT(fpizo, data_sector * SECTOR_SIZE, contents, filesize);
    }

    munmap((void *) contents, filesize);
    close(fd);

    size_t id_len = strlen(isofn) + 2; // include space for ';1'

    DirectoryRecord *r = (DirectoryRecord *) malloc(sizeof(DirectoryRecord) + id_len);

    int rlen=sizeof(DirectoryRecord) + id_len;
    memset(r, 0, rlen);

    // 6.8.1.1: "Each subsequent Directory Record recorded in that Logical
    // Sector shall begin at the byte immediately following the last byte
    // of the preceding Directory Record in that Logical Sector. Each
    // Directory Record shall end in the Logical Sector in which it
    // begins."
    int bytes_left_in_sector = SECTOR_SIZE - (dirsize(parent) % SECTOR_SIZE);
    if (sizeof(DirectoryRecord) + MAX_ID_LEN > bytes_left_in_sector) {
        rlen += bytes_left_in_sector;
    }

    r->record_len = rlen;

    SET32_LSBMSB(*r, data_sector, data_sector);
    SET32_LSBMSB(*r, data_len, filesize);

    // volume_seq_num/volume_set_size is always 1/1
    SET16_LSBMSB(*r, volume_seq_num, 1);

    izo_fill_dir_time(r, st.st_mtime);

    r->id_len = id_len;
    strcpy(r->id, isofn);
    strtoupper(r->id);
    r->id[id_len-2] = ';';
    r->id[id_len-1] = '1';

    parent->records[parent->nrecords++] = r;

    // construct ZIP local and central dir headers
 
    // id_len now includes the pathname
    id_len = strlen(fullfn);

    size_t chdrlen = sizeof(ZipCentralDirFileHeader) + id_len;
    ZipCentralDirFileHeader * chdr = (ZipCentralDirFileHeader *) malloc(chdrlen + 1);
    memset(chdr, 0, chdrlen);

    size_t lhdrlen = sizeof(ZipLocalFileHeader) + id_len;
    ZipLocalFileHeader * lhdr = (ZipLocalFileHeader *) alloca(lhdrlen);
    memset(lhdr, 0, lhdrlen);

    chdr->signature = 0x02014b50;
    lhdr->signature = 0x04034b50;

    chdr->version_needed = lhdr->version_needed = 10; // 1.0 is most basic level

    chdr->datetime = lhdr->datetime = MSDOSDateTime(st.st_mtime);

    chdr->crc32 = lhdr->crc32 = zipcrc;
    chdr->comp_size = chdr->uncomp_size = 
        lhdr->comp_size = lhdr->uncomp_size = filesize;
    chdr->filename_len = lhdr->filename_len = id_len;

    chdr->local_header_ofs = data_sector * SECTOR_SIZE - lhdrlen;
    strcpy(chdr->filename, fullfn);
    strcpy(lhdr->filename, fullfn);

    // write zip local header
    FWRITEAT(fpizo, chdr->local_header_ofs, lhdr, lhdrlen);

    // save off central dir header for later
    ziphdrs[nzipfiles++] = chdr;
    assert(nzipfiles < MAX_FILES);
    zip_cdir_len += chdrlen;

    return 0;
}

int ftw_mkfile_helper(const char *fpath, const struct stat *sb, int typeflag)
{
    char *parentdirname = NULL;
    char *nodename = NULL;

    size_t srcpathlen = strlen(sourcepath);
    if (fpath[srcpathlen-1] != '/') {
        srcpathlen += 1; // there will be a path separator anyway
    }

    // strip leading path, divide into dirname and basename
    parsepath(&fpath[srcpathlen], &parentdirname, &nodename);

    VERBOSE("%s: %s %s", fpath, parentdirname, nodename);

    if (typeflag == FTW_F) {
        mkfile(fpath, parentdirname, nodename);
    } else if (typeflag == FTW_D) {
        ISODir *d = mkisodir(parentdirname, nodename);
        struct stat st;
        STAT(fpath, &st);
        izo_fill_dir_time(d->realrecord, st.st_mtime);
    } else {
        perror("ftw_mkfile_helper");
        return -2;
    }

    free(parentdirname);
    free(nodename);
    return 0;
}

static int cmp_dirrecord(const void *_a, const void *_b)
{
    const DirectoryRecord *a = *(const DirectoryRecord **) _a;
    const DirectoryRecord *b = *(const DirectoryRecord **) _b;
    return memcmp(a->id, b->id, MIN(a->id_len, b->id_len));
}

size_t ptesize(size_t id_len)
{
    size_t ptelen = sizeof(PathTableEntry) + id_len;
    if (id_len % 2 == 1) {
        ptelen += 1; // padding
    }
    return ptelen;
}

int finalize_dir(ISODir *d)
{
    size_t dirlen = dirsize(d);

    int sectors_reqd_for_dir = sectors(dirlen);
    int dirsector = alloc_sectors(sectors_reqd_for_dir);

    // finalize the real record in the parent
    SET32_LSBMSB(*d->realrecord, data_sector, dirsector);
    SET32_LSBMSB(*d->realrecord, data_len, dirlen);

    size_t ptelen = ptesize(d->realrecord->id_len);

    PathTableEntry *pte = (PathTableEntry *) malloc(ptelen);
    memset(pte, 0, ptelen);
    d->pte_num = npaths++;
    pte->id_len = d->realrecord->id_len;
    pte->ear_length = d->realrecord->ear_sectors;
    pte->dir_sector = d->realrecord->data_sector;
    pte->parent_dir_num = d->parent->pte_num;
    memcpy(pte->id, d->name_in_parent, pte->id_len);
    // XXX: uppercase pte->id

    LOG("PTE[%u] @%u: %s (parent [%u])", d->pte_num, pte->dir_sector, pte->id, pte->parent_dir_num);
    paths[d->pte_num] = pte;
    pathtablesize += ptelen;

    // copy that record as our 'self'
    d->records[0] = newdirrecord(".");
    memcpy(d->records[0], d->realrecord, sizeof(DirectoryRecord));
    d->records[0]->id_len = 1;
    d->records[0]->id[0] = 0x00; // replace '.'

    // copy the parent (..) 'self' entry
    assert(d->parent->records[0]->data_sector > 0); // must be allocated already
    memcpy(d->records[1], d->parent->records[0], sizeof(DirectoryRecord));
    assert(d->records[1]->id_len == 1);
    assert(d->records[1]->id[0] == 0x01);

    // descend into child directories, allocate and write them first

    size_t i;

    for (i=0; i < d->nsubdirs; ++i)
    {
        finalize_dir(d->subdirs[i]);
    }

    qsort(&d->records[2], d->nrecords-2, sizeof(d->records[0]), cmp_dirrecord);

    // finally, write the directory itself
    FSEEK(fpizo, dirsector * SECTOR_SIZE, SEEK_SET);

    for (i=0; i < d->nrecords; ++i)
    {
        DirectoryRecord *f = d->records[i];
        FWRITE(fpizo, f, f->record_len);
    }

    return 0;
}

const char *get_env_with_default(const char *fieldname, const char *defval)
{
    const char *val = getenv(fieldname);
    if (val == NULL) {
        VERBOSE("%s not specified, using default '%s'", fieldname, defval);
        val = defval;
    }
    return strtoupper(strdup(val));
}

#define E(X) get_env_with_default(#X, #X)
#define EINT(X, D) atoi(get_env_with_default(#X, #D))
#define EDATE(X) get_env_with_default(#X, strdate(time(NULL)))

#define COPYENV(DEST, F, PAD) strncpypad(DEST, E(F), sizeof(DEST), PAD)

#define CATALOG_ENTRY_SIZE 0x20
void el_torito(u8 *boot_record, u8 *boot_catalog, int boot_catalog_sector)
{
    ElToritoBootRecord *etbr = (ElToritoBootRecord *) boot_record;
    // Boot Record
    static char base_ETBR[SECTOR_SIZE] =
            "\x00" "CD001" "\x01" "EL TORITO SPECIFICATION";

    // not all are this type, but all entries are the same size, and this is
    // only for spacing
    ElToritoValidationEntry *entries = (ElToritoValidationEntry *) boot_catalog;

    assert(sizeof(*etbr) == SECTOR_SIZE);
    memcpy(etbr, base_ETBR, SECTOR_SIZE);

    etbr->boot_catalog_sector = boot_catalog_sector;

    // Boot Catalog
    ElToritoValidationEntry *etve = (ElToritoValidationEntry *) &entries[0];
    ElToritoDefaultEntry *etdefault = (ElToritoDefaultEntry *) &entries[1];
    ElToritoSectionHeaderEntry *etshe = (ElToritoSectionHeaderEntry *) &entries[2];
    assert(sizeof(*etve) == CATALOG_ENTRY_SIZE);
    assert(sizeof(*etdefault) == CATALOG_ENTRY_SIZE);
    assert(sizeof(*etshe) == CATALOG_ENTRY_SIZE);

    // Validation Entry
    memset(etve, 0, CATALOG_ENTRY_SIZE);
    etve->header_id = 0x01;
    COPYENV(etve->id, developer_id, 0);
    etve->key[0] = 0x55;
    etve->key[1] = 0xAA;
    etve->checksum = -checksum((const u8 *) etve, sizeof(*etve));
    assert(checksum((const u8 *) etve, sizeof(*etve)) == 0);

    // Initial/Default Entry
    memset(etdefault, 0, CATALOG_ENTRY_SIZE);
    etdefault->boot_indicator = 0x88;
    etdefault->load_segment = 0x07c0;
    // etdefault->system_type = from partition table??
    etdefault->sector_count = 4;
    etdefault->load_rba = bootloader_sector;

    static char base_ETSHE[CATALOG_ENTRY_SIZE] = "\x91\x00";

    memcpy(etshe, base_ETSHE, CATALOG_ENTRY_SIZE);
}


int
fill_pvd(PrimaryVolumeDescriptor *pvd)
{
    memset(pvd, 0, sizeof(*pvd));

    pvd->type = VDTYPE_PRIMARY;
    memcpy(pvd->id, "CD001", 5);
    pvd->version = 0x01;

    strncpypad(pvd->system_id, E(system_id), 32, ' ');
    strncpypad(pvd->volume_id, E(volume_id), 32, ' ');

    SET16_LSBMSB(*pvd, volume_set_size, EINT(volume_set_size, 1));
    SET16_LSBMSB(*pvd, volume_sequence_number, EINT(volume_sequence_number, 1));
    SET16_LSBMSB(*pvd, logical_block_size, EINT(logical_block_size, 2048));

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

    setdate(&pvd->creation_date, EDATE(creation_date));
    setdate(&pvd->modification_date, EDATE(modification_date));
    setdate(&pvd->expiration_date, EDATE(expiration_date));
    setdate(&pvd->effective_date, EDATE(effective_date));

    pvd->file_structure_version = 0x01;
}

void usage_and_exit(const char *binname) {
    fprintf(stderr, "Usage: %s [-v] [-f] [-c <commentfn>] [-b <bootsectorfn>] -o <output-izo-name> <path-to-walk>\n", 
            binname);
    exit(EXIT_FAILURE);
}

int construct_path_tables(void **lpath, void **mpath)
{
    char *lsb = malloc(pathtablesize);
    char *msb = malloc(pathtablesize);

    *lpath = lsb;
    *mpath = msb;

    size_t i;
    for (i=0; i < npaths; i++)
    {
        PathTableEntry *lsb_pte = (PathTableEntry *) lsb;
        PathTableEntry *msb_pte = (PathTableEntry *) msb;

        size_t ptelen = ptesize(paths[i]->id_len);
        memcpy(lsb_pte, paths[i], ptelen);

        // copy also into mpath table and make bigendian
        memcpy(msb_pte, paths[i], ptelen);
        msb_pte->dir_sector = htonl(lsb_pte->dir_sector);
        msb_pte->parent_dir_num = htons(lsb_pte->parent_dir_num);

        lsb += ptelen;
        msb += ptelen;
    }

    return pathtablesize;
}

int main(int argc, char **argv)
{
    assert(crc32("123456789", 9) == 0xcbf43926);
    assert(sizeof(ZipLocalFileHeader) == 30);
    assert(sizeof(ZipCentralDirFileHeader) == 46);
    assert(sizeof(ZipEndCentralDirRecord) == 22);

    char *outfn = NULL;
    char *commentfn = NULL;

    int opt;

    while ((opt = getopt(argc, argv, "fc:o:b:v")) != -1) {
        switch (opt) {
        case 'b':         // file containing bootloader
            bootfn = strdup(optarg);
            break;
        case 'c':         // zip comment field
            commentfn = strdup(optarg);
            break;
        case 'o':         // output file (required)
            outfn = strdup(optarg);
            break;
        case 'f':         // force (use unlink if file exists)
            force = 1;
            break;
        case 'v':
            verbose++;
            break;
        default:
            usage_and_exit(argv[0]);
            break;
        };
    }

    if (outfn == NULL || optind >= argc) {
        usage_and_exit(argv[0]);
    }

    sourcepath = argv[optind];

    struct stat st;
    if (stat(outfn, &st) < 0) {
        if (errno != ENOENT) {
            perror(outfn);
            exit(-1);
        }
    } else {
        // file exists, what should be done?
        if (S_ISBLK(st.st_mode)) {  // if block device, final size is known
            blockdevlen = st.st_size;
        } else {
            if (!force) {
                LOG("Output file '%s' already exists", outfn);
                exit(-1);
            }

            unlink(outfn);
        }
    }

    fpizo = fopen(outfn, "w+b");
    int pvd_sector = alloc_sectors(1);   // Primary Volume Descriptor
    int br_sector = alloc_sectors(1);   // Boot Record
    int vdst_sector = alloc_sectors(1); // Volume Descriptor Set Terminator

    assert(pvd_sector == 16);
    assert(br_sector == 17);

    int boot_catalog_sector = alloc_sectors(1);
    memset(&root, 0, sizeof(root));

    if (ftw(sourcepath, ftw_mkfile_helper, 16) < 0) {
        perror("ftw");
        exit(-1);
    }

    finalize_dir(&root);

    u8 boot_record[SECTOR_SIZE]; 
    u8 boot_catalog[SECTOR_SIZE];

    el_torito(boot_record, boot_catalog, boot_catalog_sector);
    FWRITEAT(fpizo, boot_catalog_sector * SECTOR_SIZE, boot_catalog, SECTOR_SIZE);
    FWRITEAT(fpizo, br_sector * SECTOR_SIZE, boot_record, SECTOR_SIZE);

    // construct the PVD
    PrimaryVolumeDescriptor pvd;
    fill_pvd(&pvd);

    memcpy(&pvd.root_directory_record, root.records[0], sizeof(DirectoryRecord));
    // construct and write both path tables

    void *lsb_path_table = NULL;
    void *msb_path_table = NULL;

    int pathtablelen = construct_path_tables(&lsb_path_table, &msb_path_table);

    SET32_LSBMSB(pvd, path_table_size, pathtablelen);
    pvd.lsb_path_table_sector = alloc_sectors(sectors(pvd.path_table_size));

    int msb_path_table_sector = alloc_sectors(sectors(pvd.path_table_size));
    pvd.msb_path_table_sector = htonl(msb_path_table_sector);

    FWRITEAT(fpizo, pvd.lsb_path_table_sector * SECTOR_SIZE, lsb_path_table, pvd.path_table_size);

    FWRITEAT(fpizo, msb_path_table_sector * SECTOR_SIZE, msb_path_table, pvd.path_table_size);

    // (LEAK: path tables)

    // write the PVD and VDST
    FWRITEAT(fpizo, pvd_sector * SECTOR_SIZE, &pvd, sizeof(pvd));
    FWRITEAT(fpizo, vdst_sector * SECTOR_SIZE, VolumeDescriptorSetTerminator, sizeof(VolumeDescriptorSetTerminator));

    // get ready to write the central dir (open the zip comment file)
    void *comment = NULL;
    size_t commentlen = 0;
    int commentfd = 0;
    if (commentfn) {
        struct stat st;
        commentfd = openmapfile(commentfn, &comment, &st);
        commentlen = st.st_size;
        zip_cdir_len += commentlen;
    }

    // determine final size of izo
    long endpos = blockdevlen;

    if (endpos < 0) { // not an existing block device
        FSEEK(fpizo, 0, SEEK_END);
        endpos = ftell(fpizo);

        // extend to a multiple of 1MB
        long bytes_left_this_mb = MB - (endpos % MB);

        if (bytes_left_this_mb < zip_cdir_len) {
            endpos += MB;
        }

        endpos += bytes_left_this_mb;
    }

    // write the comment
    if (commentfd > 0) {
        FWRITEAT(fpizo, endpos - commentlen, comment, commentlen);
        close(commentfd);
    }

    // write the .zip Central Directory
    FSEEK(fpizo, endpos - zip_cdir_len, SEEK_SET);

    size_t r;
    for (r=0; r < nzipfiles; ++r)
    {
        FWRITE(fpizo, ziphdrs[r], sizeof(ZipCentralDirFileHeader) + ziphdrs[r]->filename_len);
    }

    ZipEndCentralDirRecord endcdir;
    memset(&endcdir, 0, sizeof(endcdir));

    endcdir.signature = 0x06054b50;
    endcdir.central_dir_start = endpos - zip_cdir_len;
    endcdir.central_dir_bytes = zip_cdir_len - sizeof(endcdir) - commentlen;
    endcdir.disk_num_records = endcdir.total_num_records = nzipfiles;
    endcdir.comment_len = commentlen;

    FWRITE(fpizo, &endcdir, sizeof(endcdir));

    // and done
    fclose(fpizo);
    return 0;
}

