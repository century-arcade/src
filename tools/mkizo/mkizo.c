#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include <libgen.h> // dirname, basename
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/mman.h> // mmap
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>    // O_RDONLY
#include <ftw.h>

#include "iso9660.h"
#include "ziphdr.h"

FILE *fpizo = NULL;
const char *sourcepath = NULL;
int verbose = 0;

typedef struct ISODir {
    const char *name_in_parent;    // just the basename
    struct ISODir *parent;
    DirectoryRecord *realrecord;   // same ptr as in parent.records[]

    DirectoryRecord *records[256]; // [0] is self, [1] is parent
    size_t nrecords;

    struct ISODir *subdirs[256];
    size_t nsubdirs;
} ISODir;

ISODir root;

#define LOG(FMTSTR, args...) fprintf(stderr, FMTSTR "\n", ## args)
#define VERBOSE(FMTSTR, args...) if (verbose) { LOG("%s: " FMTSTR, __FUNCTION__, ##args); }

size_t zip_cdir_len = sizeof(ZipEndCentralDirRecord);
size_t nzipfiles = 0;   // number of files in zip
ZipCentralDirFileHeader *ziphdrs[256];

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
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
//  BUG segfault:   localtime_r(&t, &tm);
    strftime(buf, sizeof(buf), "%Y%m%d%H%M%S00%z", &tm);
    return buf;
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

char *strtoupper(char *src)
{
    char *c;

    if (!src){
        return;
    }
    c = src;
    while((*c)!='\0'){
        (*c) = toupper(*c);
        c++;
    }

    return src;
}

// caller responsible to free() dirfn and leaffn
static void parsepath(const char *path, char **dirfn, char **leaffn)
{
    char *duppath = strdup(path);
    *dirfn = strdup(dirname(duppath));
    free(duppath);

    duppath = strdup(path);
    *leaffn = strdup(basename(duppath));
    free(duppath);
}

ISODir * find_isodir(const char *dirpath)
{
    if (strcmp(dirpath, ".") == 0) {
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
        (DirectoryRecord *) malloc(sizeof(DirectoryRecord) + id_len);

    // will also set id[0] to 0x0 if fn == NULL (as for root dir)
    memset(r, 0, sizeof(DirectoryRecord) + id_len);

    r->record_len = sizeof(DirectoryRecord) + id_len;

    // volume_seq_num/volume_set_size is always 1/1
    SET16_LSBMSB(*r, volume_seq_num, 1);
//    izo_fill_dir_time(r, time(NULL));

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

// fqpn in ISO/zip
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

int openmapfile(const char *fn, const void **outptr, size_t *outlen)
{
    if (fn == NULL)
        return -1;

    struct stat st;

    STAT(fn, &st);

    int fd = open(fn, O_RDONLY);
    if (fd < 0) {
        perror(fn);
        return fd;
    } 
  
    *outptr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    *outlen = st.st_size;

    return fd;
}

int mkfile(const char *localfn, const char *isodirname, const char *isofn)
{
    ISODir *parent = find_isodir(isodirname);

    size_t filesize = 0;
    const void *contents = NULL;

    int fd = openmapfile(localfn, &contents, &filesize);
    if (fd < 0) {
        return -1;    
    }

    // combine the crc and copy
    uint32_t zipcrc = crc32(contents, filesize);

    int sector = alloc_sectors(sectors(filesize));
    FWRITEAT(fpizo, (sector+1) * SECTOR_SIZE, contents, filesize);

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

    SET32_LSBMSB(*r, data_sector, sector);
    SET32_LSBMSB(*r, data_len, filesize);

    // volume_seq_num/volume_set_size is always 1/1
    SET16_LSBMSB(*r, volume_seq_num, 1);

//    izo_fill_dir_time(sb.entry, time(NULL));

    r->id_len = id_len;
    strcpy(r->id, isofn);
    strtoupper(r->id);
    r->id[id_len-2] = ';';
    r->id[id_len-1] = '1';

    parent->records[parent->nrecords++] = r;

    // construct ZIP local and central dir headers
    // reconstruct the full path/fn
    char fullfn[256] = { 0 };
    snprintf(fullfn, sizeof(fullfn), "%s/%s", isodirname, isofn);

    // id_len now includes the pathname
    id_len = strlen(fullfn);

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
    chdr->crc32 = lhdr->crc32 = zipcrc;
    chdr->comp_size = chdr->uncomp_size = 
        lhdr->comp_size = lhdr->uncomp_size = filesize;
    chdr->filename_len = lhdr->filename_len = id_len;

    chdr->local_header_ofs = (sector + 1) * SECTOR_SIZE - lhdrlen;
    strcpy(chdr->filename, fullfn);
    strcpy(lhdr->filename, fullfn);

    // write zip local header
    FWRITEAT(fpizo, chdr->local_header_ofs, lhdr, lhdrlen);

    // save off central dir header for later
    ziphdrs[nzipfiles++] = chdr;
    zip_cdir_len += chdrlen;

    return 0;
}

int ftw_mkfile_helper(const char *fpath, const struct stat *sb, int typeflag)
{
    char *parentdirname = NULL;
    char *nodename = NULL;

    // strip leading path, divide into dirname and basename
    parsepath(&fpath[strlen(sourcepath)+1], &parentdirname, &nodename);

    VERBOSE("%s: %s %s", fpath, parentdirname, nodename);

    if (typeflag == FTW_F) {
        mkfile(fpath, parentdirname, nodename);
    } else if (typeflag == FTW_D) {
        mkisodir(parentdirname, nodename);
    } else {
        perror("ftw_mkfile_helper");
        return -2;
    }

    free(parentdirname);
    free(nodename);
    return 0;
}

int finalize_dir(ISODir *d)
{
    size_t dirlen = dirsize(d);

    int sectors_reqd_for_dir = sectors(dirlen);
    int dirsector = alloc_sectors(sectors_reqd_for_dir);

    // finalize the real record in the parent
    SET32_LSBMSB(*d->realrecord, data_sector, dirsector);
    SET32_LSBMSB(*d->realrecord, data_len, dirlen);

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
    fprintf(stderr, "Usage: %s [-v] [-c <comment>] -o <output-izo-name> <path-to-walk>\n", 
            binname);
    exit(EXIT_FAILURE);
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

    while ((opt = getopt(argc, argv, "c:o:v")) != -1) {
        switch (opt) {
        case 'o':
            outfn = strdup(optarg);
            break;
        case 'v':
            verbose++;
            break;
        case 'c': 
            commentfn = strdup(optarg);
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

    unlink(outfn);

    fpizo = fopen(outfn, "w+b");
    int pvd_sector = alloc_sectors(1);   // Primary Volume Descriptor
    assert(pvd_sector == 16);
//    int br_sector = alloc_sectors(1);   // Boot Record
    int vdst_sector = alloc_sectors(1); // Volume Descriptor Set Terminator

    memset(&root, 0, sizeof(root));

    if (ftw(sourcepath, ftw_mkfile_helper, 16) < 0) {
        perror("ftw");
        exit(-1);
    }

    finalize_dir(&root);

    // write the PVD
    PrimaryVolumeDescriptor pvd;
    fill_pvd(&pvd);
    memcpy(&pvd.root_directory_record, root.records[0], sizeof(DirectoryRecord));

    FWRITEAT(fpizo, pvd_sector * SECTOR_SIZE, &pvd, sizeof(pvd));

    FWRITEAT(fpizo, vdst_sector * SECTOR_SIZE, VolumeDescriptorSetTerminator, sizeof(VolumeDescriptorSetTerminator));

    FSEEK(fpizo, 0, SEEK_END);
    long endpos = ftell(fpizo);
    long bytes_left_this_mb = MB - (endpos % MB);

    const void *comment = NULL;
    size_t commentlen = 0;
    int commentfd = 0;
    if (commentfn) {
        commentfd = openmapfile(commentfn, &comment, &commentlen);
        zip_cdir_len += commentlen;
    }

    if (bytes_left_this_mb < zip_cdir_len) {
        endpos += MB;
    }

    endpos += bytes_left_this_mb;

    if (commentfd > 0) {
        FWRITEAT(fpizo, endpos - commentlen, comment, commentlen);
        close(commentfd);
    }

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

    fclose(fpizo);
    return 0;
}

