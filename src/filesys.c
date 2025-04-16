#include "fat32.h"
#include "lexer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Compute the first sector of a given data cluster
static uint32_t first_sector_of(FAT32 *fs, uint32_t cluster) {
    uint32_t first_data = fs->reserved_sector_count
                          + (fs->num_fats * fs->fat_size);
    return ((cluster - 2) * fs->sectors_per_cluster)
           + first_data;
}

// ls: list ".", "..", then every entry in the current cluster
void list_directory(FILE *img, FAT32 *fs, uint32_t cluster) {
    printf(".\n..\n");  // always include these two
    uint32_t sector = first_sector_of(fs, cluster);
    uint32_t bytes_per_cluster = fs->bytes_per_sector * fs->sectors_per_cluster;
    uint8_t *buf = malloc(bytes_per_cluster);
    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fread(buf, 1, bytes_per_cluster, img);

    for (uint32_t off = 0; off < bytes_per_cluster; off += 32) {
        uint8_t first = buf[off];
        if (first == 0x00) break;                   // no more entries
        if (first == 0xE5) continue;                // deleted
        uint8_t attr = buf[off + 11];
        if ((attr & 0x0F) == 0x0F) continue;         // long‑name entry :contentReference[oaicite:0]{index=0}&#8203;:contentReference[oaicite:1]{index=1}
        if (buf[off] == '.')    continue;           // skip real "." and ".."

        // extract name and extension
        char name[9] = {0}, ext[4] = {0}, full[13];
        memcpy(name, &buf[off], 8);
        memcpy(ext,  &buf[off+8], 3);
        // trim trailing spaces
        for (int i = 7; i >= 0 && name[i]==' '; i--) name[i] = '\0';
        for (int i = 2; i >= 0 && ext[i]==' ';  i--) ext[i]  = '\0';

        if (ext[0])
            snprintf(full, sizeof(full), "%s.%s", name, ext);
        else
            snprintf(full, sizeof(full), "%s", name);

        printf("%s\n", full);
    }
    free(buf);
}

// cd: return  0 on success
//            1 if not found
//            2 if found but not a directory
int change_directory(FILE *img, FAT32 *fs, uint32_t *current_cluster, tokenlist *tokens) {
    if (tokens->size < 2) {
        printf("Usage: cd <directory>\n");
        return 1;
    }
    const char *target = tokens->items[1];
    if (strcmp(target, ".") == 0) return 0;
    if (strcmp(target, "..") == 0) {
        *current_cluster = fs->root_cluster;
        return 0;
    }

    uint32_t sector = first_sector_of(fs, *current_cluster);
    uint32_t bytes_per_cluster = fs->bytes_per_sector * fs->sectors_per_cluster;
    uint8_t *buf = malloc(bytes_per_cluster);
    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fread(buf, 1, bytes_per_cluster, img);

    int status = 1;  // assume not found
    for (uint32_t off = 0; off < bytes_per_cluster; off += 32) {
        uint8_t first = buf[off];
        if (first == 0x00) break;
        if (first == 0xE5) continue;
        uint8_t attr = buf[off + 11];
        if ((attr & 0x0F) == 0x0F) continue;
        if (buf[off] == '.') continue;

        // build short name
        char name[9] = {0}, ext[4] = {0}, full[13];
        memcpy(name, &buf[off], 8);
        memcpy(ext,  &buf[off+8],3);
        for (int i = 7; i>=0 && name[i]==' '; i--) name[i] = '\0';
        for (int i = 2;i>=0 && ext[i]==' ';  i--) ext[i]  = '\0';
        if (ext[0]) snprintf(full, sizeof(full), "%s.%s", name, ext);
        else        snprintf(full, sizeof(full), "%s", name);

        if (strcmp(full, target) == 0) {
            if (!(attr & 0x10)) {  // ATTR_DIRECTORY = 0x10 :contentReference[oaicite:2]{index=2}&#8203;:contentReference[oaicite:3]{index=3}
                status = 2;        // found but not dir
                break;
            }
            // fetch its first cluster
            uint16_t hi = *(uint16_t *)&buf[off + 20];
            uint16_t lo = *(uint16_t *)&buf[off + 26];
            *current_cluster = ((uint32_t)hi << 16) | lo;
            status = 0;          // success
            break;
        }
    }
    free(buf);
    return status;
}



int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [FAT32 IMAGE]\n", argv[0]);
        return 1;
    }
    FILE *image = fopen(argv[1], "rb");
    if (!image) {
        perror("Error opening image");
        return 1;
    }

    FAT32 fat32;
    if (read_boot_sector(image, &fat32)) {
        fclose(image);
        return 1;
    }

    uint32_t current_cluster = fat32.root_cluster;
    while (1) {
        printf("[%s]/> ", argv[1]);
        char *input = get_input();
        tokenlist *tokens = get_tokens(input);

        if (tokens->size > 0) {
            if (strcmp(tokens->items[0], "info") == 0) {
                print_fat32_info(&fat32);
            }
            else if (strcmp(tokens->items[0], "ls") == 0) {
                list_directory(image, &fat32, current_cluster);
            }
            else if (strcmp(tokens->items[0], "cd") == 0) {
                int rc = change_directory(image, &fat32, &current_cluster, tokens);
                if      (rc == 1) printf("Directory not found: %s\n", tokens->items[1]);
                else if (rc == 2) printf("Not a directory:    %s\n", tokens->items[1]);
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
