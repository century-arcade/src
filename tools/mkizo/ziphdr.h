#ifndef ZIPHDR_H_
#define ZIPHDR_H_

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define PACKED __attribute__ ((__packed__))

typedef struct PACKED {
    u32 signature;
    u16 version_needed;
    u16 bit_flags;
    u16 method;
    u32 datetime; // MSDOS Date Time format
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
    u32 datetime; // MSDOS Date Time format
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

uint32_t crc32(const uint8_t *buf, size_t bufLen);

#endif
