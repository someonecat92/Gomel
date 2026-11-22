#include "gomel.h"

static const struct { const char *kw; TokType t; } keywords[] = {
    {"form", T_CLASS},
    {"inherit", T_EXTENDS},
    {"this", T_THIS},
    {"number", T_INT},
    {"return", T_RETURN},
    {"provided", T_IF},
    {"loop", T_WHILE},
    {"plus", T_NEW},
    {"void", T_VOID},
    {NULL, T_UNKNOWN}
};

Token *lex(const char *src, int *outCount) {
    Token *toks = (Token*)malloc(sizeof(Token) * 4096);
    int n = 0, line = 1;
    const char *p = src;

    while (*p) {
        if (*p == '\n') { line++; p++; continue; }
        if (isspace((unsigned char)*p)) { p++; continue; }
    if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }

        // Добавляем поддержку /* ... */
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) {
                if (*p == '\n') line++; /* чтобы номера строк не сбивались */
                p++;
            }
            if (*p) p += 2;
            continue;
        }

        Token t;
        t.type = T_UNKNOWN; t.text = NULL; t.line = line;

        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *s = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            char *buf = gomel_strndup(s, (size_t)(p - s));
            t.type = T_IDENT;
            for (int i = 0; keywords[i].kw; i++)
                if (!strcmp(buf, keywords[i].kw)) { t.type = keywords[i].t; break; }
            t.text = buf;
        }
        else if (isdigit((unsigned char)*p)) {
            const char *s = p;
            while (isdigit((unsigned char)*p)) p++;
            t.type = T_NUMBER;
            t.text = gomel_strndup(s, (size_t)(p - s));
        }
        else if (*p == '"') {
            p++;
            const char *s = p;
            while (*p && *p != '"') p++;
            t.type = T_STRING;
            t.text = gomel_strndup(s, (size_t)(p - s));
            if (*p == '"') p++;
        }
        else {
            char c = *p++;
            switch (c) {
                case '{': t.type = T_LBRACE; break;
                case '}': t.type = T_RBRACE; break;
                case '(': t.type = T_LPAREN; break;
                case ')': t.type = T_RPAREN; break;
                case ';': t.type = T_SEMI;   break;
                case ',': t.type = T_COMMA;  break;
                case '.': t.type = T_DOT;    break;
                case '+': t.type = T_PLUS;   break;
                case '-': t.type = T_MINUS;  break;
                case '*': t.type = T_STAR;   break;
                case '/': t.type = T_SLASH;  break;
                case '=':
                    if (*p == '=') { p++; t.type = T_EQ; }
                    else t.type = T_ASSIGN;
                    break;
                case '<': t.type = T_LT; break;
                case '>': t.type = T_GT; break;
                default:  t.type = T_UNKNOWN;
            }
            char tmp[2]; tmp[0] = c; tmp[1] = 0;
            t.text = strdup(tmp);
        }
        toks[n++] = t;
    }
    Token eof; eof.type = T_EOF; eof.text = strdup(""); eof.line = line;
    toks[n++] = eof;
    *outCount = n;
    return toks;
}
