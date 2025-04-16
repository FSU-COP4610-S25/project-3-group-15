#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "fat32.h"    // your FAT32 struct
#include "lexer.h"    // tokenlist

// Compute and return the first sector number of a given data-cluster:
static uint32_t first_sector_of(FAT32 *fs, uint32_t cluster) {
    uint32_t first_data = fs->reserved_sector_count
                          + (fs->num_fats * fs->fat_size);
    return ((cluster - 2) * fs->sectors_per_cluster)
           + first_data;
}

// ls: list everything in 'cluster'
void list_directory(FILE *img, FAT32 *fs, uint32_t cluster) {
    uint32_t sector = first_sector_of(fs, cluster);
    uint32_t bytes_per_cluster = fs->bytes_per_sector * fs->sectors_per_cluster;
    uint8_t *buf = malloc(bytes_per_cluster);
    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fread(buf, 1, bytes_per_cluster, img);

    for (uint32_t off = 0; off < bytes_per_cluster; off += 32) {
        uint8_t first = buf[off];
        if (first == 0x00) break;              // no more entries
        if (first == 0xE5) continue;           // deleted
        uint8_t attr = buf[off + 11];
        if ((attr & 0x0F) == 0x0F) continue;    // long‑name entry :contentReference[oaicite:3]{index=3}&#8203;:contentReference[oaicite:4]{index=4}

        // extract short name + extension
        char name[9], ext[4], full[13];
        memcpy(name,  &buf[off],   8); name[8] = '\0';
        memcpy(ext,   &buf[off+8], 3); ext[3] = '\0';
        // trim trailing spaces
        for (int i = 7; i >= 0 && name[i]==' ';   i--) name[i] = '\0';
        for (int i = 2; i >= 0 && ext[i]==' ';    i--) ext[i]  = '\0';

        if (ext[0])
            snprintf(full, sizeof(full), "%s.%s", name, ext);
        else
            snprintf(full, sizeof(full), "%s", name);

        // print as directory or file
        if (attr & 0x10)                // ATTR_DIRECTORY :contentReference[oaicite:5]{index=5}&#8203;:contentReference[oaicite:6]{index=6}
            printf("[DIR] %s\n", full);
        else
            printf("      %s\n", full);
    }
    free(buf);
}

// cd: change current_cluster to the directory named tokens->items[1]
int change_directory(FILE *img, FAT32 *fs, uint32_t *current_cluster, tokenlist *tokens) {
    if (tokens->size < 2) {
        printf("Usage: cd <directory>\n");
        return -1;
    }
    const char *target = tokens->items[1];
    if (strcmp(target, "..") == 0) {
        *current_cluster = fs->root_cluster;
        return 0;
    }

    // read the same cluster as in ls()
    uint32_t sector = first_sector_of(fs, *current_cluster);
    uint32_t bytes_per_cluster = fs->bytes_per_sector * fs->sectors_per_cluster;
    uint8_t *buf = malloc(bytes_per_cluster);
    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fread(buf, 1, bytes_per_cluster, img);

    for (uint32_t off = 0; off < bytes_per_cluster; off += 32) {
        uint8_t first = buf[off];
        if (first == 0x00) break;
        if (first == 0xE5) continue;
        uint8_t attr = buf[off + 11];
        if ((attr & 0x0F) == 0x0F) continue;
        if (!(attr & 0x10))    // must be a directory :contentReference[oaicite:7]{index=7}&#8203;:contentReference[oaicite:8]{index=8}
            continue;

        // build short name
        char name[9], ext[4];
        memcpy(name,  &buf[off],   8); name[8] = '\0';
        memcpy(ext,   &buf[off+8], 3); ext[3] = '\0';
        for (int i = 7; i >= 0 && name[i]==' '; i--) name[i] = '\0';
        for (int i = 2; i >= 0 && ext[i]==' ';  i--) ext[i]  = '\0';

        if ((ext[0] && strcmp(tokens->items[1], ext)==0)  ||
            (!ext[0] && strcmp(tokens->items[1], name)==0)) {
            // fetch its first cluster
            uint16_t hi = *(uint16_t *)&buf[off + 20];
            uint16_t lo = *(uint16_t *)&buf[off + 26];
            *current_cluster = ((uint32_t)hi << 16) | lo;
            free(buf);
            return 0;
        }
    }
    free(buf);
    return -1;
}

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

