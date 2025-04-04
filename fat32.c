#include "fat32.h"
#include <stdlib.h>

int read_boot_sector(FILE *image, FAT32 *fs) {
    uint8_t buffer[512];

    if (fread(buffer, 1, 512, image) != 512) {
        fprintf(stderr, "Error: Could not read boot sector\n");
        return 1;
    }

    // Parse relevant FAT32 fields from the boot sector
    fs->bytes_per_sector = *(uint16_t *)&buffer[11]; 
    fs->sectors_per_cluster = buffer[13];            
    fs->reserved_sector_count = *(uint16_t *)&buffer[14]; 
    fs->num_fats = buffer[16];                        
    fs->total_sectors = *(uint32_t *)&buffer[32];    
    fs->fat_size = *(uint32_t *)&buffer[36];         
    fs->root_cluster = *(uint32_t *)&buffer[44];     

    // Calculate total clusters in data region
    uint32_t data_sectors = fs->total_sectors - (fs->reserved_sector_count + (fs->num_fats * fs->fat_size));
    fs->total_clusters = data_sectors / fs->sectors_per_cluster;

    return 0;
}

void print_fat32_info(FAT32 *fs) {
    printf("Root Cluster: %u\n", fs->root_cluster);
    printf("Bytes per Sector: %u\n", fs->bytes_per_sector);
    printf("Sectors per Cluster: %u\n", fs->sectors_per_cluster);
    printf("Total Clusters in Data Region: %u\n", fs->total_clusters);
    printf("Entries in One FAT: %u\n", fs->num_fats);
    printf("Size of Image: %u bytes\n", fs->total_sectors * fs->bytes_per_sector);
}
