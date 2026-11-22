#include "gomel.h"

static Class **classes = NULL;
static int nclasses = 0;

typedef struct Var { char *name; Type *type; } Var;
static Var *scope[64];
static int scopeTop = -1;
static int scopeN[64];

void sym_addClass(Class *c) {
    classes = (Class**)realloc(classes, sizeof(Class*) * (nclasses + 1));
    classes[nclasses++] = c;
}

Class *sym_findClass(const char *name) {
    if (!name) return NULL; /* Защита от вылета */
    for (int i = 0; i < nclasses; i++)
        if (!strcmp(classes[i]->name, name)) return classes[i];
    return NULL;
}

Field *sym_findField(Class *c, const char *name) {
    for (int i = 0; i < c->nfields; i++)
        if (!strcmp(c->fields[i].name, name)) return &c->fields[i];
    if (c->parent) {
        Class *p = sym_findClass(c->parent);
        if (p) return sym_findField(p, name);
    }
    return NULL;
}

Method *sym_findMethod(Class *c, const char *name) {
    for (int i = 0; i < c->nmethods; i++)
        if (!strcmp(c->methods[i].name, name)) return &c->methods[i];
    if (c->parent) {
        Class *p = sym_findClass(c->parent);
        if (p) return sym_findMethod(p, name);
    }
    return NULL;
}

void sym_pushScope(void) {
    scopeTop++;
    scope[scopeTop] = NULL;
    scopeN[scopeTop] = 0;
}

void sym_popScope(void) {
    scopeTop--;
}

void sym_addVar(const char *name, Type *t) {
    Var v;
    v.name = strdup(name);
    v.type = t;
    scope[scopeTop] = (Var*)realloc(scope[scopeTop], sizeof(Var) * (scopeN[scopeTop] + 1));
    scope[scopeTop][scopeN[scopeTop]++] = v;
}

Type *sym_findVar(const char *name) {
    for (int s = scopeTop; s >= 0; s--)
        for (int i = 0; i < scopeN[s]; i++)
            if (!strcmp(scope[s][i].name, name)) return scope[s][i].type;
    return NULL;
}

int sym_getFieldDepth(Class* c, const char* fieldName) {
    int depth = 0;
    while (c) {
        for (int i = 0; i < c->nfields; i++) {
            if (strcmp(c->fields[i].name, fieldName) == 0) return depth;
        }
        if (c->parent) {
            c = sym_findClass(c->parent);
            depth++;
        } else {
            break;
        }
    }
    return -1;
}

Class* sym_findMethodOwner(Class* c, const char* methodName) {
    while (c) {
        for (int i = 0; i < c->nmethods; i++) {
            if (strcmp(c->methods[i].name, methodName) == 0) return c;
        }
        if (c->parent) c = sym_findClass(c->parent);
        else break;
    }
    return NULL;
}
