#include <stdio.h>
#include <ctype.h>

#define BUFSIZE 1024

void wc(FILE *ofile, FILE *infile, char *inname) {
    int totalNewlines = 0;
    int totalWords = 0;
    int totalBytes = 0;
    int inword = 0;
    char buff[BUFSIZE];

    if (inname)
        infile = fopen(inname, "r");

    if (!infile)
        return;

    while(fgets(buff, BUFSIZE, infile)) {
        int i = 0;
        for (i = 0; i < BUFSIZE && buff[i]; i++, totalBytes++) {
            if (isspace(buff[i])) {
                if (inword) {
                    inword = 0;
                    totalWords++;
                }
                if (buff[i] == '\n') {
                    totalNewlines++;
                }
            }
            else {
                inword = 1;
            }
        }
    }

    printf("%d %d %d %s\n", totalNewlines, totalWords, totalBytes, inname);
    fclose(infile);
    fclose(ofile);
}

int main (int argc, char *argv[]) {
    if (argc > 1) {
        int i = 1;
	for (i = 1; i < argc; i++) {
       	    wc(stdout, NULL, argv[i]);
        }
    }
    else {
	wc(stdout, stdin, NULL);
    }
    return 0;
}
