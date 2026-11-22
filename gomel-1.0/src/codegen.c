#include "gomel.h"

static FILE *out;

static const char *translate_type(const char *type) {
    if (strcmp(type, "number") == 0) return "int";
    if (strcmp(type, "act") == 0) return "void";
    return type;
}

static int isPtrExpr(Expr *e) {
    if (!e) return 0;
    if (e->kind == E_THIS) return 1;
    if (e->kind == E_IDENT) {
        Type *vt = sym_findVar(e->sval);
        return (vt && vt->isPointer);
    }
    return 0;
}

static void emitStmt(Stmt *s, const char *thisClass);

static void emitExpr(Expr *e, const char *thisClass) {
    if (!e) return;
    switch (e->kind) {
        case E_NUM: fprintf(out, "%d", e->ival); break;
        case E_STR: fprintf(out, "\"%s\"", e->sval); break;
        case E_THIS: fprintf(out, "this"); break;
        case E_IDENT: fprintf(out, "%s", e->sval); break;
        case E_NEW:
            fprintf(out, "(%s*)malloc(sizeof(%s))", e->sval, e->sval);
            break;
        case E_FIELD: {
            const char *clsName = thisClass;
            if (e->left && e->left->kind == E_IDENT) {
                Type *vt = sym_findVar(e->left->sval);
                if (vt) clsName = vt->name;
            }
            Class *c = sym_findClass(clsName);
            int depth = sym_getFieldDepth(c, e->sval);
            int baseIsPtr = isPtrExpr(e->left);

            if (e->left && e->left->kind == E_THIS) {
                fprintf(out, "this");
                for (int i = 0; i < depth; i++) fprintf(out, "->_parent");
                fprintf(out, depth > 0 ? ".%s" : "->%s", e->sval);
            } else {
                emitExpr(e->left, thisClass);
                for (int i = 0; i < depth; i++) {
                    if (i == 0 && baseIsPtr) fprintf(out, "->_parent");
                    else fprintf(out, "._parent");
                }
                fprintf(out, depth > 0 ? ".%s" : (baseIsPtr ? "->%s" : ".%s"), e->sval);
            }
            break;
        }
        case E_BINOP:
            fprintf(out, "("); emitExpr(e->left, thisClass);
            fprintf(out, " %s ", e->sval);
            emitExpr(e->right, thisClass); fprintf(out, ")");
            break;
        case E_CALL: {
            if (!e->left && e->sval && (!strcmp(e->sval, "printf") || !strcmp(e->sval, "print"))) {
                if (!strcmp(e->sval, "print")) {
                    fprintf(out, "printf(\"%%d\\n\"");
                    for (int i = 0; i < e->nargs; i++) { fprintf(out, ", "); emitExpr(e->args[i], thisClass); }
                    fprintf(out, ")");
                } else {
                    fprintf(out, "printf(");
                    for (int i = 0; i < e->nargs; i++) { if (i) fprintf(out, ", "); emitExpr(e->args[i], thisClass); }
                    fprintf(out, ")");
                }
                break;
            }
            if (!e->left && e->sval) {
                fprintf(out, "%s(", e->sval);
                for (int i = 0; i < e->nargs; i++) { if (i) fprintf(out, ", "); emitExpr(e->args[i], thisClass); }
                fprintf(out, ")");
                break;
            }

            const char *cls = thisClass;
            if (e->left && e->left->kind == E_IDENT) {
                Type *vt = sym_findVar(e->left->sval);
                if (vt) cls = vt->name;
            }

            Class *methodOwner = sym_findMethodOwner(sym_findClass(cls), e->sval);
            if (!methodOwner) break;
            Method *m = sym_findMethod(sym_findClass(cls), e->sval);
            int baseIsPtr = isPtrExpr(e->left);

            if (baseIsPtr && e->left && e->left->kind == E_IDENT) {
                /* Полиморфный вызов через vtable */
                Class *root = methodOwner;
                while (root->parent) root = sym_findClass(root->parent);

                fprintf(out, "((%s (*)(void*", translate_type(m->retType->name));
                for (int i = 0; i < m->nparams; i++) {
                    fprintf(out, ", %s", translate_type(m->paramTypes[i]->name));
                }
                fprintf(out, "))(((%s*)%s)->vtable[%d]))((void*)%s", root->name, e->left->sval, m->vtable_index, e->left->sval);
                for (int i = 0; i < e->nargs; i++) {
                    fprintf(out, ", "); emitExpr(e->args[i], thisClass);
                }
                fprintf(out, ")");
            } else {
                /* Статический вызов */
                fprintf(out, "%s_%s(", methodOwner->name, e->sval);
                if (e->left && e->left->kind == E_IDENT) {
                    fprintf(out, "(%s*)&%s", methodOwner->name, e->left->sval);
                } else if (e->left) {
                    emitExpr(e->left, thisClass);
                } else {
                    fprintf(out, "(%s*)this", methodOwner->name);
                }
                for (int i = 0; i < e->nargs; i++) {
                    fprintf(out, ", "); emitExpr(e->args[i], thisClass);
                }
                fprintf(out, ")");
            }
            break;
        }
        default: break;
    }
}

static void emitStmt(Stmt *s, const char *thisClass) {
    if (!s) return;
    switch (s->kind) {
        case S_BLOCK:
            fprintf(out, "{\n");
            for (int i = 0; i < s->nbody; i++) emitStmt(s->body[i], thisClass);
            fprintf(out, "}\n");
            break;
        case S_VARDECL: {
            int isPtr = (s->expr && s->expr->kind == E_NEW) || s->type->isPointer;
            fprintf(out, "%s %s%s", translate_type(s->type->name), isPtr ? "*" : "", s->name);
            if (s->expr) {
                fprintf(out, " = ");
                if (s->expr->kind == E_NEW) {
                    /* ВАЖНО: кастуем к типу ПЕРЕМЕННОЙ (Animal*), а не к типу выделяемого объекта (Dog*) */
                    fprintf(out, "(%s*)malloc(sizeof(%s))", translate_type(s->type->name), s->expr->sval);
                } else {
                    emitExpr(s->expr, thisClass);
                }
            }
            fprintf(out, ";\n");
            if (isPtr && s->expr && s->expr->kind == E_NEW) {
                Class *root = sym_findClass(s->expr->sval);
                while (root->parent) root = sym_findClass(root->parent);
                fprintf(out, "((%s*)%s)->vtable = %s_vtable;\n", root->name, s->name, s->expr->sval);
            }
            s->type->isPointer = isPtr;
            sym_addVar(s->name, s->type);
            break;
        }
        case S_ASSIGN:
            emitExpr(s->target, thisClass); fprintf(out, " = ");
            emitExpr(s->expr, thisClass); fprintf(out, ";\n");
            break;
        case S_IF:
            fprintf(out, "if ("); emitExpr(s->expr, thisClass); fprintf(out, ") ");
            emitStmt(s->body[0], thisClass);
            if (s->nelse) { fprintf(out, "else "); emitStmt(s->elseBody[0], thisClass); }
            break;
        case S_WHILE:
            fprintf(out, "while ("); emitExpr(s->expr, thisClass); fprintf(out, ") ");
            emitStmt(s->body[0], thisClass);
            break;
        case S_RETURN:
            fprintf(out, "return");
            if (s->expr) { fprintf(out, " "); emitExpr(s->expr, thisClass); }
            fprintf(out, ";\n");
            break;
        case S_EXPR:
            emitExpr(s->expr, thisClass); fprintf(out, ";\n");
            break;
    }
}

void codegen(Program *p, const char *outPath) {
    out = fopen(outPath, "w");
    if (!out) { perror("output"); exit(1); }

    fprintf(out, "#include <stdio.h>\n#include <stdlib.h>\n\n");

    // 1. Typedefs
    for (int i = 0; i < p->nclasses; i++) {
        Class *c = p->classes[i];
        fprintf(out, "typedef struct %s %s;\n", c->name, c->name);
    }
    fprintf(out, "\n");

    // 2. Structs (vtable только в корне иерархии!)
    for (int i = 0; i < p->nclasses; i++) {
        Class *c = p->classes[i];
        fprintf(out, "struct %s {\n", c->name);
        if (c->parent) {
            fprintf(out, "    %s _parent;\n", c->parent);
        } else {
            fprintf(out, "    void **vtable;\n");
        }
        for (int f = 0; f < c->nfields; f++)
            fprintf(out, "    %s %s;\n", translate_type(c->fields[f].type->name), c->fields[f].name);
        fprintf(out, "};\n\n");
    }

    // 3. Prototypes (ДОЛЖНЫ БЫТЬ ДО VTABLES!)
    for (int i = 0; i < p->nclasses; i++) {
        Class *c = p->classes[i];
        for (int m = 0; m < c->nmethods; m++) {
            Method *mm = &c->methods[m];
            fprintf(out, "%s %s_%s(%s *this", translate_type(mm->retType->name), c->name, mm->name, c->name);
            for (int a = 0; a < mm->nparams; a++)
                fprintf(out, ", %s %s", translate_type(mm->paramTypes[a]->name), mm->paramNames[a]);
            fprintf(out, ");\n");
        }
    }
    fprintf(out, "\n");

    // 4. Vtables
    for (int i = 0; i < p->nclasses; i++) {
        Class *c = p->classes[i];
        int max_idx = -1;
        for (int m = 0; m < c->nmethods; m++)
            if (c->methods[m].vtable_index > max_idx) max_idx = c->methods[m].vtable_index;

        Class *parent = sym_findClass(c->parent);
        while (parent) {
            for (int m = 0; m < parent->nmethods; m++)
                if (parent->methods[m].vtable_index > max_idx) max_idx = parent->methods[m].vtable_index;
            parent = sym_findClass(parent->parent);
        }

        if (max_idx >= 0) {
            fprintf(out, "void *%s_vtable[] = { ", c->name);
            for (int idx = 0; idx <= max_idx; idx++) {
                Method *found = NULL;
                Class *found_owner = NULL;
                Class *search = c;
                while (search && !found) {
                    for (int m = 0; m < search->nmethods; m++) {
                        if (search->methods[m].vtable_index == idx) {
                            found = &search->methods[m]; found_owner = search; break;
                        }
                    }
                    search = sym_findClass(search->parent);
                }
                if (found) fprintf(out, "(void*)%s_%s, ", found_owner->name, found->name);
                else fprintf(out, "0, ");
            }
            fprintf(out, "};\n");
        }
    }
    fprintf(out, "\n");

    // 5. Implementations
    for (int i = 0; i < p->nclasses; i++) {
        Class *c = p->classes[i];
        for (int m = 0; m < c->nmethods; m++) {
            Method *mm = &c->methods[m];
            fprintf(out, "%s %s_%s(%s *this", translate_type(mm->retType->name), c->name, mm->name, c->name);
            for (int a = 0; a < mm->nparams; a++)
                fprintf(out, ", %s %s", translate_type(mm->paramTypes[a]->name), mm->paramNames[a]);
            fprintf(out, ") ");

            sym_pushScope();
            Type *tthis = (Type*)calloc(1, sizeof(Type));
            tthis->name = c->name; tthis->isPointer = 1;
            sym_addVar("this", tthis);
            for (int a = 0; a < mm->nparams; a++) {
                Type *t = (Type*)calloc(1, sizeof(Type));
                t->name = mm->paramTypes[a]->name;
                sym_addVar(mm->paramNames[a], t);
            }
            emitStmt(mm->body, c->name);
            sym_popScope();
            fprintf(out, "\n");
        }
    }

    // 6. Main
    for (int i = 0; i < p->nfuncs; i++) {
        Stmt *fn = p->funcs[i];
        fprintf(out, "%s %s(void) ", translate_type(fn->type->name), fn->name);
        sym_pushScope();
        Stmt body = *fn;
        body.kind = S_BLOCK;
        emitStmt(&body, NULL);
        sym_popScope();
        fprintf(out, "\n");
    }

    fclose(out);
}
