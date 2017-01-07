#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "descriptors.h"
#include "version.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define FS_VERSION  STR(MAJOR_VERSION)"."STR(MINOR_VERSION)

int createFS(FILE* pFile, uint32_t pBytes);

int status(FILE* pFile);

int tree(FILE* pFile);

int addFile(FILE* pVirtualDrive, FILE* pFile, char* pFileName);

int main(int argc, char** argv) {

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
    table.files_flags = 0;
    fwrite(&header, sizeof(header), 1, pFile);
    fwrite(&alloc, sizeof(alloc), 1, pFile);
    fwrite(&table, sizeof(table), 1, pFile);
    for (uint32_t i = 0; i < pBytes; i++)
        fwrite(&dummy, sizeof(dummy), 1, pFile);
    fclose(pFile);
    return 0;
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
        printf("Not valid GFS file!\n");
        fclose(pFile);
        fclose(pVirtualDrive);
        return 1;
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
        return -1;
    }

    //TODO: UNIQUE NAME

    desc.size = size;
    strcpy(desc.name, pFileName);

    desc.exists = 1;

    if (~table.files_flags) { //Empty slot
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
    } else {
        //ADD NEW SLOT
        return -1;
    }
    fclose(pFile);
    fclose(pVirtualDrive);
    return 0;
}


int tree(FILE* pFile) {
    FS_info header;
    FS_directory_table table;

    fread(&header, sizeof(FS_info), 1, pFile);
    if (memcmp(header.magic, "GFS", 3)) {
        printf("Not valid GFS file!\n");
        fclose(pFile);
        return 1;
    }
    fseek(pFile, sizeof(FS_allocation_table), SEEK_CUR);
    fread(&table, sizeof(FS_directory_table), 1, pFile);
    printf("Files: \n");
    for (uint32_t i = 0; i < FS_DIRECTORY_FILES; ++i) {
        if ((table.files_flags >> i) & 1) {
            printf("%s \t %d bytes\n", table.files[i].name, table.files[i].size);
        }
    }
    return 0;
}

int status(FILE* pFile) {
    FS_info header;
    FS_directory_table table;
    FS_allocation_table alloc;
    uint8_t version[6];
    fread(&header, sizeof(FS_info), 1, pFile);
    if (memcmp(header.magic, "GFS", 3)) {
        printf("Not valid GFS file!\n");
        fclose(pFile);
        return 1;
    }
    memcpy(version, header.version, 5);
    version[5] = 0;

    printf("FileSystem\n");
    printf("Version: %s\nSize: %d\nFree: %d\n", version, header.size, header.free);
    fread(&alloc, sizeof(FS_allocation_table), 1, pFile);
    fread(&table, sizeof(FS_directory_table), 1, pFile);

    fclose(pFile);
    return 0;
}

