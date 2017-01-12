#ifndef FS_DESCRIPTORS_H
#define FS_DESCRIPTORS_H

#define FS_MAX_NAME 32
#define FS_ALLOC_UNITS 128
#define FS_DIRECTORY_FILES 16

#define FS_FREE 0x01
#define FS_UNUSED 0x02
#define FS_OCCUPIED 0x04
#define FS_SYSTEM 0x08

#define FS_ENDPOINT 0xFFFFFFFF

#define FS_INFO_OFFSET 0
#define FS_ALLOCATION_OFFSET sizeof(FS_info)
#define FS_DIRECTORY_OFFSET FS_ALLOCATION_OFFSET + sizeof(FS_allocation_table)
#define FS_DATA_OFFSET FS_DIRECTORY_OFFSET + sizeof(FS_directory_table)

typedef struct {
    uint8_t magic[3];   //GFS
    uint8_t version[5]; //x.x.x
    uint32_t size;      //size in bytes
    uint32_t free;      //free space
    uint32_t allocation_tables;
    uint32_t directory_tables;
} FS_info;

typedef struct {
    uint8_t type;
    uint32_t offset;
    uint32_t size;
    uint32_t next_block;
} FS_allocation_unit;

typedef struct {
    FS_allocation_unit units[FS_ALLOC_UNITS];
    uint32_t unused_units;
    uint32_t offset_next;
} FS_allocation_table;

typedef struct {
    uint8_t name[FS_MAX_NAME]; //truncated
    uint8_t flags;
    uint32_t size;
    uint32_t block;
    uint64_t created;
} FS_file_entry;

typedef struct {
    uint16_t files_flags;
    FS_file_entry files[16];
    uint32_t offset_next;
} FS_directory_table;

typedef struct {
    FS_info* info_block;
    FS_allocation_table* allocation_table;
    FS_directory_table* directory_table;
} FS_descriptors;

#endif //FS_DESCRIPTORS_H
