#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    
    char player_name[50];
    printf("Enter your name: ");
    if (fgets(player_name, sizeof(player_name), stdin)!=NULL) {
        player_name[strcspn(player_name, "\n")]='\0';
    }

    return 0;
}