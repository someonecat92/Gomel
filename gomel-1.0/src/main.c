#include "gomel.h"

void build_vtable(Program *prog) {
    int next_index = 0;
    for (int i = 0; i < prog->nclasses; i++) {
        Class *c = prog->classes[i];
        for (int m = 0; m < c->nmethods; m++) {
            Method *method = &c->methods[m];
            int found = 0;
            /* »щем, есть ли уже такой метод в родительских классах */
            Class *parent = sym_findClass(c->parent);
            while (parent) {
                for (int pm = 0; pm < parent->nmethods; pm++) {
                    if (strcmp(parent->methods[pm].name, method->name) == 0) {
                        method->vtable_index = parent->methods[pm].vtable_index;
                        found = 1;
                        break;
                    }
                }
                if (found) break;
                parent = sym_findClass(parent->parent);
            }
            if (!found) {
                method->vtable_index = next_index++;
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("»спользование: gomel input.gom output.c\n");
        return 1;
    }

    FILE *in = fopen(argv[1], "rb");
    if (!in) { perror("input"); return 1; }
    fseek(in, 0, SEEK_END);
    long sz = ftell(in);
    fseek(in, 0, SEEK_SET);
    char *src = (char*)malloc((size_t)sz + 1);
    fread(src, 1, (size_t)sz, in);
    src[sz] = 0;
    fclose(in);

    int ntok = 0;
    Token *toks = lex(src, &ntok);
    Program *prog = parse(toks, ntok);

    for (int i = 0; i < prog->nclasses; i++) sym_addClass(prog->classes[i]);
    build_vtable(prog); /* <--- ƒќЅј¬»“№ Ё“ќ */

    codegen(prog, argv[2]);
    printf("Gomel: сгенерирован %s (%d токенов, %d классов, %d функций)\n",
           argv[2], ntok, prog->nclasses, prog->nfuncs);
    return 0;
}
