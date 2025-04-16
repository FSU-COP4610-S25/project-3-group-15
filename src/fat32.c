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
//FIANAS ATTEMPT
// #include "fat32.h"
//#include <stdio.h>
//
//int read_bpb(FILE *fp, BPB *bpb) {
//    fseek(fp, 0, SEEK_SET);
//    if (fread(bpb, sizeof(BPB), 1, fp) != 1) {
//        return -1;
//    }
//    return 0;
//}
//
//void print_info(const BPB *bpb) {
//    uint32_t TotSec = (bpb->BPB_TotSec16 != 0) ? bpb->BPB_TotSec16 : bpb->BPB_TotSec32;
//    uint32_t FATSz = (bpb->BPB_FATSz16 != 0) ? bpb->BPB_FATSz16 : bpb->BPB_FATSz32;
//
//    uint32_t DataSec = TotSec - (bpb->BPB_RsvdSecCnt + (bpb->BPB_NumFATs * FATSz));
//    uint32_t CountOfClusters = DataSec / bpb->BPB_SecPerClus;
//
//    printf("Position of root cluster: %u\n", bpb->BPB_RootClus);
//    printf("Bytes per sector: %u\n", bpb->BPB_BytsPerSec);
//    printf("Sectors per cluster: %u\n", bpb->BPB_SecPerClus);
//    printf("Total # of clusters in data region: %u\n", CountOfClusters);
//    printf("# of entries in one FAT: %u\n", (FATSz * bpb->BPB_BytsPerSec) / 4);
//    printf("Size of image (in bytes): %lu\n", (unsigned long)TotSec * bpb->BPB_BytsPerSec);
//}
