#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/mman.h>

#include "iso9660.h"

#include "zip_crc32.c"

#define SECTOR_SIZE 2048
#define PACKED __attribute__ ((__packed__))

#define LOG(FMT, args...) fprintf(stderr, FMT, ##args)

static const int MB = 1024 * 1024;

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct PACKED {
    u32 signature;
    u16 version_needed;
    u16 bit_flags;
    u16 method;
    u16 time;
    u16 date;
    u32 crc32;
    u32 comp_size;
    u32 uncomp_size;
    u16 filename_len;
    u16 extra_len;
    char filename[];
//    char m_extra[];
} ZipLocalFileHeader;

typedef struct PACKED {
    u32 signature;
    u16 version_made_by;
    u16 version_needed;
    u16 bit_flags;
    u16 method;
    u16 date;
    u16 time;
    u32 crc32;
    u32 comp_size;
    u32 uncomp_size;
    u16 filename_len;
    u16 extra_len;
    u16 comment_len;
    u16 disknum_start;
    u16 internal_attr;
    u32 external_attr;
    u32 local_header_ofs;
    char filename[];
//    char m_extra[];
//    char m_comment[];
} ZipCentralDirFileHeader;

typedef struct PACKED {
    u32 signature;
    u16 disk_num;
    u16 disk_start;
    u16 disk_num_records;
    u16 total_num_records;
    u32 central_dir_bytes;
    u32 central_dir_start;  // relative to start of archive
    u16 comment_len;
    u8 comment[];
} ZipEndCentralDirRecord;

#define SECTOR(N) (isoptr + (N) * SECTOR_SIZE)


static
const DirectoryRecord *find_file_at_sector_helper(void *isoptr,
                                                  int sector_num,
                                                  const DirectoryRecord *dir)
{
    const DirectoryRecord *entry = SECTOR(dir->data_sector);
    for (; entry->record_len != 0; entry = NEXT_DIR_ENTRY(entry))
    {
        if (entry->flags & ISO_DIRECTORY)
        {
            if (entry->id[0] == 0 || entry->id[0] == 1) { // self or parent
                continue;
            }

            const DirectoryRecord *r = find_file_at_sector_helper(isoptr, sector_num, entry);
            if (r != NULL) {
                return r;
            }
        }

        int start = entry->data_sector;
        int end = entry->data_sector + entry->data_len / SECTOR_SIZE;

        if (entry->data_len % SECTOR_SIZE == 0)
            end -= 1;

        if (sector_num >= start && sector_num <= end)
        {
            return entry;
        }
    }

    return NULL;
}

const DirectoryRecord *find_file_at_sector(void *isoptr, int sector_num)
{
    const PrimaryVolumeDescriptor *pvd = SECTOR(16);
    const DirectoryRecord *rootrecord =  &pvd->root_directory_record;
    return find_file_at_sector_helper(isoptr, sector_num, rootrecord);
}

// for local file header
struct added_sector
{
    u8 data[SECTOR_SIZE];
    int sector_lba; // inserted just before this LBA in the original ISO
};

void usage_and_exit(const char *binname) {
    fprintf(stderr, "Usage: %s [-o <output-izo-name>] [-i <input-iso>] <files-to-include-in-zip>...\n", 
            binname);
    exit(EXIT_FAILURE);
}

typedef struct ISOFile {
    const DirectoryRecord *entry;  // on ISO
    char fn[256]; // null-terminated (entry->id isn't)
    struct ISOFile *next; // simple linked list
} ISOFile;

ISOFile *get_files_in_dir(void *isoptr, int dirsector,
                          ISOFile *head, const char *basepath)
{
    DirectoryRecord *entry = SECTOR(dirsector);
    for ( ; entry->record_len > 0 ; entry = NEXT_DIR_ENTRY(entry))
    {
        if (entry->id[0] == 0x0) {
            // current directory
            continue;
        } else if (entry->id[0] == 0x01) {
            // parent directory
            continue;
        }

        char fqpn[256] = { 0 };
        strcpy(fqpn, basepath);
        strncat(fqpn, entry->id, entry->id_len);

        if (entry->flags & ISO_DIRECTORY) {
            // recurse into directories
            strcat(fqpn, "/");
            head = get_files_in_dir(isoptr, entry->data_sector, head, fqpn);
        } else {
            ISOFile *f = (ISOFile *) malloc(sizeof(ISOFile));

            // remove trailing ;1
            *strrchr(fqpn, ';') = 0;

            f->entry = entry;
            strcpy(f->fn, fqpn);
            f->next = head;
            head = f;
        }
    }
    return head;
}

static int
create_zip_hdrs(void *isoptr, ISOFile *f,
                ZipLocalFileHeader **out_lhdr, int *out_lhdr_len, 
                ZipCentralDirFileHeader **out_chdr, int *out_chdr_len)
{
    const DirectoryRecord *prev_file = find_file_at_sector(isoptr, f->entry->data_sector - 1);
    if (prev_file == NULL) {
        fprintf(stderr, "no previous file\n");
        return -1;
    }         

    int prev_filesize = prev_file->data_len;
    int leftover = SECTOR_SIZE - (prev_filesize % SECTOR_SIZE);

    if (leftover == SECTOR_SIZE) {
        leftover = 0; 
    }

    size_t fnlen = strlen(f->fn);
    size_t extralen = 0; // SECTOR_SIZE - (filesize % SECTOR_SIZE); // XXX: whole extra sector if mod == 0
    size_t lhdrlen = sizeof(ZipLocalFileHeader) + fnlen + extralen;
    ZipLocalFileHeader * lhdr = (ZipLocalFileHeader *) malloc(lhdrlen);

    lhdr->signature = 0x04034b50;
    lhdr->version_needed = 0x00;
    lhdr->bit_flags = 0x00;
    lhdr->method = 0x00;
    lhdr->time = 0x00;
    lhdr->date = 0x00;
    lhdr->crc32 = 0x00;
    lhdr->comp_size = lhdr->uncomp_size = f->entry->data_len;
    lhdr->filename_len = fnlen;
    lhdr->extra_len = extralen;
    strcpy(lhdr->filename, f->fn);

    size_t chdrlen = sizeof(ZipCentralDirFileHeader) + fnlen;
    ZipCentralDirFileHeader * chdr = (ZipCentralDirFileHeader *) malloc(chdrlen);
    chdr->signature = 0x02014b50;
    chdr->version_made_by = 0x00;
    chdr->version_needed = 0x00;
    chdr->bit_flags = 0x00;
    chdr->method = 0x00;
    chdr->date = 0x00;
    chdr->time = 0x00;
    chdr->crc32 = 0x00;
    chdr->comp_size = chdr->uncomp_size = f->entry->data_len;
    chdr->filename_len = fnlen;
    chdr->extra_len = extralen;
    chdr->comment_len = 0;
    chdr->disknum_start = 0;
    chdr->internal_attr = 0x00;
    chdr->external_attr = 0x00;
    chdr->local_header_ofs = 0;
    strcpy(chdr->filename, f->fn);
//    char m_extra[];
//    char m_comment[];

    uint32_t crc = crc32(SECTOR(f->entry->data_sector), f->entry->data_len);
    chdr->crc32 = lhdr->crc32 = crc;

    *out_lhdr = lhdr;
    *out_lhdr_len = lhdrlen;
    *out_chdr = chdr;
    *out_chdr_len = chdrlen;

    if (leftover >= lhdrlen) {
        chdr->local_header_ofs = 
                        f->entry->data_sector * SECTOR_SIZE - lhdrlen;
        return 0;
    }
    
    LOG("not enough leftover space in %s (size %d)\n", prev_file->id, prev_filesize);
    return -1;
}


int
main(int argc, char **argv)
{
    assert(crc32("123456789", 9) == 0xcbf43926);
    assert(sizeof(ZipLocalFileHeader) == 30);
    assert(sizeof(ZipCentralDirFileHeader) == 46);
    assert(sizeof(ZipEndCentralDirRecord) == 22);

    const char *infn = NULL;
    char outfn[256] = { 0 };

    int errors = 0;
    int fdiso, fdout;

    int opt;

    while ((opt = getopt(argc, argv, "i:o:")) != -1) {
        switch (opt) {
        case 'i': // input ISO
            infn = strdup(optarg);
            break;
        case 'o':
            strcpy(outfn, optarg);
            break;
        default:
            usage_and_exit(argv[0]);
        };
    }

    if (optind > argc) {
        usage_and_exit(argv[0]);
    }

    if (!outfn[0]) {
        strcpy(outfn, infn);
        char *ext = strrchr(outfn, '.');
        *ext = 0;
        strcat(ext, ".izo");
    }

    fprintf(stderr, "Converting ISO '%s' to IZO '%s'\n", infn, outfn);

    if ((fdiso = open(infn, O_RDONLY)) < 0) {
        perror(infn);
        exit(EXIT_FAILURE);
    }

    struct stat st;
    if (fstat(fdiso, &st) < 0) {
        perror("fstat iso");
        exit(EXIT_FAILURE);
    }

    unsigned long long isosize = st.st_size;

    void *isoptr = mmap(NULL, isosize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fdiso, 0);

    if (isoptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // get file info from ISO

    PrimaryVolumeDescriptor *pvd = SECTOR(16);
//    printf("# sectors=%d\n", pvd->num_sectors);
    const DirectoryRecord *rootrecord =  &pvd->root_directory_record;
    assert(rootrecord->record_len == sizeof(DirectoryRecord) + rootrecord->id_len);

    ISOFile *files = get_files_in_dir(isoptr, rootrecord->data_sector, NULL, "");

    ISOFile *f;
    size_t num_files=0;
    ZipCentralDirFileHeader *cdir_entries[256] = { NULL };
    for (f = files; f != NULL; f = f->next)
    {
        LOG("%s at ISO sector %d (%u bytes)\n",
            f->fn, f->entry->data_sector, f->entry->data_len);
    }

    for (f = files; f != NULL; f = f->next)
    {
        // if fn not in whitelist, skip
        size_t ifn;
        for (ifn=optind; ifn < argc; ++ifn) {
            if (strcasecmp(argv[ifn], f->fn) == 0) // TODO: globmatch
            {
                LOG("including %s\n", f->fn);
                ZipLocalFileHeader *local_hdr = NULL;
                int localhdr_len = -1;
                int centdirhdr_len = -1;
    
                if (create_zip_hdrs(isoptr, f,
                                &local_hdr, &localhdr_len,
                                &cdir_entries[num_files], &centdirhdr_len) < 0)
                {
                    errors++;
                    continue;
                }                        

                // copy local header into mmap'ed iso (non-writeback)
                void *iso_ziplhdr = SECTOR(f->entry->data_sector) - localhdr_len;
                memcpy(iso_ziplhdr, local_hdr, localhdr_len);

                num_files++;
            }
        }
    }

    if ((fdout = open(outfn, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {
        perror(outfn);
        exit(EXIT_FAILURE);
    }

    // write out sectors from original ISO
    int sector;
    for (sector=0; sector * SECTOR_SIZE < isosize; ++sector)
    {
        ssize_t r;

        r = write(fdout, isoptr + sector*SECTOR_SIZE, SECTOR_SIZE);
        assert (r == SECTOR_SIZE);
    }

    // put together the .zip central directory in memory

    char cdir_buf[32768];
    int cdir_len = 0;

    for (num_files=0; cdir_entries[num_files]; ++num_files)
    {
        size_t entry_len = sizeof(ZipCentralDirFileHeader) + cdir_entries[num_files]->filename_len;
       
        memcpy(&cdir_buf[cdir_len], cdir_entries[num_files], entry_len);
        cdir_len += entry_len;
    }

    ZipEndCentralDirRecord *endcdir = (ZipEndCentralDirRecord *) cdir_buf;
    endcdir->signature = 0x06054b50;
    endcdir->disk_num = 0;
    endcdir->disk_start = 0;
    endcdir->central_dir_bytes = cdir_len;
    endcdir->disk_num_records = endcdir->total_num_records = num_files;
    endcdir->comment_len = 8;   // for vanity hashing

    // make the IZO a multiple of 1MB
    
    cdir_len += sizeof(ZipEndCentralDirRecord) + 8;

    unsigned long long newisosize = sector * SECTOR_SIZE;
    newisosize += cdir_len + MB;
    newisosize &= ~(MB-1);

    endcdir->central_dir_start = newisosize - cdir_len;
    assert(newisosize > isosize + cdir_len);  // plus
    fprintf(stderr, "ISO size = %llu, new IZO size = %llu\n", isosize, newisosize);

    if (ftruncate(fdout, newisosize) < 0) {
        perror("ftruncate");
        ++errors;
    }

    // and write out the .zip Central Directory Header

    if (lseek(fdout, endcdir->central_dir_start, SEEK_SET) < 0) {
        perror("lseek");
        ++errors;
    };

    ssize_t r = write(fdout, cdir_buf, cdir_len);
    assert(r == cdir_len);

#if 0
    struct added_sector *add_sectors[256] = { NULL };
    int ninserts = 0;
    int num_files = 0;

    for (ISOFile *file = files; file->next != NULL; file = file->next)
    {
                
    }

    assert (isosize % SECTOR_SIZE == 0);

#ifdef ADD_SECTORS
    // for each inserted sector, bump the LBAs in directories and path tables.
    int i;
    for (i=0; add_sectors[i]; ++i)
    {
        // for each entry in the root directory,
        //     bump LBA extent +1 in both LSB and MSB if >= as above

        for (entry = SECTOR(rootrecord->data_sector);
                entry->record_len > 0; entry = NEXT_DIR_ENTRY(entry))
        {
            if (entry->data_sector >= add_sectors[i]->sector_lba)
            {
                entry->data_sector += 1;
                // XXX: also bump entry->msb_data_sector
            }
        }
        
        PathTableEntry *lsb_path_table = SECTOR(pvd->lsb_path_table_sector);
        int j;
        for (j=0; j < pvd->path_table_size; ++j) {
            if (lsb_path_table[j].dir_sector >= add_sectors[i]->sector_lba) {
                lsb_path_table[j].dir_sector += 1;
            }
            lsb_path_table = NEXT_PATH_TABLE_ENTRY(lsb_path_table);
        }
//     PathTableEntry *msb_path_table = SECTOR(ntohl(pvd->msb_path_table_sector));

    }

    pvd->num_sectors += ninserts;
    // XXX: also msb_num_sectors;
#endif // ADD_SECTORS

#ifdef ADD_SECTORS
        int i;
        for (i=0; add_sectors[i]; ++i) {
            if (add_sectors[i]->sector_lba == sector)
            {
                r = write(fdout, add_sectors[i]->data, SECTOR_SIZE);
                fprintf(stderr, "inserted sector at %d\n", sector);
                assert (r == SECTOR_SIZE);
                break;
            }
        }            
#endif

    }

#endif
    munmap(isoptr, isosize);
    close(fdiso);
    close(fdout);

    if (errors) {
        fprintf(stderr, "WARNING: %d non-fatal errors\n", errors);
    }

    exit(0);
}
