#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdio.h>

#pragma pack(push, 1)
typedef struct {
    uint8_t jump[3];              // 0x00
    uint8_t oem[8];               // 0x03
    uint16_t bytes_per_sector;   // 0x0B
    uint8_t sectors_per_cluster; // 0x0D
    uint16_t reserved_sector_count; // 0x0E
    uint8_t num_fats;            // 0x10
    uint16_t root_entry_count;   // 0x11 (unused in FAT32)
    uint16_t total_sectors_16;   // 0x13
    uint8_t media;               // 0x15
    uint16_t fat_size_16;        // 0x16 (unused in FAT32)
    uint16_t sectors_per_track;  // 0x18
    uint16_t num_heads;          // 0x1A
    uint32_t hidden_sectors;     // 0x1C
    uint32_t total_sectors_32;   // 0x20
    uint32_t fat_size_32;        // 0x24
    uint16_t ext_flags;          // 0x28
    uint16_t fs_version;         // 0x2A
    uint32_t root_cluster;       // 0x2C
    // (You can include more fields if needed)
} BPB;
#pragma pack(pop)

typedef struct {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint32_t total_sectors;
    uint32_t fat_size;
    uint32_t root_cluster;
    uint32_t total_clusters;
    uint32_t fat_entries;
} FAT32;

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