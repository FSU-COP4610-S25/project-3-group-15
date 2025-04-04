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
