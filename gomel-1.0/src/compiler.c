#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: gomelc <file.gom>\n");
        return 1;
    }

    char input[256];
    strcpy(input, argv[1]);

    char base[256];
    strcpy(base, input);

    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    char cmd[1024];

    printf("[1/3] Generating C code from %s...\n", input);
    sprintf(cmd, "gomel.exe %s %s.c", input, base);
    if (system(cmd) != 0) {
        printf("Error: Failed to generate C code.\n");
        return 1;
    }

    printf("[2/3] Compiling %s.c to %s.exe...\n", base, base);
    /* Используем пути и флаг -mconsole для надежности */
    sprintf(cmd, "\"C:\\MinGW\\bin\\gcc.exe\" -mconsole %s.c -o %s.exe", base, base);
    if (system(cmd) != 0) {
        printf("Error: gcc compilation failed.\n");
        return 1;
    }

    printf("[3/3] Success! Running %s.exe:\n", base);
    printf("------------------------\n");

    sprintf(cmd, "%s.exe", base);
    system(cmd);

    return 0;
}
