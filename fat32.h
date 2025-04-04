#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdio.h>

// Struct to store FAT32 boot sector fields
typedef struct {
    uint16_t bytes_per_sector;  // Bytes per sector
    uint8_t sectors_per_cluster; // Sectors per cluster
    uint16_t reserved_sector_count; // Reserved sectors (including boot sector)
    uint8_t num_fats;           // Number of FATs
    uint32_t total_sectors;      // Total sectors in file system
    uint32_t fat_size;          // Size of one FAT (in sectors)
    uint32_t root_cluster;      // First cluster of the root directory
    uint32_t total_clusters;    // Total clusters in data region (calculated)
} FAT32;

// Function declarations
int read_boot_sector(FILE *image, FAT32 *fs);
void print_fat32_info(FAT32 *fs);

#endif

//FIANAS ATTEMPT 
// #ifndef FAT32_H
//#define FAT32_H
//
//#include <stdint.h>
//#include <stdio.h>
//
//#pragma pack(push, 1)
//typedef struct {
//    uint8_t BS_jmpBoot[3];
//    uint8_t BS_OEMName[8];
//    uint16_t BPB_BytsPerSec;
//    uint8_t BPB_SecPerClus;
//    uint16_t BPB_RsvdSecCnt;
//    uint8_t BPB_NumFATs;
//    uint16_t BPB_RootEntCnt;
//    uint16_t BPB_TotSec16;
//    uint8_t BPB_Media;
//    uint16_t BPB_FATSz16;
//    uint16_t BPB_SecPerTrk;
//    uint16_t BPB_NumHeads;
//    uint32_t BPB_HiddSec;
//    uint32_t BPB_TotSec32;
//    uint32_t BPB_FATSz32;
//    uint16_t BPB_ExtFlags;
//    uint16_t BPB_FSVer;
//    uint32_t BPB_RootClus;
//    uint16_t BPB_FSInfo;
//    uint16_t BPB_BkBootSec;
//    uint8_t BPB_Reserved[12];
//    uint8_t BS_DrvNum;
//    uint8_t BS_Reserved1;
//    uint8_t BS_BootSig;
//    uint32_t BS_VolID;
//    uint8_t BS_VolLab[11];
//    uint8_t BS_FilSysType[8];
//} BPB;
//#pragma pack(pop)
//
//int read_bpb(FILE *fp, BPB *bpb);
//void print_info(const BPB *bpb);
//
//#endif