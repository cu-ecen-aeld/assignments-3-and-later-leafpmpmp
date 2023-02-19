#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);
    char *dir = argv[1];
    char *str = argv[2];
    FILE *fi;

    if(argc != 3){
        syslog(LOG_USER, "failed: Invalid Arguments\nusage: ./writer 'file being written (w/ full dir)' 'string to write'\n");
        printf("Failed.");
        exit(1);
    }
    syslog(LOG_DEBUG, "Writing %s to %s", str, dir);

    fi = fopen(dir, "w");
    fputs(str, fi);
    fclose(fi);
    return 0;
}
