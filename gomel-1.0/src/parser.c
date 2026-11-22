#include "gomel.h"

static Token *T;
static int P, N;

static Token *peek(void) { return &T[P]; }
static Token *next(void) { return &T[P++]; }
static int  accept(TokType t) { if (peek()->type == t) { P++; return 1; } return 0; }
static void expect(TokType t) {
    if (!accept(t)) {
        fprintf(stderr, "Gomel: строка %d: ожидался токен %d, получен '%s'\n",
                peek()->line, (int)t, peek()->text);
        exit(1);
    }
}

static Type *parseType(void) {
    Token *t = next();
    Type *ty = (Type*)calloc(1, sizeof(Type));
    ty->name = strdup(t->text);
    return ty;
}

static Expr *parseExpr(void);

static Expr *parseTerm(void) {
    Token *t = peek();
    Expr *e = (Expr*)calloc(1, sizeof(Expr));

    /* унарный минус: -expr */
    if (t->type == T_MINUS) {
        next();
        Expr *operand = parseTerm();
        /* представляем как 0 - operand */
        e->kind = E_BINOP;
        e->sval = strdup("-");
        Expr *zero = (Expr*)calloc(1, sizeof(Expr));
        zero->kind = E_NUM;
        zero->ival = 0;
        e->left = zero;
        e->right = operand;
        return e;
    }

    /* унарный плюс: +expr — просто игнорируем */
    if (t->type == T_PLUS) {
        next();
        free(e);
        return parseTerm();
    }

    if (t->type == T_NUMBER) {
        next(); e->kind = E_NUM; e->ival = atoi(t->text);
    }
    else if (t->type == T_STRING) {
        next(); e->kind = E_STR; e->sval = strdup(t->text);
    }
    else if (t->type == T_THIS) {
        next();
        if (accept(T_DOT)) {
            Token *m = next();
            e->kind = E_FIELD;
            e->sval = strdup(m->text);
            e->left = (Expr*)calloc(1, sizeof(Expr));
            e->left->kind = E_THIS;
        } else {
            e->kind = E_THIS;
        }
    }

    else if (t->type == T_NEW) {
        next();
        Token *clsName = next();
        e->kind = E_NEW;
        e->sval = strdup(clsName->text);
        expect(T_LPAREN);
        expect(T_RPAREN);
    }

    else if (t->type == T_IDENT) {
        next();
        if (accept(T_LPAREN)) {
            e->kind = E_CALL; e->sval = strdup(t->text);
            while (!accept(T_RPAREN)) {
                e->args = (Expr**)realloc(e->args, sizeof(Expr*) * (e->nargs + 1));
                e->args[e->nargs++] = parseExpr();
                accept(T_COMMA);
            }
        }
        else if (accept(T_DOT)) {
            Token *m = next();
            if (accept(T_LPAREN)) {
                e->kind = E_CALL; e->sval = strdup(m->text);
                e->left = (Expr*)calloc(1, sizeof(Expr));
                e->left->kind = E_IDENT; e->left->sval = strdup(t->text);
                while (!accept(T_RPAREN)) {
                    e->args = (Expr**)realloc(e->args, sizeof(Expr*) * (e->nargs + 1));
                    e->args[e->nargs++] = parseExpr();
                    accept(T_COMMA);
                }
            } else {
                e->kind = E_FIELD; e->sval = strdup(m->text);
                e->left = (Expr*)calloc(1, sizeof(Expr));
                e->left->kind = E_IDENT; e->left->sval = strdup(t->text);
            }
        }
        else {
            e->kind = E_IDENT; e->sval = strdup(t->text);
        }
    }
    else if (accept(T_LPAREN)) {
        free(e);
        e = parseExpr();
        expect(T_RPAREN);
    }
    else {
        fprintf(stderr, "Gomel: строка %d: неожиданный токен '%s'\n", t->line, t->text);
        exit(1);
    }
    return e;
}

static Expr *parseExpr(void) {
    Expr *left = parseTerm();
    while (peek()->type == T_PLUS || peek()->type == T_MINUS ||
           peek()->type == T_STAR || peek()->type == T_SLASH ||
           peek()->type == T_EQ   || peek()->type == T_LT   || peek()->type == T_GT) {
        Token *op = next();
        Expr *b = (Expr*)calloc(1, sizeof(Expr));
        b->kind = E_BINOP;
        b->sval = strdup(op->text);
        b->left = left;
        b->right = parseTerm();
        left = b;
    }
    return left;
}

static Stmt *parseBlock(void);

static Stmt *parseStmt(void) {
    Token *t = peek();
    Stmt *s = (Stmt*)calloc(1, sizeof(Stmt));

    if (t->type == T_LBRACE) {
        return parseBlock();
    }

    if (t->type == T_IF) {
        next();
        expect(T_LPAREN);
        s->kind = S_IF;
        s->expr = parseExpr();
        expect(T_RPAREN);
        s->body = (Stmt**)realloc(s->body, sizeof(Stmt*) * (s->nbody + 1));
        s->body[s->nbody++] = parseBlock();
        if (accept(T_ELSE)) {
            s->elseBody = (Stmt**)realloc(s->elseBody, sizeof(Stmt*) * (s->nelse + 1));
            s->elseBody[s->nelse++] = parseBlock();
        }
        return s;
    }

    if (t->type == T_WHILE) {
        next();
        expect(T_LPAREN);
        s->kind = S_WHILE;
        s->expr = parseExpr();
        expect(T_RPAREN);
        s->body = (Stmt**)realloc(s->body, sizeof(Stmt*) * (s->nbody + 1));
        s->body[s->nbody++] = parseBlock();
        return s;
    }

    if (t->type == T_RETURN) {
        next();
        s->kind = S_RETURN;
        if (!accept(T_SEMI)) { s->expr = parseExpr(); expect(T_SEMI); }
        return s;
    }

    if (t->type == T_INT || t->type == T_VOID ||
        (t->type == T_IDENT && peek()[1].type == T_IDENT)) {
        Type *ty = parseType();
        Token *nm = next();
        s->kind = S_VARDECL;
        s->type = ty;
        s->name = strdup(nm->text);

        if (accept(T_ASSIGN)) {
            s->expr = parseExpr();
            /* Если переменная инициализируется через plus, помечаем её как указатель */
            if (s->expr && s->expr->kind == E_NEW) {
                ty->isPointer = 1;
            }
        }
        expect(T_SEMI);
        return s;
    }

    Expr *e = parseExpr();
    if (accept(T_ASSIGN)) {
        s->kind = S_ASSIGN;
        s->target = e;
        s->expr = parseExpr();
    } else {
        s->kind = S_EXPR;
        s->expr = e;
    }
    expect(T_SEMI);
    return s;
}

static Stmt *parseBlock(void) {
    Stmt *b = (Stmt*)calloc(1, sizeof(Stmt));
    b->kind = S_BLOCK;
    expect(T_LBRACE);
    while (!accept(T_RBRACE)) {
        b->body = (Stmt**)realloc(b->body, sizeof(Stmt*) * (b->nbody + 1));
        b->body[b->nbody++] = parseStmt();
    }
    return b;
}

static Class *parseClass(void) {
    expect(T_CLASS);
    Class *c = (Class*)calloc(1, sizeof(Class));
    c->name = strdup(next()->text);
    if (accept(T_EXTENDS)) c->parent = strdup(next()->text);
    expect(T_LBRACE);

    while (!accept(T_RBRACE)) {
        Type *ty = parseType();
        char *name = strdup(next()->text);

        if (accept(T_LPAREN)) {
            Method *m = (Method*)calloc(1, sizeof(Method));
            m->retType = ty;
            m->name = name;
            while (!accept(T_RPAREN)) {
                m->paramTypes = (Type**)realloc(m->paramTypes, sizeof(Type*) * (m->nparams + 1));
                m->paramNames = (char**)realloc(m->paramNames, sizeof(char*) * (m->nparams + 1));
                m->paramTypes[m->nparams] = parseType();
                m->paramNames[m->nparams] = strdup(next()->text);
                m->nparams++;
                accept(T_COMMA);
            }
            m->body = parseBlock();
            c->methods = (Method*)realloc(c->methods, sizeof(Method) * (c->nmethods + 1));
            c->methods[c->nmethods++] = *m;
            free(m);
        } else {
            Field f;
            f.type = ty;
            f.name = name;
            c->fields = (Field*)realloc(c->fields, sizeof(Field) * (c->nfields + 1));
            c->fields[c->nfields++] = f;
            expect(T_SEMI);
        }
    }
    return c;
}

Program *parse(Token *toks, int count) {
    T = toks; P = 0; N = count;
    Program *p = (Program*)calloc(1, sizeof(Program));

    while (peek()->type != T_EOF) {
        if (peek()->type == T_CLASS) {
            Class *c = parseClass();
            p->classes = (Class**)realloc(p->classes, sizeof(Class*) * (p->nclasses + 1));
            p->classes[p->nclasses++] = c;
        } else {
            Type *ret = parseType();
            char *fname = strdup(next()->text);
            Stmt *fn = (Stmt*)calloc(1, sizeof(Stmt));
            fn->kind = S_BLOCK;
            fn->type = ret;
            fn->name = fname;

            expect(T_LPAREN);
            while (!accept(T_RPAREN)) {
                parseType();
                next();
                accept(T_COMMA);
            }

            Stmt *b = parseBlock();
            fn->body = b->body;
            fn->nbody = b->nbody;

            p->funcs = (Stmt**)realloc(p->funcs, sizeof(Stmt*) * (p->nfuncs + 1));
            p->funcs[p->nfuncs++] = fn;
        }
    }
    return p;
}
