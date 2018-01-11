#include <stdio.h>
#include <unistd.h>

int
main() {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        printf("error getcwd\n");
        return 0;
    }
    printf("%s",buffer);
    return 0;
}
