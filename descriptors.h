#ifndef FS_DESCRIPTORS_H
#define FS_DESCRIPTORS_H

#define FS_MAX_NAME 32
#define FS_ALLOC_UNITS 128
#define FS_DIRECTORY_FILES 16

#define FS_FREE 0
#define FS_UNUSED 1
#define FS_OCCUPIED 2

#define FS_ENDPOINT 0xFFFFFFFF

#define FS_DATA_OFFSET sizeof(FS_info) + sizeof(FS_allocation_table) + sizeof(FS_directory_table)

typedef struct {
    uint8_t magic[3];   //GFS
    uint8_t version[5]; //x.x.x
    uint32_t size;      //size in bytes
    uint32_t free;      //free space
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
    uint8_t exists;
    uint32_t created;
    uint32_t last_modified;
    uint32_t size;
    uint32_t block;
} FS_file_entry;

typedef struct {
    uint16_t files_flags;
    FS_file_entry files[16];
    uint32_t offset_next;
} FS_directory_table;


#endif //FS_DESCRIPTORS_H
