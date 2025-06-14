#include "fat32.h"
#include "lexer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define EOC_MARK       0x0FFFFFFF

#define MAX_OPEN_FILES 128

typedef struct {
    char name[13];         // 8.3 name (null-terminated)
    uint32_t cluster;      // starting cluster
    uint32_t size;         // file size
    char mode[4];          // "-r", "-w", etc
    uint32_t offset;       // file pointer offset
    char path[1024];       // full path
    int in_use;            // 1 = open, 0 = closed
} OpenFile;

OpenFile open_files[MAX_OPEN_FILES] = {0};
void cmd_write(FILE *img, FAT32 *fs, uint32_t cur, tokenlist *tok);
void set_fat_entry(FILE *img, FAT32 *fs, uint32_t cluster, uint32_t value);

// Compute the first sector of a data cluster
static uint32_t first_sector_of(FAT32 *fs, uint32_t cluster) {
    uint32_t first_data = fs->reserved_sector_count
                          + (fs->num_fats * fs->fat_size);
    return ((cluster - 2) * fs->sectors_per_cluster)
           + first_data;
}

// List directory contents
void list_directory(FILE *img, FAT32 *fs, uint32_t cluster) {
    printf(".\n..\n");
    uint32_t sector = first_sector_of(fs, cluster);
    uint32_t bpc    = fs->bytes_per_sector * fs->sectors_per_cluster;
    uint8_t *buf    = malloc(bpc);
    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fread(buf,1,bpc,img);
    for (uint32_t off=0; off<bpc; off+=32) {
        uint8_t first = buf[off];
        if (first==0x00) break;
        if (first==0xE5) continue;
        uint8_t attr = buf[off+11];
        if ((attr&0x0F)==0x0F) continue;
        if (buf[off]=='.') continue;
        char name[9]={0}, ext[4]={0}, full[13];
        memcpy(name, &buf[off], 8);
        memcpy(ext,  &buf[off+8],3);
        for(int i=7;i>=0&&name[i]==' ';i--) name[i]='\0';
        for(int i=2;i>=0&&ext[i]==' '; i--) ext[i]='\0';
        if (ext[0]) snprintf(full,sizeof(full),"%s.%s",name,ext);
        else         snprintf(full,sizeof(full),"%s",name);
        printf("%s\n", full);
    }
    free(buf);
}

// Change directory, update cwd string
int change_directory(FILE *img, FAT32 *fs, uint32_t *cur, uint32_t *parent, tokenlist *tok, char *cwd) {
    if (tok->size < 2) {
        printf("Usage: cd <dir>\n");
        return 1;
    }

    const char *t = tok->items[1];
    if (!strcmp(t, ".")) return 0;

    if (!strcmp(t, "..")) {
        if (*cur == fs->root_cluster) {
            cwd[0] = '\0';
            return 0;
        }

        // Try to find the ".." entry to get parent cluster
        uint32_t sector = first_sector_of(fs, *cur);
        uint32_t bpc = fs->bytes_per_sector * fs->sectors_per_cluster;
        uint8_t *buf = malloc(bpc);
        fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
        fread(buf, 1, bpc, img);

        uint32_t parent = fs->root_cluster;
        for (uint32_t off = 0; off < bpc; off += 32) {
            if (buf[off] == 0x00 || buf[off] == 0xE5) continue;
            if (strncmp((char *)&buf[off], "..        ", 11) == 0) {
                uint16_t hi = *(uint16_t *)&buf[off + 20];
                uint16_t lo = *(uint16_t *)&buf[off + 26];
                parent = ((uint32_t)hi << 16) | lo;
                break;
            }
        }
        free(buf);

        *cur = parent;

        // Trim the last "/dir" from cwd
        char *last = strrchr(cwd, '/');
        if (last) {
            *last = '\0';
            if (strlen(cwd) == 0)
                cwd[0] = '\0';
        }

        return 0;
    }

    // Normal directory change (not "..")
    uint32_t sector = first_sector_of(fs, *cur);
    uint32_t bpc = fs->bytes_per_sector * fs->sectors_per_cluster;
    uint8_t *buf = malloc(bpc);
    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fread(buf, 1, bpc, img);

    int status = 1;
    for (uint32_t off = 0; off < bpc; off += 32) {
        uint8_t f = buf[off];
        if (f == 0x00) break;
        if (f == 0xE5) continue;
        uint8_t a = buf[off + 11];
        if ((a & 0x0F) == 0x0F) continue;
        if (buf[off] == '.') continue;

        char name[9] = {0}, ext[4] = {0}, full[13];
        memcpy(name, &buf[off], 8);
        memcpy(ext, &buf[off + 8], 3);
        for (int i = 7; i >= 0 && name[i] == ' '; i--) name[i] = '\0';
        for (int i = 2; i >= 0 && ext[i] == ' '; i--) ext[i] = '\0';
        if (ext[0]) snprintf(full, sizeof(full), "%s.%s", name, ext);
        else        snprintf(full, sizeof(full), "%s", name);

        if (!strcmp(full, t)) {
            if (!(a & ATTR_DIRECTORY)) {
                status = 2;
                break;
            }
            uint16_t hi = *(uint16_t *)&buf[off + 20];
            uint16_t lo = *(uint16_t *)&buf[off + 26];
            *parent = *cur;
            *cur = ((uint32_t)hi << 16) | lo;
            status = 0;

            char tmp[1024];
            if (!cwd[0]) snprintf(tmp, sizeof(tmp), "/%s", t);
            else         snprintf(tmp, sizeof(tmp), "%s/%s", cwd, t);
            strcpy(cwd, tmp);
            break;
        }
    }

    free(buf);
    return status;
}


// Format to 8.3 uppercase, space-padded
static void format_short_name(const char *in, char out[11]) {
    const char *dot=strchr(in,'.'); int len=dot?(dot-in):strlen(in);
    for(int i=0;i<8;i++) out[i]= i<len ? toupper((unsigned char)in[i]) : ' ';
    if(dot){ const char*e=dot+1; int el=strlen(e);
        for(int i=0;i<3;i++) out[8+i]= i<el? toupper((unsigned char)e[i]) : ' ';
    } else for(int i=8;i<11;i++) out[i]=' ';
}

// Check for existing entry
static int entry_exists(FILE *img, FAT32 *fs, uint32_t cluster, char sn[11]) {
    uint32_t sector=first_sector_of(fs,cluster), bpc=fs->bytes_per_sector*fs->sectors_per_cluster;
    uint8_t *buf=malloc(bpc);
    fseek(img,sector*fs->bytes_per_sector,SEEK_SET); fread(buf,1,bpc,img);
    for(uint32_t off=0;off<bpc;off+=32){ uint8_t f=buf[off]; if(f==0x00) break; if(f==0xE5) continue;
        uint8_t a=buf[off+11]; if((a&0x0F)==0x0F) continue;
        if(!memcmp(buf+off,sn,11)){ free(buf); return 1; }
    }
    free(buf); return 0;
}

// Allocate one free cluster in both FAT copies
static uint32_t allocate_cluster(FILE*img, FAT32*fs) {
    uint32_t fat_start=fs->reserved_sector_count*fs->bytes_per_sector;
    uint32_t fat_bytes=fs->fat_size*fs->bytes_per_sector;
    uint8_t *fat=malloc(fat_bytes);
    fseek(img,fat_start,SEEK_SET); fread(fat,1,fat_bytes,img);
    uint32_t total=fat_bytes/4, newc=0;
    for(uint32_t i=2;i<total;i++){
        uint32_t e=(*(uint32_t*)(fat+i*4))&0x0FFFFFFF;
        if(e==0){ *(uint32_t*)(fat+i*4)=EOC_MARK; newc=i; break; }
    }
    if(!newc){ free(fat); return 0; }
    // write FAT1
    fseek(img,fat_start,SEEK_SET); fwrite(fat,1,fat_bytes,img);
    // write FAT2
    fseek(img,fat_start+fat_bytes,SEEK_SET); fwrite(fat,1,fat_bytes,img);
    free(fat);
    return newc;
}

// Add entry into directory cluster
static int add_directory_entry(FILE*img, FAT32*fs, uint32_t cluster,
                               char sn[11], uint8_t attr,
                               uint32_t fc, uint32_t sz) {
    uint32_t sector=first_sector_of(fs,cluster), bpc=fs->bytes_per_sector*fs->sectors_per_cluster;
    uint8_t *buf=malloc(bpc);
    fseek(img,sector*fs->bytes_per_sector,SEEK_SET); fread(buf,1,bpc,img);
    int ok=0;
    for(uint32_t off=0;off<bpc;off+=32){ uint8_t f=buf[off]; if(f==0x00||f==0xE5){
            memcpy(buf+off,sn,11); buf[off+11]=attr;
            memset(buf+off+12,0,14);
            *(uint16_t*)(buf+off+20) = (fc>>16)&0xFFFF;
            *(uint16_t*)(buf+off+26) = fc&0xFFFF;
            *(uint32_t*)(buf+off+28) = sz;
            ok=1; break;
        }
    }
    if(ok){ fseek(img,sector*fs->bytes_per_sector,SEEK_SET); fwrite(buf,1,bpc,img);}
    free(buf);
    return ok?0:-1;
}
int find_entry(FILE *img, FAT32 *fs, uint32_t cluster, const char *target, char shortname[11],
    uint32_t *start_cluster, uint32_t *size, uint8_t *attr_out) {
    uint32_t sector = first_sector_of(fs, cluster);
    uint32_t bpc = fs->bytes_per_sector * fs->sectors_per_cluster;
    uint8_t *buf = malloc(bpc);
    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fread(buf, 1, bpc, img);

    for (uint32_t off = 0; off < bpc; off += 32) {
    if (buf[off] == 0x00 || buf[off] == 0xE5) continue;
    if ((buf[off + 11] & 0x0F) == 0x0F) continue;

    char name[9] = {0}, ext[4] = {0}, full[13];
    memcpy(name, &buf[off], 8);
    memcpy(ext, &buf[off + 8], 3);
    for (int i = 7; i >= 0 && name[i] == ' '; i--) name[i] = '\0';
    for (int i = 2; i >= 0 && ext[i] == ' '; i--) ext[i] = '\0';
    if (ext[0])
    snprintf(full, sizeof(full), "%s.%s", name, ext);
    else
    snprintf(full, sizeof(full), "%s", name);

    if (strcmp(full, target) == 0) {
    memcpy(shortname, &buf[off], 11);
    uint16_t hi = *(uint16_t *)&buf[off + 20];
    uint16_t lo = *(uint16_t *)&buf[off + 26];
    *start_cluster = ((uint32_t)hi << 16) | lo;
    *size = *(uint32_t *)&buf[off + 28];
    if (attr_out) *attr_out = buf[off + 11];
    free(buf);
    return 0;  // found
    }
    }

    free(buf);
    return -1;
}

// mkdir implementation
void cmd_mkdir(FILE*img, FAT32*fs, uint32_t*cur, tokenlist*tok){
    if (tok->size < 2) {
        printf("Usage: mkdir <dir>\n");
        return;
    }

    char sn[11];
    format_short_name(tok->items[1], sn);

    if (entry_exists(img, fs, *cur, sn)) {
        printf("Error: '%s' exists\n", tok->items[1]);
        return;
    }

    uint32_t nc = allocate_cluster(img, fs);
    if (!nc) {
        printf("Error: no free cluster\n");
        return;
    }

    // zero out new cluster
    uint32_t s0 = first_sector_of(fs, nc);
    uint32_t bpc = fs->bytes_per_sector * fs->sectors_per_cluster;
    uint8_t *z = calloc(1, bpc);
    fseek(img, s0 * fs->bytes_per_sector, SEEK_SET);
    fwrite(z, 1, bpc, img);
    free(z);

    // Properly formatted "." and ".." entries
    char dot[11]  = { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
    char dot2[11] = { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
    dot[0] = '.';
    dot2[0] = '.';
    dot2[1] = '.';

    add_directory_entry(img, fs, nc, dot,  ATTR_DIRECTORY, nc,    0);  // .
    add_directory_entry(img, fs, nc, dot2, ATTR_DIRECTORY, *cur, 0);  // ..

    // Add new dir entry to parent
    if (add_directory_entry(img, fs, *cur, sn, ATTR_DIRECTORY, nc, 0) != 0)
        printf("Error: cannot link dir\n");
}


// creat implementation
void cmd_creat(FILE*img, FAT32*fs, uint32_t*cur, tokenlist*tok){
    if(tok->size<2){ printf("Usage: creat <file>\n"); return; }
    char sn[11]; format_short_name(tok->items[1],sn);
    if(entry_exists(img,fs,*cur,sn)){ printf("Error: '%s' exists\n",tok->items[1]); return; }
    if(add_directory_entry(img,fs,*cur,sn,ATTR_ARCHIVE,0,0)!=0)
        printf("Error: cannot create file\n");
}

void cmd_open(FILE *img, FAT32 *fs, uint32_t cur, tokenlist *tok, const char *cwd) {
    if (tok->size < 3) {
        printf("Usage: open <filename> <-r|-w|-rw|-wr>\n");
        return;
    }

    const char *fname = tok->items[1];
    const char *flag = tok->items[2];

    if (!(strcmp(flag, "-r") == 0 || strcmp(flag, "-w") == 0 ||
          strcmp(flag, "-rw") == 0 || strcmp(flag, "-wr") == 0)) {
        printf("Error: invalid open mode '%s'\n", flag);
        return;
    }

    // Check if already open
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].in_use && strcmp(open_files[i].name, fname) == 0) {
            printf("Error: file '%s' already opened\n", fname);
            return;
        }
    }

    char shortname[11];
    uint32_t cluster, size;
    uint8_t attr;
    if (find_entry(img, fs, cur, fname, shortname, &cluster, &size, &attr) != 0) {
        printf("Error: file '%s' not found\n", fname);
        return;
    }

    if (attr & ATTR_DIRECTORY) {
        printf("Error: '%s' is a directory\n", fname);
        return;
    }

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_files[i].in_use) {
            strncpy(open_files[i].name, fname, sizeof(open_files[i].name));
            open_files[i].cluster = cluster;
            open_files[i].size = size;
            strncpy(open_files[i].mode, flag + 1, sizeof(open_files[i].mode)); // skip '-'
            open_files[i].offset = 0;
            snprintf(open_files[i].path, sizeof(open_files[i].path), "%s/%s", cwd[0] ? cwd : "", fname);
            open_files[i].in_use = 1;
            printf("Opened '%s' with mode %s (offset = 0)\n", fname, flag);
            return;
        }
    }

    printf("Error: too many open files\n");
}

void cmd_close(tokenlist *tok) {
    if (tok->size < 2) {
        printf("Usage: close <filename>\n");
        return;
    }

    const char *fname = tok->items[1];

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].in_use && strcmp(open_files[i].name, fname) == 0) {
            open_files[i].in_use = 0;
            printf("Closed '%s'\n", fname);
            return;
        }
    }

    printf("Error: file '%s' is not opened\n", fname);
}

void cmd_lsof() {
    int found = 0;
    printf("%-5s %-13s %-5s %-8s %s\n", "Index", "Name", "Mode", "Offset", "Path");
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].in_use) {
            printf("%-5d %-13s %-5s %-8u %s\n",
                i,
                open_files[i].name,
                open_files[i].mode,
                open_files[i].offset,
                open_files[i].path);
            found = 1;
        }
    }

    if (!found)
        printf("No files are currently open.\n");
}

void cmd_lseek(tokenlist *tok) {
    if (tok->size < 3) {
        printf("Usage: lseek <filename> <offset>\n");
        return;
    }

    const char *fname = tok->items[1];
    int offset = atoi(tok->items[2]);

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].in_use && strcmp(open_files[i].name, fname) == 0) {
            if (offset < 0 || (uint32_t)offset > open_files[i].size) {
                printf("Error: offset %d is out of bounds (file size = %u bytes)\n", offset, open_files[i].size);
                return;
            }

            open_files[i].offset = offset;
            printf("Set offset of '%s' to %d\n", fname, offset);
            return;
        }
    }

    printf("Error: file '%s' is not open\n", fname);
}

uint32_t get_next_cluster(FILE *img, FAT32 *fs, uint32_t cluster) {
    uint32_t fat_offset = fs->reserved_sector_count * fs->bytes_per_sector + cluster * 4;
    fseek(img, fat_offset, SEEK_SET);
    uint32_t entry;
    fread(&entry, 4, 1, img);
    return entry & 0x0FFFFFFF;
}

void cmd_read(FILE *img, FAT32 *fs, uint32_t cur, tokenlist *tok){
    if (tok->size < 3) {
        printf("Usage: read <filename> <size>\n");
        return;
    }

    const char *fname = tok->items[1];
    int req_size = atoi(tok->items[2]);
    if (req_size <= 0) {
        printf("Error: invalid read size\n");
        return;
    }

    // Step 1: Check if file exists and is not a directory
    char shortname[11];
    uint32_t cluster = 0, size = 0;
    uint8_t attr = 0;
    if (find_entry(img, fs, cur, fname, shortname, &cluster, &size, &attr) != 0) {
        printf("Error: file '%s' does not exist\n", fname);
        return;
    }

    if (attr & ATTR_DIRECTORY) {
        printf("Error: '%s' is a directory\n", fname);
        return;
    }

    // Step 2: Check if file is open and open for reading
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].in_use && strcmp(open_files[i].name, fname) == 0) {
            if (!strchr(open_files[i].mode, 'r')) {
                printf("Error: file '%s' is not opened for reading\n", fname);
                return;
            }

            uint32_t offset = open_files[i].offset;
            uint32_t file_size = open_files[i].size;

            if (offset >= file_size) {
                printf("EOF reached\n");
                return;
            }

            uint32_t to_read = (offset + req_size > file_size) ? (file_size - offset) : req_size;
            uint32_t cluster_size = fs->bytes_per_sector * fs->sectors_per_cluster;
            uint32_t curr_cluster = open_files[i].cluster;
            uint32_t bytes_left = to_read;
            uint32_t skip = offset;

            while (skip >= cluster_size) {
                curr_cluster = get_next_cluster(img, fs, curr_cluster);
                if (curr_cluster >= 0x0FFFFFF8) {
                    printf("Error: offset beyond file clusters\n");
                    return;
                }
                skip -= cluster_size;
            }

            char *buf = malloc(to_read + 1);
            uint32_t buf_pos = 0;

            while (bytes_left > 0 && curr_cluster < 0x0FFFFFF8) {
                uint32_t sector = first_sector_of(fs, curr_cluster);
                uint8_t *cluster_buf = malloc(cluster_size);
                fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
                fread(cluster_buf, 1, cluster_size, img);

                uint32_t start = skip;
                uint32_t available = cluster_size - start;
                uint32_t copy_bytes = (bytes_left < available) ? bytes_left : available;

                memcpy(buf + buf_pos, cluster_buf + start, copy_bytes);
                buf_pos += copy_bytes;
                bytes_left -= copy_bytes;
                skip = 0;

                free(cluster_buf);

                if (bytes_left > 0)
                    curr_cluster = get_next_cluster(img, fs, curr_cluster);
            }

            buf[to_read] = '\0';
            fwrite(buf, 1, to_read, stdout);
            printf("\n");

            open_files[i].offset += to_read;
            free(buf);
            return;
        }
    }

    // File exists, but wasn't found in open table
    printf("Error: file '%s' is not open\n", fname);
}

void cmd_write(FILE *img, FAT32 *fs, uint32_t cur, tokenlist *tok) {
    if (tok->size < 3) {
        printf("Usage: write <filename> \"<string>\"\n");
        return;
    }

    const char *fname = tok->items[1];
    char *data = tok->items[2];
    if (data[0] == '"') data++;
    size_t len = strlen(data);
    if (data[len - 1] == '"') data[len - 1] = '\0';
    len = strlen(data);

    char shortname[11];
    uint32_t cluster = 0, size = 0;
    uint8_t attr = 0;
    if (find_entry(img, fs, cur, fname, shortname, &cluster, &size, &attr) != 0) {
        printf("Error: file '%s' does not exist\n", fname);
        return;
    }
    if (attr & ATTR_DIRECTORY) {
        printf("Error: '%s' is a directory\n", fname);
        return;
    }

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].in_use && strcmp(open_files[i].name, fname) == 0) {
            if (!strchr(open_files[i].mode, 'w')) {
                printf("Error: file '%s' not opened for writing\n", fname);
                return;
            }

            uint32_t offset = open_files[i].offset;
            uint32_t needed = offset + len;
            uint32_t cluster_size = fs->bytes_per_sector * fs->sectors_per_cluster;

            uint32_t num_clusters = (needed + cluster_size - 1) / cluster_size;
            uint32_t curr = open_files[i].cluster;
            uint32_t chain_len = 1;
            uint32_t tail = curr;

            while (get_next_cluster(img, fs, tail) < 0x0FFFFFF8) {
                tail = get_next_cluster(img, fs, tail);
                chain_len++;
            }

            uint32_t extra = num_clusters > chain_len ? num_clusters - chain_len : 0;
            while (extra > 0) {
                uint32_t new_cluster = allocate_cluster(img, fs);
                set_fat_entry(img, fs, tail, new_cluster);
                set_fat_entry(img, fs, new_cluster, EOC_MARK);
                tail = new_cluster;
                extra--;
            }

            curr = open_files[i].cluster;
            uint32_t skip = offset;
            while (skip >= cluster_size) {
                curr = get_next_cluster(img, fs, curr);
                skip -= cluster_size;
            }

            size_t written = 0;
            while (written < len && curr < 0x0FFFFFF8) {
                uint32_t sec = first_sector_of(fs, curr);
                uint8_t *buf = malloc(cluster_size);
                fseek(img, sec * fs->bytes_per_sector, SEEK_SET);
                fread(buf, 1, cluster_size, img);

                size_t start = skip;
                size_t space = cluster_size - start;
                size_t to_copy = len - written < space ? len - written : space;

                memcpy(buf + start, data + written, to_copy);
                fseek(img, sec * fs->bytes_per_sector, SEEK_SET);
                fwrite(buf, 1, cluster_size, img);
                free(buf);

                written += to_copy;
                skip = 0;
                if (written < len)
                    curr = get_next_cluster(img, fs, curr);
            }

            open_files[i].offset += written;
            if (open_files[i].offset > open_files[i].size)
                open_files[i].size = open_files[i].offset;

            // Update file size in directory entry
            uint32_t dir_sector = first_sector_of(fs, cur);
            uint32_t bpc = fs->bytes_per_sector * fs->sectors_per_cluster;
            uint8_t *dir_buf = malloc(bpc);
            fseek(img, dir_sector * fs->bytes_per_sector, SEEK_SET);
            fread(dir_buf, 1, bpc, img);

            for (uint32_t off = 0; off < bpc; off += 32) {
                if (memcmp(dir_buf + off, shortname, 11) == 0) {
                    *(uint32_t *)(dir_buf + off + 28) = open_files[i].size;
                    break;
                }
            }

            fseek(img, dir_sector * fs->bytes_per_sector, SEEK_SET);
            fwrite(dir_buf, 1, bpc, img);
            free(dir_buf);

            printf("Wrote %zu bytes to '%s'\n", written, fname);
            return;
        }
    }

    printf("Error: file '%s' is not open\n", fname);
}

void set_fat_entry(FILE *img, FAT32 *fs, uint32_t cluster, uint32_t value) {
    for (int i = 0; i < fs->num_fats; i++) {
        uint32_t fat_offset = (fs->reserved_sector_count + i * fs->fat_size) * fs->bytes_per_sector;
        fseek(img, fat_offset + cluster * 4, SEEK_SET);
        value &= 0x0FFFFFFF;
        fwrite(&value, 4, 1, img);
    }
}

void remove_directory_entry(FILE *img, FAT32 *fs, uint32_t cluster, char shortname[11]) {
    uint32_t sector = first_sector_of(fs, cluster);
    uint32_t bpc = fs->bytes_per_sector * fs->sectors_per_cluster;
    uint8_t *buf = malloc(bpc);
    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fread(buf, 1, bpc, img);

    for (uint32_t off = 0; off < bpc; off += 32) {
        if (memcmp(&buf[off], shortname, 11) == 0) {
            buf[off] = 0xE5;  // mark as deleted
            break;
        }
    }

    fseek(img, sector * fs->bytes_per_sector, SEEK_SET);
    fwrite(buf, 1, bpc, img);
    free(buf);
}

void cmd_mv(FILE *img, FAT32 *fs, uint32_t *cur, uint32_t parent_cluster, tokenlist *tok) {
    if (tok->size < 3) {
        printf("Usage: mv <source> <dest>\n");
        return;
    }

    const char *src = tok->items[1];
    const char *dest = tok->items[2];

    if (!strcmp(dest, ".") || !strcmp(dest, "..")) {
        if (!strcmp(dest, ".")) {
            printf("Error: cannot rename to '.'\n");
            return;
        }
        if (!strcmp(src, "..") || !strcmp(src, ".")) {
            printf("Error: cannot move '.' or '..'\n");
            return;
        }

        char shortname[11];
        uint32_t src_cluster = 0, src_size = 0;
        uint8_t src_attr = 0;
        if (find_entry(img, fs, *cur, src, shortname, &src_cluster, &src_size, &src_attr) != 0) {
            printf("Error: source '%s' does not exist\n", src);
            return;
        }

        if (parent_cluster == 0 || parent_cluster == *cur) {
            printf("Error: could not locate parent directory\n");
            return;
        }

        if (add_directory_entry(img, fs, parent_cluster, shortname, src_attr, src_cluster, src_size) != 0) {
            printf("Error: could not move to parent\n");
            return;
        }

        remove_directory_entry(img, fs, *cur, shortname);
        printf("Moved '%s' to parent directory\n", src);
        return;
    }

    char dest_short[11];
    format_short_name(dest, dest_short);

    // Check source
    char src_short[11];
    uint32_t src_cluster, src_size;
    uint8_t src_attr;
    if (find_entry(img, fs, *cur, src, src_short, &src_cluster, &src_size, &src_attr) != 0) {
        printf("Error: source '%s' does not exist\n", src);
        return;
    }

    // Check destination
    char dummy_short[11];
    uint32_t dest_cluster, dummy_size;
    uint8_t dest_attr;
    int dest_exists = find_entry(img, fs, *cur, dest, dummy_short, &dest_cluster, &dummy_size, &dest_attr) == 0;

    if (dest_exists) {
        if (!(dest_attr & ATTR_DIRECTORY)) {
            printf("Error: destination '%s' is a file\n", dest);
            return;
        }

        if (add_directory_entry(img, fs, dest_cluster, src_short, src_attr, src_cluster, src_size) != 0) {
            printf("Error: could not move into directory\n");
            return;
        }

        remove_directory_entry(img, fs, *cur, src_short);
        printf("Moved '%s' to directory '%s'\n", src, dest);
        return;
    }

    // Rename in place
    if (add_directory_entry(img, fs, *cur, dest_short, src_attr, src_cluster, src_size) != 0) {
        printf("Error: could not rename\n");
        return;
    }
    remove_directory_entry(img, fs, *cur, src_short);
    printf("Renamed '%s' to '%s'\n", src, dest);
}


int main(int argc, char*argv[]){
    if(argc!=2){ fprintf(stderr,"Usage: %s [IMG]\n",argv[0]); return 1; }
    FILE*img=fopen(argv[1],"rb+"); if(!img){ perror("open"); return 1; }
    FAT32 fs; if(read_boot_sector(img,&fs)) return 1;
    uint32_t cur=fs.root_cluster; char cwd[1024]="";
    uint32_t parent_cluster = fs.root_cluster;
    while(1){ printf("%s%s> ",argv[1],cwd);
        char*in=get_input(); tokenlist*tl=get_tokens(in);
        if(tl->size){
            if(!strcmp(tl->items[0],"ls"))   list_directory(img,&fs,cur);
            else if(!strcmp(tl->items[0],"cd")){
                int r = change_directory(img, &fs, &cur, &parent_cluster, tl, cwd);
                if(r==1) printf("Directory not found\n");
                else if(r==2) printf("Not a directory\n");
            }
            else if(!strcmp(tl->items[0],"mkdir")) cmd_mkdir(img,&fs,&cur,tl);
            else if(!strcmp(tl->items[0],"creat")) cmd_creat(img,&fs,&cur,tl);
            else if(!strcmp(tl->items[0],"exit")) break;
            else if(!strcmp(tl->items[0],"info")) print_fat32_info(&fs);
            else if (!strcmp(tl->items[0], "open")) cmd_open(img, &fs, cur, tl, cwd);
            else if (!strcmp(tl->items[0], "close")) cmd_close(tl);
            else if (!strcmp(tl->items[0], "lsof")) cmd_lsof();
            else if (!strcmp(tl->items[0], "lseek")) cmd_lseek(tl);
            else if (!strcmp(tl->items[0], "read")) cmd_read(img, &fs, cur, tl);
            else if (!strcmp(tl->items[0], "write")) cmd_write(img, &fs, cur, tl);
            else if (!strcmp(tl->items[0], "mv")) cmd_mv(img, &fs, &cur,parent_cluster, tl);
            else printf("Unknown: %s\n",tl->items[0]);
        }
        free(in); free_tokens(tl);
    }
    fclose(img);
    return 0;
}
