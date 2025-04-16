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
int change_directory(FILE *img, FAT32 *fs, uint32_t *cur, tokenlist *tok, char *cwd) {
    if (tok->size<2) { printf("Usage: cd <dir>\n"); return 1; }
    const char *t=tok->items[1];
    if (!strcmp(t,".")) return 0;
    if (!strcmp(t,"..")) {
        *cur = fs->root_cluster;
        cwd[0]='\0';
        return 0;
    }
    uint32_t sector=first_sector_of(fs,*cur), bpc=fs->bytes_per_sector*fs->sectors_per_cluster;
    uint8_t *buf=malloc(bpc);
    fseek(img,sector*fs->bytes_per_sector,SEEK_SET); fread(buf,1,bpc,img);
    int status=1;
    for(uint32_t off=0;off<bpc;off+=32){
        uint8_t f=buf[off]; if(f==0x00) break; if(f==0xE5) continue;
        uint8_t a=buf[off+11]; if((a&0x0F)==0x0F) continue; if(buf[off]=='.') continue;
        char name[9]={0}, ext[4]={0}, full[13];
        memcpy(name,&buf[off],8); memcpy(ext,&buf[off+8],3);
        for(int i=7;i>=0&&name[i]==' ';i--) name[i]='\0';
        for(int i=2;i>=0&&ext[i]==' '; i--) ext[i]='\0';
        if(ext[0]) snprintf(full,sizeof(full),"%s.%s",name,ext);
        else       snprintf(full,sizeof(full),"%s",name);
        if(!strcmp(full,t)){
            if(!(a&ATTR_DIRECTORY)){ status=2; break;}
            uint16_t hi=*(uint16_t*)&buf[off+20], lo=*(uint16_t*)&buf[off+26];
            *cur = ((uint32_t)hi<<16)|lo; status=0;
            char tmp[1024];
            if (!cwd[0]) snprintf(tmp,sizeof(tmp),"/%s",t);
            else         snprintf(tmp,sizeof(tmp),"%s/%s",cwd,t);
            strcpy(cwd,tmp);
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

// mkdir implementation
void cmd_mkdir(FILE*img, FAT32*fs, uint32_t*cur, tokenlist*tok){
    if(tok->size<2){ printf("Usage: mkdir <dir>\n"); return; }
    char sn[11]; format_short_name(tok->items[1],sn);
    if(entry_exists(img,fs,*cur,sn)){ printf("Error: '%s' exists\n",tok->items[1]); return; }
    uint32_t nc=allocate_cluster(img,fs);
    if(!nc){ printf("Error: no free cluster\n"); return; }
    // zero new cluster
    uint32_t s0=first_sector_of(fs,nc), bpc=fs->bytes_per_sector*fs->sectors_per_cluster;
    uint8_t *z=calloc(1,bpc);
    fseek(img,s0*fs->bytes_per_sector,SEEK_SET); fwrite(z,1,bpc,img); free(z);
    // write '.' entry
    char dot[11]={' '}, dot2[11]={' '};
    dot[0]='.';   dot2[0]='.'; dot2[1]='.';
    add_directory_entry(img,fs,nc,dot,ATTR_DIRECTORY,nc,0);
    add_directory_entry(img,fs,nc,dot2,ATTR_DIRECTORY,*cur,0);
    // link into parent
    if(add_directory_entry(img,fs,*cur,sn,ATTR_DIRECTORY,nc,0)!=0)
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

int main(int argc, char*argv[]){
    if(argc!=2){ fprintf(stderr,"Usage: %s [IMG]\n",argv[0]); return 1; }
    FILE*img=fopen(argv[1],"rb+"); if(!img){ perror("open"); return 1; }
    FAT32 fs; if(read_boot_sector(img,&fs)) return 1;
    uint32_t cur=fs.root_cluster; char cwd[1024]="";
    while(1){ printf("%s%s> ",argv[1],cwd);
        char*in=get_input(); tokenlist*tl=get_tokens(in);
        if(tl->size){
            if(!strcmp(tl->items[0],"ls"))   list_directory(img,&fs,cur);
            else if(!strcmp(tl->items[0],"cd")){
                int r=change_directory(img,&fs,&cur,tl,cwd);
                if(r==1) printf("Directory not found\n");
                else if(r==2) printf("Not a directory\n");
            }
            else if(!strcmp(tl->items[0],"mkdir")) cmd_mkdir(img,&fs,&cur,tl);
            else if(!strcmp(tl->items[0],"creat")) cmd_creat(img,&fs,&cur,tl);
            else if(!strcmp(tl->items[0],"exit")) break;
            else if(!strcmp(tl->items[0],"info")) print_fat32_info(&fs);
            else printf("Unknown: %s\n",tl->items[0]);
        }
        free(in); free_tokens(tl);
    }
    fclose(img);
    return 0;
}