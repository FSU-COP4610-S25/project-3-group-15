#include "fat32.h"
#include "lexer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

// Compute the first sector of a given data cluster
static uint32_t first_sector_of(FAT32 *fs, uint32_t cluster) {
    uint32_t first_data = fs->reserved_sector_count
                          + (fs->num_fats * fs->fat_size);
    return ((cluster - 2) * fs->sectors_per_cluster)
           + first_data;
}

// ls: list ".", "..", then each real entry in the current cluster
void list_directory(FILE *img, FAT32 *fs, uint32_t cluster) {
    printf(".\n..\n");  // always include these
    uint32_t sector = first_sector_of(fs, cluster);
    uint32_t bpc = fs->bytes_per_sector * fs->sectors_per_cluster;
    uint8_t *buf = malloc(bpc);
    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fread(buf, 1, bpc, img);

    for (uint32_t off = 0; off < bpc; off += 32) {
        uint8_t first = buf[off];
        if      (first == 0x00) break;            // no more entries
        else if (first == 0xE5)  continue;         // deleted
        uint8_t attr = buf[off + 11];
        if ((attr & 0x0F) == 0x0F) continue;       // long‑name entry

        // skip the on‑disk "." and ".."
        //if (buf[off] == '.') continue;

        // build short name + optional extension
        char name[9] = {0}, ext[4] = {0}, full[13];
        memcpy(name, &buf[off],   8);
        memcpy(ext,  &buf[off+8], 3);
        // trim trailing spaces
        for (int i = 7; i>=0 && name[i]==' '; i--) name[i] = '\0';
        for (int i = 2; i>=0 && ext[i]==' ';  i--) ext[i]  = '\0';

        if (ext[0])
            snprintf(full, sizeof(full), "%s.%s", name, ext);
        else
            snprintf(full, sizeof(full), "%s", name);

        printf("[DEBUG] Found: raw='%.11s' attr=0x%02X -> final='%s'\n", &buf[off], attr, full);

	printf("%s\n", full);
    }
    free(buf);
}
// cd: update current_cluster and cwd
int change_directory(FILE *img, FAT32 *fs,
                     uint32_t *current_cluster,
                     tokenlist *tokens,
                     char *cwd)
{
    if (tokens->size < 2) {
        printf("Usage: cd <directory>\n");
        return 1;
    }
    const char *target = tokens->items[1];
    if (strcmp(target, ".") == 0)    return 0;
    if (strcmp(target, "..") == 0) {
        *current_cluster = fs->root_cluster;
        cwd[0] = '\0';             // back to root: empty cwd
        return 0;
    }

    // scan for entry in the current directory…
    uint32_t sector = first_sector_of(fs, *current_cluster);
    uint32_t bpc    = fs->bytes_per_sector * fs->sectors_per_cluster;
    uint8_t *buf    = malloc(bpc);
    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fread(buf, 1, bpc, img);

    int status = 1;
    for (uint32_t off = 0; off < bpc; off += 32) {
        uint8_t first = buf[off];
        if      (first == 0x00) break;
        else if (first == 0xE5)  continue;
        uint8_t attr = buf[off + 11];
        if ((attr & 0x0F) == 0x0F) continue;
        if (buf[off] == '.')     continue;

        // build short name + extension
        char name[9] = {0}, ext[4] = {0}, full[13];
        memcpy(name, &buf[off],   8);
        memcpy(ext,  &buf[off+8], 3);
        for (int i = 7; i>=0 && name[i]==' '; i--) name[i] = '\0';
        for (int i = 2; i>=0 && ext[i]==' ';  i--) ext[i]  = '\0';
        if (ext[0])
            snprintf(full, sizeof(full), "%s.%s", name, ext);
        else
            snprintf(full, sizeof(full), "%s", name);

        if (strcmp(full, target) == 0) {
            if (!(attr & 0x10)) {  // not a directory
                status = 2;
                break;
            }
            uint16_t hi = *(uint16_t *)&buf[off + 20];
            uint16_t lo = *(uint16_t *)&buf[off + 26];
            *current_cluster = ((uint32_t)hi << 16) | lo;
            status = 0;

            // update cwd: always prefix a single '/'
            char tmp[1024];
            if (cwd[0] == '\0')
                snprintf(tmp, sizeof(tmp), "/%s", target);
            else
                snprintf(tmp, sizeof(tmp), "%s/%s", cwd, target);
            strcpy(cwd, tmp);
            break;
        }
    }

    free(buf);
    return status;
}

void mkdir_entry(FILE *img, FAT32 *fs, uint32_t current_cluster, const char *name, int is_dir) {
    uint32_t sector = first_sector_of(fs, current_cluster);
    uint32_t bpc = fs->bytes_per_sector * fs->sectors_per_cluster;
    uint8_t *buf = malloc(bpc);

    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fread(buf, 1, bpc, img);

    // Convert name to 8.3 format
    char entry_name[11];
    memset(entry_name, ' ', 11); // space-fill all 11 characters
    
    // Copy up to 8 characters before '.'
    int i = 0, j = 0;
    while (name[i] && name[i] != '.' && j < 8) {
        entry_name[j++] = toupper(name[i++]);
    }

    // Copy up to 3 characters after '.'
    if (name[i] == '.') {
        i++; // skip the dot
        j = 8;
        int k = 0;
        while (name[i] && k < 3) {
            entry_name[j++] = toupper(name[i++]);
            k++;
        }
    }

    // Check if it already exists
    for (uint32_t off = 0; off < bpc; off += 32) {
        if (memcmp(&buf[off], entry_name, 11) == 0) {
            printf("Error: entry '%s' already exists\n", name);
            free(buf);
            return;
        }
    }

    // Find a free slot
    int found = 0;
    uint32_t offset = 0;
    for (; offset < bpc; offset += 32) {
        if (buf[offset] == 0x00 || buf[offset] == 0xE5) {
            found = 1;
            break;
        }
    }
    if (!found) {
        printf("Error: no space in current directory\n");
        free(buf);
        return;
    }

    // Write new entry
    memcpy(&buf[offset], entry_name, 11);
    buf[offset + 11] = is_dir ? 0x10 : 0x20; // attributes
    for (int i = 12; i < 32; i++) buf[offset + i] = 0;
    printf("[DEBUG] Wrote entry: name='%.11s', attr=0x%02X\n", entry_name, buf[offset + 11]);

    // No allocated cluster yet → set cluster fields to 0
    *(uint16_t *)&buf[offset + 20] = 0;
    *(uint16_t *)&buf[offset + 26] = 0;

    // File size = 0
    *(uint32_t *)&buf[offset + 28] = 0;

    // Write back
    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fwrite(buf, 1, bpc, img);
    fflush(img);

    printf("Created %s: %s\n", is_dir ? "directory" : "file", name);
    free(buf);
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
    char     cwd[1024]       = "";      // start “at root” with empty cwd

    while (1) {
        // prompt: e.g. "bin/fat32.img>" or "bin/fat32.img/BLUE>"
        printf("%s%s> ", argv[1], cwd);

        char      *input  = get_input();
        tokenlist *tokens = get_tokens(input);

        if (tokens->size > 0) {
            if (strcmp(tokens->items[0], "info") == 0) {
                print_fat32_info(&fat32);
            }
            else if (strcmp(tokens->items[0], "ls") == 0) {
                list_directory(image, &fat32, current_cluster);
            }
            else if (strcmp(tokens->items[0], "cd") == 0) {
                int rc = change_directory(image, &fat32,
                                          &current_cluster,
                                          tokens, cwd);
                if      (rc == 1) printf("Directory not found: %s\n", tokens->items[1]);
                else if (rc == 2) printf("Not a directory:    %s\n", tokens->items[1]);
            }
            else if (strcmp(tokens->items[0], "exit") == 0) {
                break;
            }
            else if (strcmp(tokens->items[0], "mkdir") == 0) {
                if (tokens->size < 2) {
                    printf("Usage: mkdir <dirname>\n");
                } else {
                    mkdir_entry(image, &fat32, current_cluster, tokens->items[1], 1);
                }
            }
            else if (strcmp(tokens->items[0], "creat") == 0) {
                if (tokens->size < 2) {
                    printf("Usage: creat <filename>\n");
                } else {
                    mkdir_entry(image, &fat32, current_cluster, tokens->items[1], 0);
                }
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
