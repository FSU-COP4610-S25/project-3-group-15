
hear me out ok
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fat32.h"
#include "lexer.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [FAT32 IMAGE FILE]\n", argv[0]);
        return 1;
    }

    FILE *image = fopen(argv[1], "rb");
    if (!image) {
        fprintf(stderr, "Error: Unable to open file %s\n", argv[1]);
        return 1;
    }

    FAT32 fat32;
    if (read_boot_sector(image, &fat32) != 0) {
        fclose(image);
        return 1;
    }

    while (1) {
        printf("[%s]/> ", argv[1]);

        char *input = get_input();
        tokenlist *tokens = get_tokens(input);

        if (tokens->size > 0) {
            if (strcmp(tokens->items[0], "info") == 0) {
                print_fat32_info(&fat32);
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




/*

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [FAT32 IMAGE FILE]\n", argv[0]);
        return 1;
    }

    image = fopen(argv[1], "rb");
    if (!image) {
        perror("Error opening image");
        return 1;
    }

    while(1) {
        if (command == "exit")
            break;
        else if (command == "info") {
            //implement info here 
        }
    }
  
    fclose(image);
    printf("Image unmounted.\n");
    return 0;
}
 */
// FIANAS ATTEMPT:
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include "fat32.h"
//#include "lexer.h"
//
//FILE *fp = NULL;
//BPB bpb;
//char imageName[256] = {0};
//
//int main(int argc, char *argv[]) {
//    if (argc != 2) {
//        fprintf(stderr, "Usage: ./filesys [FAT32 IMAGE FILE]\n");
//        return 1;
//    }
//
//    strncpy(imageName, argv[1], sizeof(imageName) - 1);
//    fp = fopen(imageName, "rb+");
//    if (!fp) {
//        perror("Error opening image file");
//        return 1;
//    }
//
//    if (read_bpb(fp, &bpb) != 0) {
//        fprintf(stderr, "Error reading BPB\n");
//        fclose(fp);
//        return 1;
//    }
//
//    while (1) {
//        printf("%s/> ", imageName);
//
//        char *input = get_input();
//        tokenlist *tokens = get_tokens(input);
//
//        if (tokens->size == 0) {
//            free(input);
//            free_tokens(tokens);
//            continue;
//        }
//
//        char *cmd = tokens->items[0];
//
//        if (strcmp(cmd, "exit") == 0) {
//            free(input);
//            free_tokens(tokens);
//            break;
//        } else if (strcmp(cmd, "info") == 0) {
//            print_info(&bpb);
//        } else {
//            printf("Unknown command: %s\n", cmd);
//        }
//
//        free(input);
//        free_tokens(tokens);
//    }
//
//    fclose(fp);
//    return 0;
//}



