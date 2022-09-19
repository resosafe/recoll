#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include "closefrom.h"

int main(int argc, char **argv)
{
    int i;

    int fd0 = open("/etc/group", 0);
    if (fd0 < 0) {
        perror("open /etc/group");
        exit(1);
    }
    
    if (dup2(fd0, 11) < 0) {
        perror("dup2->11");
        exit(1);
    }
    if (dup2(fd0, 19) < 0) {
        perror("dup2->19");
        exit(1);
    }
    if (dup2(fd0, 99)< 0) {
        perror("dup2->99 (ok)");
    }
    if (dup2(fd0, 999) < 0) {
        perror("dup3->999 (ok)");
    }

    libclf_closefrom(11);
    for (i = 0; i < 10000; i++) {
        if (fcntl(i, F_GETFL) != -1) {
            fprintf(stderr, "Descriptor %d is still open", i);
            if (i < 11)
                fprintf(stderr, " (OK)\n");
            else
                fprintf(stderr, " (BAD)\n");
        }
    }
    exit(0);
}
