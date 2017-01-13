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

int removeFile(FILE* pDrive, char* pFile);

int status(FILE* pDrive);

int tree(FILE* pDrive);

int findFile(FS_file_entry* pFile, uint32_t* pIndex, FS_descriptors* pDesc, char* pFilename);

void blockCopy(FILE* pDrive, FILE* pFile, FS_allocation_unit* pUnit, uint32_t pSize, uint8_t pDirection);

int loadDescriptors(FILE* pDrive, FS_descriptors* pDest);

int saveDescriptors(FILE* pDrive, FS_descriptors* pDest);

int discardDescriptors(FS_descriptors* pDest);

size_t fsize(FILE* pFile);

int getFile(FILE* pDrive, char* pDest, char* pFilename);

int addFile(FILE* pDrive, char* pFilename);

int createDirectoryBlock(FS_descriptors* pDesc);

uint32_t findBlock(FS_descriptors* pDesc, uint8_t pType);

uint32_t findBlockSize(FS_descriptors* pDesc, uint8_t pType, uint32_t pSize);

uint32_t allocateSystemBlock(FS_descriptors* pDesc, uint32_t pSize);

int createAllocationBlock(FS_descriptors* pDesc);

int main(int argc, char** argv) {
    FILE* virtualDrive = NULL;
    int result = 0;

    if (argc < 2 || !strcmp(argv[1], "help")) {
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
        int bytes;
        if (argc < 4) {
            printf("Provide correct arguments:\n");
            printf("FS create <drive> <size in bytes>\n");
            return 1;
        }
        virtualDrive = fopen(argv[2], "wb+");
        if (virtualDrive == NULL) {
            printf("Could not create file :ccc\n");
            return ST_CANT_OPEN;
        }
        bytes = atoi(argv[3]);
        if (bytes < 0) {
            printf("Size can not be negative!\n");
            return 1;
        }
        result = createFS(virtualDrive, (uint32_t) bytes);
    }

    virtualDrive = fopen(argv[2], "rb+");
    if (virtualDrive == NULL) {
        printf("Could not open file :ccc\n");
        return ST_CANT_OPEN;
    }

    if (!strcmp(argv[1], "status")) {
        if (argc < 2) {
            printf("Provide correct arguments:\n");
            printf("FS status <filename>\n");
            return 1;
        }
        result = status(virtualDrive);
    }

    if (!strcmp(argv[1], "tree")) {
        if (argc < 2) {
            printf("Provide correct arguments:\n");
            printf("FS tree <filename>\n");
            return 1;
        }
        result = tree(virtualDrive);
    }

    if (!strcmp(argv[1], "add")) {
        char* filename;
        if (argc < 3) {
            printf("Provide correct arguments:\n");
            printf("FS add <drive> <filename>\n");
            return 1;
        }
        filename = argv[3];
        result = addFile(virtualDrive, filename);
    }

    if (!strcmp(argv[1], "get")) {
        char* destname;
        char* filename;
        if (argc < 5) {
            printf("Provide correct arguments:\n");
            printf("FS get <drive> <filename> <destination>\n");
            return 1;
        }
        destname = argv[4];
        filename = argv[3];

        result = getFile(virtualDrive, destname, filename);
    }

    if (!strcmp(argv[1], "remove")) {
        char* filename;
        if (argc < 4) {
            printf("Provide correct arguments:\n");
            printf("FS get <drive> <filename>\n");
            return 1;
        }
        filename = argv[3];
        result = removeFile(virtualDrive, filename);
    }

    fclose(virtualDrive);
    printf("STATUS: %d\n", result);
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

int addFile(FILE* pDrive, char* pFilename) {
    FILE* file;
    uint32_t size;
    FS_descriptors desc;
    FS_file_entry* file_entry = NULL;

    file = fopen(pFilename, "rb");
    if (file == NULL)
        return ST_CANT_OPEN;

    loadDescriptors(pDrive, &desc);
    size = (uint32_t) fsize(file);

    if (desc.info_block->free < size) {
        fclose(file);
        printf("Not enough space!\n");
        printf("Free: %d\n", desc.info_block->free);
        printf("Required: %d\n", size);
        return ST_NOT_ENOUGH_SPACE;
    }

    if (findFile(NULL, NULL, &desc, pFilename) == ST_OK) {
        fclose(file);
        printf("File with name: %s already exists!\n", pFilename);
        return ST_EXISTS;
    }
    desc.info_block->free -= size;

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
        if (createDirectoryBlock(&desc) != ST_OK)
            return ST_NOT_ENOUGH_SPACE;
        FS_directory_table* dir = &desc.directory_table[desc.info_block->directory_tables - 1];
        file_entry = &dir->files[0];
        dir->files_flags |= 1;
    }

    file_entry->size = size;
    file_entry->created = (uint64_t) time(NULL);
    strcpy((char*) file_entry->name, pFilename);

    uint32_t freeBlock;
    FS_allocation_unit* fsUnit;
    FS_allocation_unit* lastUnit = NULL;

    while (size != 0) {
        freeBlock = findBlock(&desc, FS_FREE);
        if (freeBlock == FS_ENDPOINT)
            return ST_NOT_ENOUGH_SPACE;

        fsUnit = &desc.allocation_table[freeBlock / FS_ALLOC_UNITS].units[freeBlock % FS_ALLOC_UNITS];

        if (lastUnit != NULL)
            lastUnit->next_block = freeBlock;
        else
            file_entry->block = freeBlock;

        if (fsUnit->size > size) {
            blockCopy(pDrive, file, fsUnit, size, DIR_FROM_FILE);
            uint32_t unusedBlock = findBlock(&desc, FS_UNUSED);
            if (unusedBlock == FS_ENDPOINT) {
                if (createAllocationBlock(&desc) != ST_OK)
                    return ST_NOT_ENOUGH_SPACE;
            unusedBlock = FS_ALLOC_UNITS * desc.info_block->allocation_tables;
            }

            FS_allocation_unit* unusedUnit = &desc.allocation_table[unusedBlock / FS_ALLOC_UNITS].units[unusedBlock %
                                                                                                        FS_ALLOC_UNITS];
            unusedUnit->type = FS_FREE;
            unusedUnit->size = fsUnit->size - size;
            unusedUnit->offset = fsUnit->offset + size;
            unusedUnit->next_block = FS_ENDPOINT;
            desc.allocation_table[unusedBlock / FS_ALLOC_UNITS].unused_units -= 1;
            fsUnit->size = size;
        } else {
            blockCopy(pDrive, file, fsUnit, fsUnit->size, DIR_FROM_FILE);
            fsUnit->type = FS_OCCUPIED;
            fsUnit->next_block = FS_ENDPOINT;
        }
        fsUnit->type = FS_OCCUPIED;
        fsUnit->next_block = FS_ENDPOINT;
        size -= fsUnit->size;
        lastUnit = fsUnit;
    }

    saveDescriptors(pDrive, &desc);
    discardDescriptors(&desc);
    fclose(file);
    return ST_OK;
}

int getFile(FILE* pDrive, char* pDest, char* pFilename) {
    FS_descriptors desc;
    FS_file_entry file;
    FILE* dest;
    uint32_t block;

    if (loadDescriptors(pDrive, &desc) != ST_OK) {
        return ST_NOT_VALID_FILE;
    };


    if (findFile(&file, NULL, &desc, pFilename)) {
        return ST_NOT_FOUND;
    }

    dest = fopen(pDest, "wb+");
    block = file.block;
    while (block != FS_ENDPOINT) {
        FS_allocation_unit* unit = &desc.allocation_table[block / FS_ALLOC_UNITS].units[block % FS_ALLOC_UNITS];
        blockCopy(pDrive, dest, unit, unit->size, DIR_TO_FILE);
        block = unit->next_block;
    }

    fclose(dest);
    return ST_OK;
}

int removeFile(FILE* pDrive, char* pFile) {
    FS_file_entry file;
    uint32_t file_idx;
    FS_descriptors desc;

    if (loadDescriptors(pDrive, &desc) != ST_OK) {
        return ST_NOT_VALID_FILE;
    };

    if (findFile(&file, &file_idx, &desc, pFile))
        return ST_NOT_FOUND;

    uint32_t block = file.block;
    while (block != FS_ENDPOINT) {
        desc.allocation_table[block / FS_ALLOC_UNITS].units[block % FS_ALLOC_UNITS].type = FS_FREE;
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
//        }     //TODO: DEFRAG
        block = desc.allocation_table[block / FS_ALLOC_UNITS].units[block % FS_ALLOC_UNITS].next_block;
    }

    desc.directory_table[file_idx / FS_DIRECTORY_FILES].files_flags &= ~(1 << file_idx % FS_DIRECTORY_FILES);
    desc.info_block->free += file.size;

    saveDescriptors(pDrive, &desc);
    discardDescriptors(&desc);

    return ST_OK;
}

int tree(FILE* pDrive) {
    FS_descriptors desc;
    char time[20];

    if (loadDescriptors(pDrive, &desc) != ST_OK) {
        return ST_NOT_VALID_FILE;
    };

    printf("Files: \n");
    for (uint32_t block = 0; block < desc.info_block->directory_tables; ++block) {
        FS_directory_table* directory = &desc.directory_table[block];
        for (uint32_t file = 0; file < FS_DIRECTORY_FILES; ++file) {
            if ((directory->files_flags >> file) & 1) {
                strftime(time, 20, "%H:%M:%S %d-%m-%Y", localtime((const time_t*) &directory->files[file].created));
                printf("%s\t%d bytes\t%s\n", directory->files[file].name, directory->files[file].size, time);
            }
        }
    }
    return ST_OK;
}

int status(FILE* pDrive) {
    FS_descriptors desc;
    uint8_t version[6];

    if (loadDescriptors(pDrive, &desc) != ST_OK) {
        return ST_NOT_VALID_FILE;
    }

    memcpy(version, desc.info_block->version, 5);
    version[5] = 0;

    printf("GFS File System\n");
    printf("API Version: %s\n", FS_VERSION);

    printf("\nINFO SECTION(OFF: 0x%02x)\n", (uint32_t) (FS_INFO_OFFSET));
    printf("VERSION: %s\nSIZE: %d\nFREE: %d\nALLOCATION TABLES: %d\nDIRECTORY TABLES: %d\n", version,
           desc.info_block->size, desc.info_block->free, desc.info_block->allocation_tables,
           desc.info_block->directory_tables);

    for (uint32_t i = 0; i < desc.info_block->allocation_tables; ++i) {
        printf("\nALLOCATION SECTION(OFF: 0x%02x)\n", (uint32_t) (FS_ALLOCATION_OFFSET));
        printf("UNITS: %d\tUNUSED_UNITS: %d\tNEXT: %d\n", FS_ALLOC_UNITS, desc.allocation_table[i].unused_units,
               desc.allocation_table[i].offset_next);
    }

    for (uint32_t i = 0; i < desc.info_block->directory_tables; ++i) {
        printf("\nDIRECTORY SECTION %d(OFF: 0x%02x)\n", i, (uint32_t) (FS_DIRECTORY_OFFSET));
        printf("FLAGS: 0x%04x\tNEXT: %d\n", desc.directory_table[i].files_flags, desc.directory_table[i].offset_next);

        printf("%-4s %-20s %-5s %-5s\n", "ID", "NAME", "SIZE", "BLOCK");

        for (uint32_t file = 0; file < FS_DIRECTORY_FILES; ++file) {
            if ((desc.directory_table[i].files_flags >> file) & 1)
                printf("%-4d %-20s %-5d %-5d\n", file, desc.directory_table[i].files[file].name,
                       desc.directory_table[i].files[file].size,
                       desc.directory_table[i].files[file].block);
        }
    }
    for (uint32_t i = 0; i < desc.info_block->allocation_tables; ++i) {
        printf("\nDATA SECTION(OFF: 0x%02x):\n", (uint32_t) (FS_DATA_OFFSET));
        printf("%-6s %-16s %-10s %-6s %-6s\n", "TYPE", "BLOCK", "OFFSET", "SIZE", "NEXT");
        for (uint32_t unit = 0; unit < FS_ALLOC_UNITS; ++unit) {
            if (desc.allocation_table[i].units[unit].type & FS_SYSTEM) {
                printf("%-6s %-3d[%3d, %3d]    0x%04x   %6d\n", "SYS", FS_ALLOC_UNITS * i + unit, i, unit,
                       desc.allocation_table[i].units[unit].offset,
                       desc.allocation_table[i].units[unit].size);

            } else if (desc.allocation_table[i].units[unit].type & FS_FREE) {
                printf("%-6s %-3d[%3d, %3d]    0x%04x   %6d\n", "FREE", FS_ALLOC_UNITS * i + unit, i, unit,
                       desc.allocation_table[i].units[unit].offset,
                       desc.allocation_table[i].units[unit].size);

            } else if (desc.allocation_table[i].units[unit].type & FS_OCCUPIED) {
                if (desc.allocation_table[i].units[unit].next_block != FS_ENDPOINT)
                    printf("%-6s %-3d[%3d, %3d]    0x%04x   %6d %6d\n", "DATA", FS_ALLOC_UNITS * i + unit, i, unit,
                           desc.allocation_table[i].units[unit].offset,
                           desc.allocation_table[i].units[unit].size,
                           desc.allocation_table[i].units[unit].next_block);
                else
                    printf("%-6s %-3d[%3d, %3d]    0x%04x   %6d\n", "DATA", FS_ALLOC_UNITS * i + unit, i, unit,
                           desc.allocation_table[i].units[unit].offset,
                           desc.allocation_table[i].units[unit].size);
            }
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

    pDest->allocation_table = malloc((1 + pDest->info_block->allocation_tables) * sizeof(FS_allocation_table));
    pDest->directory_table = malloc((1 + pDest->info_block->directory_tables) * sizeof(FS_directory_table));

    fread(&pDest->allocation_table[0], sizeof(FS_allocation_table), 1, pDrive);
    for (uint32_t i = 1; i < pDest->info_block->allocation_tables; ++i) {
        uint32_t block = pDest->allocation_table[i - 1].offset_next;
        uint32_t offset = pDest->allocation_table[block / FS_ALLOC_UNITS].units[block % FS_ALLOC_UNITS].offset;
        fseek(pDrive, FS_DATA_OFFSET + offset, SEEK_SET);
        fread(&pDest->allocation_table[i], sizeof(FS_allocation_table), 1, pDrive);
    }
    fseek(pDrive, FS_DIRECTORY_OFFSET, SEEK_SET);

    fread(&pDest->directory_table[0], sizeof(FS_directory_table), 1, pDrive);
    for (uint32_t i = 1; i < pDest->info_block->directory_tables; ++i) {
        uint32_t block = pDest->directory_table[i - 1].offset_next;
        uint32_t offset = pDest->allocation_table[block / FS_ALLOC_UNITS].units[block % FS_ALLOC_UNITS].offset;
        fseek(pDrive, FS_DATA_OFFSET + offset, SEEK_SET);
        fread(&pDest->directory_table[i], sizeof(FS_directory_table), 1, pDrive);
    }
    return ST_OK;
}

int saveDescriptors(FILE* pDrive, FS_descriptors* pDest) {
    fseek(pDrive, 0, SEEK_SET);
    fwrite(pDest->info_block, sizeof(FS_info), 1, pDrive);

    fwrite(&pDest->allocation_table[0], sizeof(FS_allocation_table), 1, pDrive);
    for (uint32_t i = 1; i < pDest->info_block->allocation_tables; ++i) {
        uint32_t block = pDest->allocation_table[i - 1].offset_next;
        uint32_t offset = pDest->allocation_table[block / FS_ALLOC_UNITS].units[block % FS_ALLOC_UNITS].offset;
        fseek(pDrive, FS_DATA_OFFSET + offset, SEEK_SET);
        fwrite(&pDest->allocation_table[i], sizeof(FS_allocation_table), 1, pDrive);
    }

    fseek(pDrive, FS_DIRECTORY_OFFSET, SEEK_SET);
    fwrite(&pDest->directory_table[0], sizeof(FS_directory_table), 1, pDrive);
    for (uint32_t i = 1; i < pDest->info_block->directory_tables; ++i) {
        uint32_t block = pDest->directory_table[i - 1].offset_next;
        uint32_t offset = pDest->allocation_table[block / FS_ALLOC_UNITS].units[block % FS_ALLOC_UNITS].offset;
        fseek(pDrive, FS_DATA_OFFSET + offset, SEEK_SET);
        fwrite(&pDest->directory_table[i], sizeof(FS_directory_table), 1, pDrive);
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
            fread(buf, sizeof(uint8_t), size, pFile);
            fwrite(buf, sizeof(uint8_t), size, pDrive);
        } else if (pDirection == DIR_TO_FILE) {
            fread(buf, sizeof(uint8_t), size, pDrive);
            fwrite(buf, sizeof(uint8_t), size, pFile);
        }
        pSize -= size;
    }
}

int createAllocationBlock(FS_descriptors* pDesc) {
    uint32_t freeBlock = findBlockSize(pDesc, FS_FREE, sizeof(FS_allocation_table));
    if(freeBlock == FS_ENDPOINT)
        return ST_NOT_ENOUGH_SPACE;

    memset(&pDesc->allocation_table[pDesc->info_block->allocation_tables], FS_UNUSED, sizeof(FS_allocation_table));

    FS_allocation_unit* freeUnit = &pDesc->allocation_table[freeBlock/FS_ALLOC_UNITS].units[freeBlock%FS_ALLOC_UNITS];
    FS_allocation_unit* newUnit = &pDesc->allocation_table[pDesc->info_block->allocation_tables].units[0];


    newUnit->type = FS_FREE;
    newUnit->size = freeUnit->size - sizeof(FS_allocation_table);
    newUnit->next_block = FS_ENDPOINT;
    newUnit->offset = freeUnit->offset + sizeof(FS_allocation_table);
    freeUnit->type = FS_SYSTEM;
    freeUnit->size = sizeof(FS_allocation_table);

    printf("NEW ALLOc: %d\n", freeBlock);
    pDesc->allocation_table[pDesc->info_block->allocation_tables].offset_next = FS_ENDPOINT;
    pDesc->allocation_table[pDesc->info_block->allocation_tables].unused_units = FS_ALLOC_UNITS - 1;
    pDesc->allocation_table[pDesc->info_block->allocation_tables - 1].offset_next = freeBlock;
    pDesc->info_block->allocation_tables += 1;
    return ST_OK;
}

int createDirectoryBlock(FS_descriptors* pDesc) {
    uint32_t nextBlock = allocateSystemBlock(pDesc, sizeof(FS_directory_table));
    if (nextBlock == FS_ENDPOINT)
        return ST_NOT_ENOUGH_SPACE;
    printf("AHA: %d\n", nextBlock);
    memset(&pDesc->directory_table[pDesc->info_block->directory_tables], 0, sizeof(FS_directory_table));
    pDesc->directory_table[pDesc->info_block->directory_tables].offset_next = FS_ENDPOINT;
    pDesc->directory_table[pDesc->info_block->directory_tables - 1].offset_next = nextBlock;
    pDesc->info_block->directory_tables += 1;
    return ST_OK;
}

uint32_t allocateSystemBlock(FS_descriptors* pDesc, uint32_t pSize) {
    uint32_t block;
    uint32_t unusedBlock;
    FS_allocation_unit* unit;
    FS_allocation_unit* unusedUnit;

    if (pSize > pDesc->info_block->size)
        return FS_ENDPOINT;

    block = findBlockSize(pDesc, FS_FREE, pSize);
    if (block == FS_ENDPOINT) {
        //TODO: DEFRAG
        return FS_ENDPOINT;
    }

    unit = &pDesc->allocation_table[block / FS_ALLOC_UNITS].units[block % FS_ALLOC_UNITS];
    unit->type = FS_SYSTEM;
    unit->next_block = FS_ENDPOINT;
    if (unit->size > pSize) {
        unusedBlock = findBlock(pDesc, FS_UNUSED);
        if (unusedBlock == FS_ENDPOINT) {
            if (createAllocationBlock(pDesc) != ST_OK)
                return FS_ENDPOINT;
            unusedUnit = &pDesc->allocation_table[pDesc->info_block->allocation_tables - 1].units[0];
        } else {
            unusedUnit = &pDesc->allocation_table[unusedBlock / FS_ALLOC_UNITS].units[unusedBlock % FS_ALLOC_UNITS];
        }

        unusedUnit->type = FS_FREE;
        unusedUnit->size = unit->size - pSize;
        unusedUnit->next_block = FS_ENDPOINT;
        unusedUnit->offset = unit->offset + pSize;
    }
    unit->size = pSize;
    pDesc->info_block->free -= pSize;
    return block;
}

uint32_t findBlock(FS_descriptors* pDesc, uint8_t pType) {
    for (uint32_t block = 0; block < pDesc->info_block->allocation_tables; ++block) {
        for (uint32_t unit = 0; unit < FS_ALLOC_UNITS; ++unit) {
            FS_allocation_unit* fsUnit = &pDesc->allocation_table[block].units[unit];
            if (fsUnit->type == pType)
                return block * FS_ALLOC_UNITS + unit;
        }
    }
    return FS_ENDPOINT;
}

uint32_t findBlockSize(FS_descriptors* pDesc, uint8_t pType, uint32_t pSize) {
    for (uint32_t block = 0; block < pDesc->info_block->allocation_tables; ++block) {
        for (uint32_t unit = 0; unit < FS_ALLOC_UNITS; ++unit) {
            FS_allocation_unit* fsUnit = &pDesc->allocation_table[block].units[unit];
            if (fsUnit->type == pType && fsUnit->size >= pSize)
                return block * FS_ALLOC_UNITS + unit;
        }
    }
    return FS_ENDPOINT;
}

size_t fsize(FILE* pFile) {
    size_t size;
    fpos_t pos;
    fgetpos(pFile, &pos);
    fseek(pFile, 0, SEEK_END);
    size = (uint32_t) ftell(pFile);
    fsetpos(pFile, &pos);
    return size;
}