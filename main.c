#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "descriptors.h"
#include "version.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define FS_VERSION  STR(MAJOR_VERSION)"."STR(MINOR_VERSION)

#define ST_OK 0
#define ST_CANT_OPEN -1
#define ST_EXISTS -2
#define ST_NOT_VALID_FILE -3
#define ST_NOT_FOUND -4
#define ST_NOT_ENOUGH_SPACE -5

#define DIR_FROM_FILE 0x01
#define DIR_TO_FILE 0x02

int createFS(FILE* pDrive, uint32_t pBytes);

int addFile(FILE* pDrive, FILE* pFile, char* pFilename);

int getFile(FILE* pDrive, FILE* pDest, char* pFilename);

int removeFile(FILE* pDrive, char* pFile);

int status(FILE* pDrive);

int tree(FILE* pDrive);

void blockCopy(FILE* pDrive, FILE* pFile, FS_allocation_unit* pUnit, uint32_t pSize, uint8_t pDirection);

int loadDescriptors(FILE* pDrive, FS_descriptors* pDest);

int saveDescriptors(FILE* pDrive, FS_descriptors* pDest);

size_t fsize(FILE* pFile);

int discardDescriptors(FS_descriptors* pDest);

int findFile(FS_file_entry* pFile, uint32_t* pIndex, FS_descriptors* pDesc, char* pFilename) ;

int main(int argc, char** argv) {
    FILE* virtualDrive = NULL;
    int result = 0;

    if (argc < 2) {
        printf("Provide correct module: \n");
        printf("create, drop, add, remove, tree, status, version\n");
        printf("are allowed\n");
        return 1;
    }

    if (!strcmp(argv[1], "version")) {
        printf("FileSystem version "FS_VERSION"\n");
        printf("(c)2017 Wojciech Gruszka\n");
        return 0;
    }

    if (!strcmp(argv[1], "create")) {
        char* filename;
        int bytes;
        if (argc < 3) {
            printf("Provide correct arguments:\n");
            printf("FS create <size in bytes> <filename>\n");
            return 1;
        }
        filename = argv[3];
        bytes = atoi(argv[2]);
        if (bytes < 0) {
            printf("Size can not be negative!\n");
            return 1;
        }
        virtualDrive = fopen(filename, "wb");
        if (!virtualDrive) {
            printf("Could not open file!\n");
            return 1;
        }
        result = createFS(virtualDrive, (uint32_t) bytes);
    }

    if (!strcmp(argv[1], "status")) {
        char* filename;
        if (argc < 2) {
            printf("Provide correct arguments:\n");
            printf("FS status <filename>\n");
            return 1;
        }
        filename = argv[2];
        virtualDrive = fopen(filename, "rb");
        if (!virtualDrive) {
            printf("Could not open file!\n");
            return 1;
        }
        result = status(virtualDrive);
    }

    if (!strcmp(argv[1], "tree")) {
        char* filename;
        if (argc < 2) {
            printf("Provide correct arguments:\n");
            printf("FS tree <filename>\n");
            return 1;
        }
        filename = argv[2];
        virtualDrive = fopen(filename, "rb");
        if (!virtualDrive) {
            printf("Could not open file!\n");
            return 1;
        }
        result = tree(virtualDrive);
    }

    if (!strcmp(argv[1], "add")) {
        char* virtname;
        char* filename;
        FILE* file;
        if (argc < 3) {
            printf("Provide correct arguments:\n");
            printf("FS add <drive> <filename>\n");
            return 1;
        }
        filename = argv[3];
        virtname = argv[2];
        file = fopen(filename, "rb");
        virtualDrive = fopen(virtname, "rb+");

        if (!virtualDrive || !file) {
            printf("Could not open file!\n");
            return 1;
        }
        result = addFile(virtualDrive, file, filename);
    }

    if (!strcmp(argv[1], "get")) {
        char* virtname;
        char* destname;
        char* filename;
        FILE* dest;
        if (argc < 5) {
            printf("Provide correct arguments:\n");
            printf("FS get <drive> <filename> <destination>\n");
            return 1;
        }
        destname = argv[4];
        filename = argv[3];
        virtname = argv[2];

        dest = fopen(destname, "wb");
        virtualDrive = fopen(virtname, "rb+");

        if (!virtualDrive || !dest) {
            printf("Could not open file!\n");
            return 1;
        }
        result = getFile(virtualDrive, dest, filename);
    }

    if (!strcmp(argv[1], "remove")) {
        char* virtname;
        char* filename;
        if (argc < 4) {
            printf("Provide correct arguments:\n");
            printf("FS get <drive> <filename>\n");
            return 1;
        }
        filename = argv[3];
        virtname = argv[2];

        virtualDrive = fopen(virtname, "rb+");

        if (!virtualDrive) {
            printf("Could not open file!\n");
            return 1;
        }
        result = removeFile(virtualDrive, filename);
    }
    if (virtualDrive != NULL)
        fclose(virtualDrive);
    return result;
}

int createFS(FILE* pDrive, uint32_t pBytes) {
    FS_info header;
    FS_directory_table directoryTable;
    FS_allocation_table allocationTable;

    uint8_t dummy = 0;

    header.magic[0] = 'G';
    header.magic[1] = 'F';
    header.magic[2] = 'S';
    strncpy((char*) header.version, FS_VERSION, 5);
    header.size = pBytes;
    header.free = pBytes;
    header.allocation_tables = 1;
    header.directory_tables = 1;

    allocationTable.offset_next = FS_ENDPOINT;
    allocationTable.unused_units = FS_ALLOC_UNITS - 1;
    allocationTable.units[0].type = FS_FREE;
    allocationTable.units[0].offset = 0;
    allocationTable.units[0].size = pBytes;
    allocationTable.units[0].next_block = FS_ENDPOINT;
    for (uint32_t i = 1; i < FS_ALLOC_UNITS; ++i)
        allocationTable.units[i].type = FS_UNUSED;

    directoryTable.files_flags = 0;
    directoryTable.offset_next = FS_ENDPOINT;

    fwrite(&header, sizeof(header), 1, pDrive);
    fwrite(&allocationTable, sizeof(allocationTable), 1, pDrive);
    fwrite(&directoryTable, sizeof(directoryTable), 1, pDrive);

    for (uint32_t i = 0; i < pBytes; i++)
        fwrite(&dummy, sizeof(dummy), 1, pDrive);

    return ST_OK;
}

int addFile(FILE* pDrive, FILE* pFile, char* pFilename) {

    uint32_t size;
    FS_descriptors desc;
    FS_file_entry* file_entry = NULL;

    loadDescriptors(pDrive, &desc);
    size = (uint32_t) fsize(pFile);

    if (desc.info_block->free < size) {
        fclose(pFile);
        printf("Not enough space!\n");
        printf("Free: %d\n", desc.info_block->free);
        printf("Required: %d\n", size);
        return ST_NOT_ENOUGH_SPACE;
    }

    if (findFile(NULL, NULL, &desc, pFilename) == ST_OK) {
        fclose(pFile);
        printf("File with name: %s already exists!\n", pFilename);
        return ST_EXISTS;
    }

    desc.info_block->size -= size;
    //FIND EMPTY FILE RECORD
    for (uint32_t dir_block = 0; dir_block < desc.info_block->directory_tables; ++dir_block) {
        if (file_entry != NULL)
            break;
        if (~desc.directory_table[dir_block].files_flags == 0)
            continue;
        for (uint32_t dir_position = 0; dir_position < FS_DIRECTORY_FILES; ++dir_position)
            if (((~desc.directory_table[dir_block].files_flags) >> (dir_position)) & 1) {
                file_entry = &desc.directory_table[dir_block].files[dir_position];
                desc.directory_table[dir_block].files_flags |= (1 << dir_position);
                break;
            }
    }

    if (file_entry == NULL) {
        //TODO: CREATE NEW BLOCK
    }

    file_entry->size = size;
    strcpy((char*) file_entry->name, pFilename);
    file_entry->exists = 1;
    file_entry->created = (uint32_t) time(NULL);

    uint32_t last_block = FS_ENDPOINT;
    uint32_t last_unit = FS_ENDPOINT;

    for (uint32_t block = 0; block < desc.info_block->allocation_tables; ++block) {
        if (size == 0)
            break;
        for (uint32_t unit = 0; unit < FS_ALLOC_UNITS; ++unit) {
            if (size == 0)
                break;
            FS_allocation_unit* fsUnit = &desc.allocation_table[block].units[unit];

            if (fsUnit->type == FS_FREE) {
                if (last_block == FS_ENDPOINT)
                    file_entry->block = block * FS_ALLOC_UNITS + unit;
                else
                    desc.allocation_table[last_block].units[last_unit].next_block = block * FS_ALLOC_UNITS + unit;

                if (fsUnit->size > size) {
                    blockCopy(pDrive, pFile, fsUnit, size, DIR_FROM_FILE);

                    uint32_t unused_block = 0;
                    uint32_t unused_unit = 0;
                    FS_allocation_unit* _unit = NULL;
                    for (; unused_block < desc.info_block->allocation_tables; ++unused_block) {
                        if(_unit != NULL)
                            break;
                        for (unused_unit = 0; unused_unit < FS_ALLOC_UNITS; ++unused_unit) {
                            if (desc.allocation_table[unused_block].units[unused_unit].type == FS_UNUSED){
                                _unit = &desc.allocation_table[unused_block].units[unused_unit];
                                break;
                            }
                        }
                    }
                    _unit->type = FS_FREE;
                    _unit->size = fsUnit->size - size;
                    _unit->offset = fsUnit->offset + size;
                    _unit->next_block = FS_ENDPOINT;
                    desc.allocation_table[unused_block].unused_units -= 1;
                    fsUnit->type = FS_OCCUPIED;
                    fsUnit->size = size;
                    fsUnit->next_block = FS_ENDPOINT;
                    size = 0;
                    break;
                } else {
                    blockCopy(pDrive, pFile, fsUnit, fsUnit->size, DIR_FROM_FILE);
                    fsUnit->type = FS_OCCUPIED;
                    fsUnit->next_block = FS_ENDPOINT;
                    last_block = block;
                    last_unit = unit;
                    size -= fsUnit->size;
                }
            }
        }
    }

    saveDescriptors(pDrive, &desc);
    discardDescriptors(&desc);
    fclose(pFile);
    return ST_OK;
}

int getFile(FILE* pDrive, FILE* pDest, char* pFilename) {
    FS_info header;
    FS_directory_table table;
    FS_allocation_table alloc;
    FS_file_entry file;
    uint32_t block;

    fread(&header, sizeof(FS_info), 1, pDrive);
    fread(&alloc, sizeof(FS_allocation_table), 1, pDrive);
    fread(&table, sizeof(FS_directory_table), 1, pDrive);

    if (memcmp(header.magic, "GFS", 3)) {
        fclose(pDest);
        return ST_NOT_VALID_FILE;
    }

    if (findFile(&file, NULL, &table, pFilename)) {
        fclose(pDest);
        return ST_NOT_FOUND;
    }

    block = file.block;
    while (block != FS_ENDPOINT) {
        blockCopy(pDrive, pDest, &alloc.units[block], alloc.units[block].size, DIR_TO_FILE);
        block = alloc.units[block].next_block;
    }

    fclose(pDest);
    return ST_OK;
}

int removeFile(FILE* pDrive, char* pFile) {
    FS_file_entry file;
    uint32_t block;
    uint32_t file_idx;
    FS_descriptors desc;

    loadDescriptors(pDrive, &desc);


    if (findFile(&file, &file_idx, &desc, pFile))
        return ST_NOT_FOUND;

    block = file.block;
//    while (block != FS_ENDPOINT) {
//        alloc.units[block].type = FS_FREE;
//        uint32_t cnt = 0;
//        for (uint32_t i = 0; i < FS_ALLOC_UNITS && cnt != 2; ++i) {
//            //left block
//            if (alloc.units[block].offset == alloc.units[i].offset + alloc.units[i].size) {
//                if (alloc.units[i].type == FS_FREE) {
//                    alloc.units[block].type = FS_UNUSED;
//                    alloc.unused_units += 1;
//                    alloc.units[i].size += alloc.units[block].size;
//                    cnt += 1;
//                    block = i;
//                }
//                //right block
//            } else if (alloc.units[block].offset + alloc.units[block].size == alloc.units[i].offset) {
//                if (alloc.units[i].type == FS_FREE) {
//                    alloc.units[i].type = FS_UNUSED;
//                    alloc.unused_units += 1;
//                    alloc.units[block].size += alloc.units[i].size;
//                    cnt += 1;
//                }
//            }
//        }
//        block = alloc.units[block].next_block;
//    }
    desc.directory_table[file_idx/FS_DIRECTORY_FILES].files[file_idx%FS_DIRECTORY_FILES].exists = 0;
    desc.directory_table[file_idx/FS_DIRECTORY_FILES].files_flags &= ~(1 << file_idx%FS_DIRECTORY_FILES);
    desc.info_block->free += file.size;

    fseek(pDrive, 0, SEEK_SET);

    saveDescriptors(pDrive, &desc);
    discardDescriptors(&desc);

    return ST_OK;
}

int tree(FILE* pDrive) {
    FS_info header;
    FS_directory_table directoryTable;

    fread(&header, sizeof(FS_info), 1, pDrive);
    if (memcmp(header.magic, "GFS", 3)) {
        fclose(pDrive);
        return ST_NOT_VALID_FILE;
    }
    fseek(pDrive, sizeof(FS_allocation_table), SEEK_CUR);
    fread(&directoryTable, sizeof(FS_directory_table), 1, pDrive);
    printf("Files: \n");
    for (uint32_t i = 0; i < FS_DIRECTORY_FILES; ++i) {
        if ((directoryTable.files_flags >> i) & 1) {
            printf("%s \t %d bytes\n", directoryTable.files[i].name, directoryTable.files[i].size);
        }
    }
    return ST_OK;
}

int status(FILE* pDrive) {
    FS_info header;
    FS_directory_table table;
    FS_allocation_table alloc;
    uint8_t version[6];
    fread(&header, sizeof(FS_info), 1, pDrive);
    if (memcmp(header.magic, "GFS", 3)) {
        fclose(pDrive);
        return ST_NOT_VALID_FILE;
    }
    memcpy(version, header.version, 5);
    version[5] = 0;

    printf("FileSystem\n");
    printf("API Version: %s\n", FS_VERSION);
    fread(&alloc, sizeof(FS_allocation_table), 1, pDrive);
    fread(&table, sizeof(FS_directory_table), 1, pDrive);

    printf("\nINFO SECTION(OFF: 0x%02x)\n", (uint32_t) (FS_INFO_OFFSET));
    printf("VERSION: %s\nSIZE: %d\nFREE: %d\n", version, header.size, header.free);

    printf("\nALLOCATION SECTION(OFF: 0x%02x)\n", (uint32_t) (FS_ALLOCATION_OFFSET));
    printf("UNITS: %d\tUNUSED_UNITS: %d\tNEXT: %d\n", FS_ALLOC_UNITS, alloc.unused_units, alloc.offset_next);

    printf("\nDIRECTORY SECTION(OFF: 0x%02x)\n", (uint32_t) (FS_DIRECTORY_OFFSET));
    printf("FLAGS: 0x%02x\tNEXT: %d\n", table.files_flags, table.offset_next);
    for (uint32_t i = 0; i < FS_DIRECTORY_FILES; ++i) {
        if ((table.files_flags >> i) & 1)
            printf("ID: %d\tNAME: %-32s\tSIZE: %d\tBLOCK: %d\n", i, table.files[i].name, table.files[i].size,
                   table.files[i].block);
    }


    printf("\nDATA SECTION(OFF: 0x%02x):\n", (uint32_t) (FS_DATA_OFFSET));
    for (uint32_t i = 0; i < FS_ALLOC_UNITS; ++i) {
        if (alloc.units[i].type & FS_SYSTEM) {
            printf("SYS  BLOCK [%2d, %2d]\tOFFSET: 0x%02x\tSIZE: %d\n", 0, i, alloc.units[i].offset,
                   alloc.units[i].size);
        } else if (alloc.units[i].type & FS_FREE) {
            printf("FREE BLOCK [%2d, %2d]\tOFFSET: 0x%02x\tSIZE: %d\n", 0, i, alloc.units[i].offset,
                   alloc.units[i].size);
        } else if (alloc.units[i].type & FS_OCCUPIED) {
            printf("USED BLOCK [%2d, %2d]\tOFFSET: 0x%02x\tSIZE: %d\tNEXT: %d\n", 0, i, alloc.units[i].offset,
                   alloc.units[i].size, alloc.units[i].next_block);
        }
    }
    return ST_OK;
}

int findFile(FS_file_entry* pFile, uint32_t* pIndex, FS_descriptors* pDesc, char* pFilename) {
    for (uint32_t block = 0; block < pDesc->info_block->directory_tables; ++block) {
        for (uint32_t file = 0; file < FS_DIRECTORY_FILES; ++file) {
            FS_directory_table* dir = &pDesc->directory_table[block];
            if ((dir->files_flags >> file) & 1 && strcmp((const char*) dir->files[file].name, pFilename) == 0) {
                if (pFile != NULL)
                    *pFile = pDesc->directory_table[block].files[file];
                if (pIndex != NULL)
                    *pIndex = file;
                return ST_OK;
            }
        }
    }
    return ST_NOT_FOUND;
}

int loadDescriptors(FILE* pDrive, FS_descriptors* pDest) {

    pDest->info_block = malloc(sizeof(FS_info));
    fread(pDest->info_block, sizeof(FS_info), 1, pDrive);

    if (memcmp(pDest->info_block->magic, "GFS", 3)) {
        free(pDest->info_block);
        return ST_NOT_VALID_FILE;
    }

    pDest->allocation_table = malloc(pDest->info_block->allocation_tables * sizeof(FS_allocation_table));
    pDest->directory_table = malloc(pDest->info_block->directory_tables * sizeof(FS_directory_table));

    fread(&pDest->allocation_table[0], sizeof(FS_allocation_table), 1, pDrive);
    for (uint32_t i = 1; i < pDest->info_block->allocation_tables; ++i) {
        fseek(pDrive, pDest->allocation_table[i - 1].offset_next, SEEK_SET);
        fread(&pDest->allocation_table[i], sizeof(FS_allocation_table), 1, pDrive);
    }
    fseek(pDrive, FS_DIRECTORY_OFFSET, SEEK_SET);

    fread(&pDest->directory_table[0], sizeof(FS_directory_table), 1, pDrive);
    for (uint32_t i = 1; i < pDest->info_block->directory_tables; ++i) {
        fseek(pDrive, pDest->directory_table[i - 1].offset_next, SEEK_SET);
        fread(&pDest->directory_table[i], sizeof(FS_allocation_table), 1, pDrive);
    }
    return ST_OK;
}

int saveDescriptors(FILE* pDrive, FS_descriptors* pDest) {
    fseek(pDrive, 0, SEEK_SET);
    fwrite(pDest->info_block, sizeof(FS_info), 1, pDrive);

    fwrite(&pDest->allocation_table[0], sizeof(FS_allocation_table), 1, pDrive);

    for (uint32_t i = 1; i < pDest->info_block->allocation_tables; ++i) {
        fseek(pDrive, pDest->allocation_table[i - 1].offset_next, SEEK_SET);
        fwrite(&pDest->allocation_table[i], sizeof(FS_allocation_table), 1, pDrive);
    }
    fseek(pDrive, FS_DIRECTORY_OFFSET, SEEK_SET);

    fwrite(&pDest->directory_table[0], sizeof(FS_directory_table), 1, pDrive);
    for (uint32_t i = 1; i < pDest->info_block->directory_tables; ++i) {
        fseek(pDrive, pDest->directory_table[i - 1].offset_next, SEEK_SET);
        fwrite(&pDest->directory_table[i], sizeof(FS_allocation_table), 1, pDrive);
    }
    return ST_OK;
}

int discardDescriptors(FS_descriptors* pDest) {
    free(pDest->info_block);
    free(pDest->allocation_table);
    free(pDest->directory_table);
    return ST_OK;
}

void blockCopy(FILE* pDrive, FILE* pFile, FS_allocation_unit* pUnit, uint32_t pSize, uint8_t pDirection) {
    uint8_t buf[1024];
    fseek(pDrive, FS_DATA_OFFSET + pUnit->offset, SEEK_SET);
    while (pSize > 0) {
        uint32_t size;
        if (pSize > 1024) size = 1024;
        else size = pSize;
        if (pDirection == DIR_FROM_FILE) {
            fread(buf, sizeof(uint8_t), pSize, pFile);
            fwrite(buf, sizeof(uint8_t), pSize, pDrive);
        } else if (pDirection == DIR_TO_FILE) {
            fread(buf, sizeof(uint8_t), pSize, pDrive);
            fwrite(buf, sizeof(uint8_t), pSize, pFile);
        }
        pSize -= size;
    }
}

size_t fsize(FILE* pFile) {
    size_t size;
    fseek(pFile, 0, SEEK_END);
    size = (uint32_t) ftell(pFile);
    fseek(pFile, 0, SEEK_SET);
    return size;
}