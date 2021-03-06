#ifndef ISO9660_H_
#define ISO9660_H_

#include <stdint.h>

#define SECTOR_SIZE 2048
#define PACKED __attribute__ ((__packed__))

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef struct {
    char year[4];
    char month[2];
    char day[2];
    char hour[2];
    char minute[2];
    char second[2];
    char centisecond[2];   // 100ths of a second
    int8_t gmt_offset;     // 15-min intervals
} DecimalDateTime;

struct DirectoryRecord
{
    u8   record_len;
    u8   ear_sectors;
    u32  data_sector;
    u32  msb_data_sector;
    u32  data_len;
    u32  msb_data_len;
    u8   years_since_1900, 
         month, 
         day, 
         hour, 
         minute, 
         second, 
         gmt_offset;
    u8   flags;
    u8   interleaved_file_unit_size;
    u8   interleaved_gap_size;
    u16  volume_seq_num;
    u16  msb_volume_seq_num;
    u8   id_len;
    char id[];

} __attribute__ ((packed));

enum iso_flag_enum_s {
    ISO_HIDDEN          =   1,
    ISO_DIRECTORY       =   2,
    ISO_ASSOCIATED      =   4,
    ISO_RECORD          =   8,
    ISO_PROTECTION      =  16,
    ISO_DRESERVED1      =  32,
    ISO_DRESERVED2      =  64,
    ISO_MULTIEXTENT     = 128,
} iso_flag_enums;

typedef struct DirectoryRecord DirectoryRecord;

typedef struct PrimaryVolumeDescriptor {
	u8   type;
    char id[5];
    u8   version;
    u8   __unused1;
    char system_id[32];
    char volume_id[32];
    char __unused2[8];
    u32  num_sectors;          // volume_space_size
    u32  msb_num_sectors;
	char escape_sequences[32];
    u16  volume_set_size;
    u16  msb_volume_set_size;
	u16  volume_sequence_number;
	u16  msb_volume_sequence_number;
	u16  logical_block_size;
	u16  msb_logical_block_size;
	u32  path_table_size;
	u32  msb_path_table_size;

    // lsb_ are named explicitly because the lsb/msb are not the same value
    //    and have to be allocated/set separately
	u32  lsb_path_table_sector;
	u32  lsb_alt_path_table_sector;
	u32  msb_path_table_sector;
	u32  msb_alt_path_table_sector;

    DirectoryRecord root_directory_record;
    u8   root_id; // must be 0x00
    char volume_set_id[128];
    char publisher_id[128];
    char preparer_id[128];
	char application_id[128];
	char copyright_file_id[37];
	char abstract_file_id[37];
	char bibliographical_file_id[37];

    DecimalDateTime creation_date;
    DecimalDateTime modification_date;
    DecimalDateTime expiration_date;
    DecimalDateTime effective_date;

    u8 file_structure_version;
	u8 __unused4;
	u8 application_data[512];
	u8 __unused5;
} PrimaryVolumeDescriptor;   // must be at sector 16

typedef struct PACKED ElToritoBootRecord {
	u8   type;        // VDTYPE_BOOT = 0
    char id[5];       // "CD001"
    u8   version;     // 0x01
    char boot_system_id[32];  // "EL TORITO SPECIFICATION" padded with 0
    char boot_id[32]; // must be 0
    u32  boot_catalog_sector;
    char unused[1973];
} ElToritoBootRecord;  // must be at sector 17

typedef struct PACKED {
    u8   header_id;   // 0x01
    u8   platform_id; // 0x00 = 80x86
    u16  reserved;
    char id[24];      // manufacturer/developer of CD-ROM
    u16  checksum;    // sum of all words in the record should be 0
    u8   key[2];      // 0x55, 0xAA
} ElToritoValidationEntry;

typedef struct PACKED {
    u8   boot_indicator;  // 0x88 = Bootable
    u8   boot_media_type; // 0x00 = No Emulation
    u16  load_segment;    // 0x07c0 (also default)
    u8   system_type;     // byte 5 from Partition Table in boot image
    u8   __unused;
    u16  sector_count;    // number of virtual/emulated sectors loaded by bios
    u32  load_rba;        // start address of virtual disk
    u8   __unused2[20];
} ElToritoDefaultEntry;

typedef struct PACKED {
    u8 header_indicator;  // 0x91 = Final Header
    u8 platform_id;       // 0x00 = 80x86
    u16 num_entries;      // number of section entries following == 0
    u8 id[28];            // irrelevant if no section entries
} ElToritoSectionHeaderEntry;

#ifdef UNNEEDED // VDST always has the same contents
typedef struct {
    u8 type;    // 0xff
    char id[5]; // 'CD001'
    u8 version; // 0x01
} VolumeDescriptorSetTerminator;
#endif

const u8 VolumeDescriptorSetTerminator[] = "\xff" "CD001" "\x01";

enum  {
    VDTYPE_BOOT = 0,
    VDTYPE_PRIMARY = 1,
    VDTYPE_SUPP = 2,
    VDTYPE_PARTITION = 3,
    VDTYPE_END = 255,
};

typedef struct PathTableEntry {
    u8 id_len;
    u8 ear_length;
    u32 dir_sector;
    u16 parent_dir_num;
    char id[];
    // and then a padding byte if id_len is odd
} PathTableEntry;

#define NEXT_PATH_TABLE_ENTRY(E) \
    ((PathTableEntry *) (((u8 *) E) + sizeof(PathTableEntry) + E->id_len + (E->id_len & 0x1)))

#define NEXT_DIR_ENTRY(E) \
    ((DirectoryRecord *) (((u8 *) E) + E->record_len))

#endif
