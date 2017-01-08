#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
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

int createFS(FILE* pDrive, uint32_t pBytes);

int addFile(FILE* pDrive, FILE* pFile, char* pFilename);

int getFile(FILE* pDrive, FILE* pDest, char* pFilename);

int removeFile(FILE* pDrive, char* pFile);

int status(FILE* pDrive);

int tree(FILE* pDrive);

int findFile(FS_file_entry* pFile, uint32_t* pIndex, FS_directory_table* pDirectory, char* pFilename);

void blockCopy(FILE* pDrive, FILE* pFile, FS_allocation_unit* pUnit, uint32_t pSize, uint8_t pDirection) ;

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
    FS_info header;
    FS_directory_table directoryTable;
    FS_allocation_table allocationTable;
    FS_file_entry fileEntry;
    uint32_t size;

    fread(&header, sizeof(FS_info), 1, pDrive);
    fread(&allocationTable, sizeof(FS_allocation_table), 1, pDrive);
    fread(&directoryTable, sizeof(FS_directory_table), 1, pDrive);

    if (memcmp(header.magic, "GFS", 3)) {
        fclose(pFile);
        return ST_NOT_VALID_FILE;
    }

    fseek(pFile, 0, SEEK_END);
    size = (uint32_t) ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    if (header.free < size) {
        fclose(pFile);
        fclose(pDrive);
        printf("Not enough space!\n");
        printf("Free: %d\n", header.free);
        printf("Required: %d\n", size);
        return ST_NOT_ENOUGH_SPACE;
    }

    if (findFile(NULL, NULL, &directoryTable, pFilename) == ST_OK) {
        fclose(pFile);
        printf("File with name: %s already exists!\n", pFilename);
        return ST_EXISTS;
    }


    fileEntry.size = size;
    strcpy(fileEntry.name, pFilename);

    fileEntry.exists = 1;

    if (!directoryTable.files_flags) {
        //TODO: ALLOCATE DIRECTORY TABLE
    }

    //CHECK IF SUFFICIENT TABLE, ALLOC, and copy
    uint32_t last_block = FS_ENDPOINT;
    for (uint32_t idx = 0; idx < FS_ALLOC_UNITS; ++idx) {
        if (allocationTable.units[idx].type == FS_FREE) {
            // LINK TO LAST BLOCK
            if (last_block != FS_ENDPOINT)
                allocationTable.units[last_block].next_block = idx;
            else
                fileEntry.block = idx;

            if (allocationTable.units[idx].size > size) {
                blockCopy(pDrive, pFile, &allocationTable.units[idx], size, FS_DIR_FROM_FILE);

                if (allocationTable.unused_units == 0) {
                    //TODO: ALLOC NEW TABLE
                }
                // FIND NEW BLOCK
                uint32_t unused_block = 0;
                for (; unused_block < FS_ALLOC_UNITS; ++unused_block)
                    if (allocationTable.units[unused_block].type == FS_UNUSED) break;
                // SET DESCRIPTORS
                allocationTable.units[unused_block].type = FS_FREE;
                allocationTable.units[unused_block].size = allocationTable.units[idx].size - size;
                allocationTable.units[unused_block].offset = allocationTable.units[idx].offset + size;
                allocationTable.units[idx].size = size;
                allocationTable.units[idx].type = FS_OCCUPIED;
                allocationTable.units[idx].next_block = FS_ENDPOINT;
                allocationTable.unused_units -= 1;
                break;
            } else if (allocationTable.units[idx].size <= size) {
                blockCopy(pDrive, pFile, &allocationTable.units[idx], allocationTable.units[idx].size, FS_DIR_FROM_FILE);
                allocationTable.units[idx].type = FS_OCCUPIED;
                allocationTable.units[idx].next_block = FS_ENDPOINT;
                size -= allocationTable.units[idx].size;
                last_block = idx;
                if (size == 0)
                    break;
            }
        }
    }

    uint32_t i = 0;
    for (; i < FS_DIRECTORY_FILES; ++i)
        if (!((directoryTable.files_flags >> i) & 1)) break;
    directoryTable.files[i] = fileEntry;
    directoryTable.files_flags |= (1 << i);
    header.free -= size;

    fseek(pDrive, 0, SEEK_SET);
    fwrite(&header, sizeof(FS_info), 1, pDrive);
    fwrite(&allocationTable, sizeof(FS_allocation_table), 1, pDrive);
    fwrite(&directoryTable, sizeof(FS_directory_table), 1, pDrive);

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
        blockCopy(pDrive, pDest, &alloc.units[block], alloc.units[block].size, FS_DIR_TO_FILE);
        block = alloc.units[block].next_block;
    }

    fclose(pDest);
    return ST_OK;
}


int removeFile(FILE* pDrive, char* pFile) {
    FS_info header;
    FS_directory_table table;
    FS_allocation_table alloc;
    FS_file_entry file;
    uint32_t block;
    uint32_t file_idx;
    fread(&header, sizeof(FS_info), 1, pDrive);
    fread(&alloc, sizeof(FS_allocation_table), 1, pDrive);
    fread(&table, sizeof(FS_directory_table), 1, pDrive);

    if (memcmp(header.magic, "GFS", 3))
        return ST_NOT_VALID_FILE;

    if (findFile(&file, &file_idx, &table, pFile))
        return ST_NOT_FOUND;

    block = file.block;
    while (block != FS_ENDPOINT) {
        alloc.units[block].type = FS_FREE;
        uint32_t cnt = 0;
        for (uint32_t i = 0; i < FS_ALLOC_UNITS && cnt != 2; ++i) {
            //left block
            if (alloc.units[block].offset == alloc.units[i].offset + alloc.units[i].size) {
                if (alloc.units[i].type == FS_FREE) {
                    alloc.units[block].type = FS_UNUSED;
                    alloc.unused_units += 1;
                    alloc.units[i].size += alloc.units[block].size;
                    cnt += 1;
                    block = i;
                }
                //right block
            } else if (alloc.units[block].offset + alloc.units[block].size == alloc.units[i].offset) {
                if (alloc.units[i].type == FS_FREE) {
                    alloc.units[i].type = FS_UNUSED;
                    alloc.unused_units += 1;
                    alloc.units[block].size += alloc.units[i].size;
                    cnt += 1;
                }
            }
        }
        block = alloc.units[block].next_block;
    }
    table.files[file_idx].exists = 0;
    table.files_flags &= ~(1 << file_idx);
    header.free += file.size;

    fseek(pDrive, 0, SEEK_SET);
    fwrite(&header, sizeof(FS_info), 1, pDrive);
    fwrite(&alloc, sizeof(FS_allocation_table), 1, pDrive);
    fwrite(&table, sizeof(FS_directory_table), 1, pDrive);

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


int findFile(FS_file_entry* pFile, uint32_t* pIndex, FS_directory_table* pDirectory, char* pFilename) {
    //TODO: multiple tables
    for (uint32_t i = 0; i < FS_DIRECTORY_FILES; ++i) {
        if ((pDirectory->files_flags >> i) & 1 && strcmp(pDirectory->files[i].name, pFilename) == 0) {
            if (pFile != NULL)
                *pFile = pDirectory->files[i];
            if (pIndex != NULL)
                *pIndex = i;
            return ST_OK;
        }
    }
    return ST_NOT_FOUND;
}


void blockCopy(FILE* pDrive, FILE* pFile, FS_allocation_unit* pUnit, uint32_t pSize, uint8_t pDirection) {
    uint8_t buf[1024];
    fseek(pDrive, FS_DATA_OFFSET + pUnit->offset, SEEK_SET);
    while (pSize > 0) {
        uint32_t size;
        if (pSize > 1024) size = 1024;
        else size = pSize;
        if (pDirection == FS_DIR_FROM_FILE) {
            fread(buf, sizeof(uint8_t), pSize, pFile);
            fwrite(buf, sizeof(uint8_t), pSize, pDrive);
        } else if (pDirection == FS_DIR_TO_FILE) {
            fread(buf, sizeof(uint8_t), pSize, pDrive);
            fwrite(buf, sizeof(uint8_t), pSize, pFile);
        }
        pSize -= size;
    }
}
