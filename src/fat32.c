#include "fat32.h"    // your FAT32 struct
#include "lexer.h"    // tokenlist


int read_boot_sector(FILE *image, FAT32 *fs) {
    BPB bpb;

    // Read the BPB struct from the start of the file
    fseek(image, 0, SEEK_SET);
    if (fread(&bpb, sizeof(BPB), 1, image) != 1) {
        fprintf(stderr, "Error: Could not read BPB\n");
        return 1;
    }

    fs->bytes_per_sector = bpb.bytes_per_sector;
    fs->sectors_per_cluster = bpb.sectors_per_cluster;
    fs->reserved_sector_count = bpb.reserved_sector_count;
    fs->num_fats = bpb.num_fats;
    fs->fat_size = bpb.fat_size_32;
    fs->root_cluster = bpb.root_cluster;

    // Determine total sectors (prefer total_sectors_32)
    fs->total_sectors = bpb.total_sectors_32;

    // Calculate total clusters in data region
    uint32_t data_sectors = fs->total_sectors -
        (fs->reserved_sector_count + fs->num_fats * fs->fat_size);
    fs->total_clusters = data_sectors / fs->sectors_per_cluster;

    // Calculate entries in one FAT
    uint32_t fat_bytes = fs->fat_size * fs->bytes_per_sector;
    fs->fat_entries = fat_bytes / 4;

    return 0;
}

void print_fat32_info(FAT32 *fs) {
    printf("Root Cluster: %u\n", fs->root_cluster);
    printf("Bytes per Sector: %u\n", fs->bytes_per_sector);
    printf("Sectors per Cluster: %u\n", fs->sectors_per_cluster);
    printf("Total Clusters in Data Region: %u\n", fs->total_clusters);
    printf("Entries in One FAT: %u\n", fs->fat_entries);
    printf("Size of Image: %u bytes\n", fs->total_sectors * fs->bytes_per_sector);
}

