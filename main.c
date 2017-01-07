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

int createFS(FILE* pFile, uint32_t pBytes);

int status(FILE* pFile);

int tree(FILE* pFile);

int addFile(FILE* pVirtualDrive, FILE* pFile, char* pFileName);

void blockCopyFromFile(FILE* pDrive, FILE* pFile, FS_allocation_unit* pUnit, uint32_t pSize);

int getFile(FILE* pDrive, FILE* pDest, char* pFilename);

int findFile(FS_file_entry* pFile, FS_directory_table* pDirectory, char* pFilename);

void blockCopyToFile(FILE* pDrive, FILE* pFile, FS_allocation_unit* pUnit, uint32_t pSize);

int main(int argc, char** argv) {
    char* drive;


    if (argc < 2) {
        printf("Provide correct module: \n");
        printf("create, drop, add, remove, tree, status, version\n");
        printf("are allowed\n");
        return 1;
    }

    if (argc >= 3)
        drive = argv[2];

    if (!strcmp(argv[1], "version")) {
        printf("FileSystem version "FS_VERSION"\n");
        printf("(c)2017 Wojciech Gruszka\n");
        return 0;
    }

    if (!strcmp(argv[1], "create")) {
        char* filename;
        int bytes;
        FILE* virtualDrive;
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
        return createFS(virtualDrive, (uint32_t) bytes);
    }

    if (!strcmp(argv[1], "status")) {
        char* filename;
        FILE* virtualDrive;
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
        return status(virtualDrive);
    }

    if (!strcmp(argv[1], "tree")) {
        char* filename;
        FILE* virtualDrive;
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
        return tree(virtualDrive);
    }

    if (!strcmp(argv[1], "add")) {
        char* virtname;
        char* filename;
        FILE* virtualDrive;
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
        fseek(virtualDrive, 0, SEEK_SET);
        return addFile(virtualDrive, file, filename);
    }

    if (!strcmp(argv[1], "get")) {
        char* virtname;
        char* destname;
        char* filename;
        FILE* virtualDrive;
        FILE* dest;
        if (argc < 3) {
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
        fseek(virtualDrive, 0, SEEK_SET);
        return getFile(virtualDrive, dest, filename);
    }

    return 0;
}

int createFS(FILE* pFile, uint32_t pBytes) {
    FS_info header;
    FS_directory_table table;
    FS_allocation_table alloc;
    uint8_t dummy = 0;
    header.magic[0] = 'G';
    header.magic[1] = 'F';
    header.magic[2] = 'S';
    strncpy(header.version, FS_VERSION, 5);
    header.size = pBytes;
    header.free = pBytes;
    alloc.offset_next = 0;
    alloc.unused_units = FS_ALLOC_UNITS - 1;
    table.files_flags = 0;

    for (uint32_t i = 1; i < FS_ALLOC_UNITS; ++i)
        alloc.units[i].type = FS_UNUSED;
    alloc.units[0].type = FS_FREE;
    alloc.units[0].offset = 0;
    alloc.units[0].size = pBytes;
    alloc.units[0].next_block = FS_ENDPOINT;

    fwrite(&header, sizeof(header), 1, pFile);
    fwrite(&alloc, sizeof(alloc), 1, pFile);
    fwrite(&table, sizeof(table), 1, pFile);
    for (uint32_t i = 0; i < pBytes; i++)
        fwrite(&dummy, sizeof(dummy), 1, pFile);
    fclose(pFile);
    return ST_OK;
}

int addFile(FILE* pVirtualDrive, FILE* pFile, char* pFileName) {
    FS_info header;
    FS_directory_table table;
    FS_allocation_table alloc;
    FS_file_entry desc;
    uint32_t size;

    fread(&header, sizeof(FS_info), 1, pVirtualDrive);
    fread(&alloc, sizeof(FS_allocation_table), 1, pVirtualDrive);
    fread(&table, sizeof(FS_directory_table), 1, pVirtualDrive);

    if (memcmp(header.magic, "GFS", 3)) {
        fclose(pFile);
        fclose(pVirtualDrive);
        return ST_NOT_VALID_FILE;
    }

    fseek(pFile, 0, SEEK_END);
    size = (uint32_t) ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    if (header.free < size) {
        fclose(pFile);
        fclose(pVirtualDrive);
        printf("Not enough space!\n");
        printf("Free: %d\n", header.free);
        printf("Required: %d\n", size);
        return ST_NOT_ENOUGH_SPACE;
    }

    if (findFile(NULL, &table, pFileName) == ST_OK) {
        fclose(pFile);
        fclose(pVirtualDrive);
        printf("File with name: %s already exists!\n", pFileName);
        return ST_EXISTS;
    }


    desc.size = size;
    strcpy(desc.name, pFileName);

    desc.exists = 1;

    if (!table.files_flags) {
        //TODO: ALLOCATE DIRECTORY TABLE
    }

    //CHECK IF SUFFICIENT TABLE, ALLOC, and copy
    uint32_t last_block = FS_ENDPOINT;
    for (uint32_t idx = 0; idx < FS_ALLOC_UNITS; ++idx) {
        if (alloc.units[idx].type == FS_FREE) {
            // LINK TO LAST BLOCK
            if (last_block != FS_ENDPOINT)
                alloc.units[last_block].next_block = idx;
            else
                desc.block = idx;

            if (alloc.units[idx].size > size) {
                blockCopyFromFile(pVirtualDrive, pFile, &alloc.units[idx], size);

                if (alloc.unused_units == 0) {
                    //TODO: ALLOC NEW TABLE
                }
                // FIND NEW BLOCK
                uint32_t unused_block = 0;
                for (; unused_block < FS_ALLOC_UNITS; ++unused_block)
                    if (alloc.units[unused_block].type == FS_UNUSED) break;
                // SET DESCRIPTORS
                alloc.units[unused_block].type = FS_FREE;
                alloc.units[unused_block].size = alloc.units[idx].size - size;
                alloc.units[unused_block].offset = alloc.units[idx].offset + size;
                alloc.units[idx].size = size;
                alloc.units[idx].type = FS_OCCUPIED;
                alloc.units[idx].next_block = FS_ENDPOINT;
                alloc.unused_units -= 1;
                break;
            } else if (alloc.units[idx].size <= size) {
                blockCopyFromFile(pVirtualDrive, pFile, &alloc.units[idx], alloc.units[idx].size);
                alloc.units[idx].type = FS_OCCUPIED;
                alloc.units[idx].next_block = FS_ENDPOINT;
                size -= alloc.units[idx].size;
                last_block = idx;
            }
        }
    }

    uint32_t i = 0;
    for (; i < FS_DIRECTORY_FILES; ++i)
        if (!((table.files_flags >> i) & 1)) break;
    table.files[i] = desc;
    table.files_flags |= (1 << i);
    header.free -= size;

    fseek(pVirtualDrive, 0, SEEK_SET);
    fwrite(&header, sizeof(FS_info), 1, pVirtualDrive);
    fwrite(&alloc, sizeof(FS_allocation_table), 1, pVirtualDrive);
    fwrite(&table, sizeof(FS_directory_table), 1, pVirtualDrive);

    fclose(pFile);
    fclose(pVirtualDrive);
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
        fclose(pDrive);
        return ST_NOT_VALID_FILE;
    }

    if (findFile(&file, &table, pFilename)) {
        fclose(pDest);
        fclose(pDrive);
        return ST_NOT_FOUND;
    }

    block = file.block;
    while (block != FS_ENDPOINT) {
        blockCopyToFile(pDrive, pDest, &alloc.units[block], alloc.units[block].size);
        block = alloc.units[block].next_block;
    }

    fclose(pDest);
    fclose(pDrive);
    return ST_OK;
}

int tree(FILE* pFile) {
    FS_info header;
    FS_directory_table table;

    fread(&header, sizeof(FS_info), 1, pFile);
    if (memcmp(header.magic, "GFS", 3)) {
        fclose(pFile);
        return ST_NOT_VALID_FILE;
    }
    fseek(pFile, sizeof(FS_allocation_table), SEEK_CUR);
    fread(&table, sizeof(FS_directory_table), 1, pFile);
    printf("Files: \n");
    for (uint32_t i = 0; i < FS_DIRECTORY_FILES; ++i) {
        if ((table.files_flags >> i) & 1) {
            printf("%s \t %d bytes\n", table.files[i].name, table.files[i].size);
        }
    }
    return ST_OK;
}

int status(FILE* pFile) {
    FS_info header;
    FS_directory_table table;
    FS_allocation_table alloc;
    uint8_t version[6];
    fread(&header, sizeof(FS_info), 1, pFile);
    if (memcmp(header.magic, "GFS", 3)) {
        fclose(pFile);
        return ST_NOT_VALID_FILE;
    }
    memcpy(version, header.version, 5);
    version[5] = 0;

    printf("FileSystem\n");
    printf("Version: %s\nSize: %d\nFree: %d\n", version, header.size, header.free);
    fread(&alloc, sizeof(FS_allocation_table), 1, pFile);
    fread(&table, sizeof(FS_directory_table), 1, pFile);

    for (uint32_t i = 0; i < FS_ALLOC_UNITS; ++i) {
        if (alloc.units[i].type == FS_FREE) {
            printf("FREE BLOCK [%2d, %2d]\tSIZE: %d\n", 0, i, alloc.units[i].size);
        } else if (alloc.units[i].type == FS_OCCUPIED) {
            printf("USED BLOCK [%2d, %2d]\tSIZE: %d\tNEXT: %d\n", 0, i, alloc.units[i].size, alloc.units[i].next_block);
        }
    }
    fclose(pFile);
    return ST_OK;
}


int findFile(FS_file_entry* pFile, FS_directory_table* pDirectory, char* pFilename) {
    //TODO: multiple tables
    for (uint32_t i = 0; i < FS_DIRECTORY_FILES; ++i) {
        if ((pDirectory->files_flags >> i) & 1 && strcmp(pDirectory->files[i].name, pFilename) == 0) {
            if (pFile != NULL)
                *pFile = pDirectory->files[i];
            return ST_OK;
        }
    }
    return ST_NOT_FOUND;
}

void blockCopyFromFile(FILE* pDrive, FILE* pFile, FS_allocation_unit* pUnit, uint32_t pSize) {
    uint8_t buf[1024];
    fseek(pDrive, FS_DATA_OFFSET + pUnit->offset, SEEK_SET);
    while (pSize > 0) {
        uint32_t size;
        if (pSize > 1024) size = 1024;
        else size = pSize;
        fread(buf, sizeof(uint8_t), pSize, pFile);
        fwrite(buf, sizeof(uint8_t), pSize, pDrive);
        pSize -= size;
    }
}

void blockCopyToFile(FILE* pDrive, FILE* pFile, FS_allocation_unit* pUnit, uint32_t pSize) {
    uint8_t buf[1024];
    fseek(pDrive, FS_DATA_OFFSET + pUnit->offset, SEEK_SET);
    while (pSize > 0) {
        uint32_t size;
        if (pSize > 1024) size = 1024;
        else size = pSize;
        fread(buf, sizeof(uint8_t), pSize, pDrive);
        fwrite(buf, sizeof(uint8_t), pSize, pFile);
        pSize -= size;
    }
}
