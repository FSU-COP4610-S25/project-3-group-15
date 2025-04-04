/*
hear me out ok
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fat32.h"
#include "lexer.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [FAT32 IMAGE FILE]\n", argv[0]);
        return 1;
    }

    FILE *image = fopen(argv[1], "rb");
    if (!image) {
        fprintf(stderr, "Error: Unable to open file %s\n", argv[1]);
        return 1;
    }

    FAT32 fat32;
    if (read_boot_sector(image, &fat32) != 0) {
        fclose(image);
        return 1;
    }

    while (1) {
        printf("[%s]/> ", argv[1]);

        char *input = get_input();
        tokenlist *tokens = get_tokens(input);

        if (tokens->size > 0) {
            if (strcmp(tokens->items[0], "info") == 0) {
                print_fat32_info(&fat32);
            } 
            else if (strcmp(tokens->items[0], "exit") == 0) {
                break;
            } 
            else {
                printf("Unknown command: %s\n", tokens->items[0]);
            }
        }

        free(input);
        free_tokens(tokens);
    }

    fclose(image);
    return 0;
}

*/




#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//
//FILE *image = NULL;
//
//#pragma pack(push, 1)
//typedef struct {
//    unsigned char jump[3];
//    char oem[8];
//    unsigned short bytesPerSector;
//    unsigned char sectorsPerCluster;
//    unsigned short reservedSectorCount;
//    unsigned char numFATs;
//    unsigned short rootEntryCount;
//    unsigned short totalSectors16;
//    unsigned char media;
//    unsigned short fatSize16;
//    unsigned short sectorsPerTrack;
//    unsigned short numHeads;
//    unsigned int hiddenSectors;
//    unsigned int totalSectors32;
//
//    // FAT32 extended
//    unsigned int fatSize32;
//    unsigned short extFlags;
//    unsigned short fsVersion;
//    unsigned int rootCluster;
//    unsigned short fsInfo;
//    unsigned short backupBootSector;
//    char reserved[12];
//    unsigned char driveNumber;
//    unsigned char reserved1;
//    unsigned char bootSig;
//    unsigned int volumeID;
//    char volumeLabel[11];
//    char fsType[8];
//} BPB_struct;
//#pragma pack(pop)
//
//BPB_struct bpb;
//
//// Reads and parses the boot sector into the BPB structure
//void load_bpb() {
//    fseek(image, 0, SEEK_SET);
//    fread(&bpb, sizeof(BPB_struct), 1, image);
//}
//
//// Handles the `info` command
//void print_info() {
//    unsigned int fatSize = bpb.fatSize32;
//    unsigned int totalSectors = (bpb.totalSectors16 == 0) ? bpb.totalSectors32 : bpb.totalSectors16;
//
//    unsigned int rootDirSectors = ((bpb.rootEntryCount * 32) + (bpb.bytesPerSector - 1)) / bpb.bytesPerSector;
//    unsigned int dataSectors = totalSectors - (bpb.reservedSectorCount + (bpb.numFATs * fatSize) + rootDirSectors);
//    unsigned int clusterCount = dataSectors / bpb.sectorsPerCluster;
//
//    unsigned int entriesPerFAT = (fatSize * bpb.bytesPerSector) / 4;
//
//    fseek(image, 0, SEEK_END);
//    long size = ftell(image);
//
//    printf("Root Cluster: %u\n", bpb.rootCluster);
//    printf("Bytes per Sector: %u\n", bpb.bytesPerSector);
//    printf("Sectors per Cluster: %u\n", bpb.sectorsPerCluster);
//    printf("Total Clusters in Data Region: %u\n", clusterCount);
//    printf("Entries in one FAT: %u\n", entriesPerFAT);
//    printf("Size of image: %ld bytes\n", size);
//}
//
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [FAT32 IMAGE FILE]\n", argv[0]);
        return 1;
    }

    image = fopen(argv[1], "rb");
    if (!image) {
        perror("Error opening image");
        return 1;
    }

//    load_bpb();
//
//    char command[128];
//    while (1) {
//        printf("%s/> ", argv[1]);  // prompt
//        if (!fgets(command, sizeof(command), stdin)) break;
//
//        // Remove newline character
//        command[strcspn(command, "\n")] = 0;
//
//        if (strcmp(command, "exit") == 0) {
//            break;
//        } else if (strcmp(command, "info") == 0) {
//            print_info();
//        } else {
//            printf("Unknown command: %s\n", command);
//        }
//    }
  
// insert while loop similar to that from shell
    while(1) {
        //print prompt
        if (command == "exit")
            break;
        //else, do the system
        else if (command == "info") {
            //implement info here 
        }
    }
  
    fclose(image);
    printf("Image unmounted.\n");
    return 0;
}



