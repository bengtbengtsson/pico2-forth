#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef FORTH_DEBUG
#include <assert.h>
#endif

#include "forth_core.h"

#define STACK_SIZE 64
#define INPUT_BUF 128
#define WORD_BUF 32
#define MEM_SIZE 1024
#define VAR_LIMIT 32
#define CONST_LIMIT 32
#define VAR_BASE 100 /* first free RAM-cell for VARIABLES */

#define THREAD_MAX 512           /* unified code space               */
#define OP_LIT     -1            /* marker  LIT   <value>            */
#define OP_EXIT    -2            /* marker  EXIT  ( ; )              */

static int   thread[THREAD_MAX];
static int   thread_len = 0;     /* next free cell in thread[]       */
static int   colon_ip[128];      /* dict-index → start ip            */
static int   compiling = 0;      /* true between : … ;               */
static int   current_def_idx = -1;

static long stack[STACK_SIZE];
static int sp = 0;
static long memory[MEM_SIZE];

typedef void (*word_fn)();
typedef struct { const char *name; word_fn fn; } word_t;

static word_t dict[128];
static int dict_len = 0;

static struct { char name[WORD_BUF]; int addr; } vars[VAR_LIMIT];
static int var_count = 0;

static struct { char name[WORD_BUF]; long value; } consts[CONST_LIMIT];
static int const_count = 0;

static const char *current_lookup_word = NULL;

static void push(long v) {
#ifdef FORTH_DEBUG
    assert(sp >= 0 && sp < STACK_SIZE);
    printf("[DEBUG] push: %ld\n", v);
#endif
    stack[sp++] = v;
}

static long pop() {
    if (sp > 0) {
        long v = stack[--sp];
#ifdef FORTH_DEBUG
        printf("[DEBUG] pop: %ld\n", v);
#endif
        return v;
    }
    printf("Error: stack underflow\r\n");
    return 0;
}

/* ---------------- helpers for colon compilation ----------- */
static void compile_word(const char *tok)
{
    /* 1. numeric literal → LIT value ----------------------- */
    char *end;
    long v = strtol(tok, &end, 10);
    if (*end == '\0') {
        if (thread_len + 2 >= THREAD_MAX) { printf("Error: thread overflow\r\n"); return; }
        thread[thread_len++] = OP_LIT;
        thread[thread_len++] = (int)v;
        return;
    }

    /* 2. previously known word → dict-index ---------------- */
    for (int i = 0; i < dict_len; i++) {
        if (strcmp(tok, dict[i].name) == 0) {
            if (thread_len >= THREAD_MAX) { printf("Error: thread overflow\r\n"); return; }
            thread[thread_len++] = i;
            return;
        }
    }
    printf("? %s\r\n", tok);   /* unknown at compile-time */
}

/* ---------------- runtime for colon words ----------------- */
static void colon_dispatch(void)
{
    int idx = -1;
    for (int i = 0; i < dict_len; i++)
        if (dict[i].name == current_lookup_word) { idx = i; break; }
    if (idx < 0) { printf("Error: colon dispatch\n"); return; }

    int ip = colon_ip[idx];
    for (;;) {
        int code = thread[ip++];
        if (code == OP_EXIT) break;
        if (code == OP_LIT)  { push(thread[ip++]); continue; }

        current_lookup_word = dict[code].name;
        if (dict[code].fn) {
            dict[code].fn();              /* primitive / constant   */
        } else {
            /* variable   (fn == NULL) --------------------------- */
            for (int j = 0; j < var_count; j++)
                if (strcmp(current_lookup_word, vars[j].name) == 0) {
                    push(vars[j].addr);
                    goto next;
                }
            printf("Error: unresolved word %s\r\n", current_lookup_word);
        }
    next:;
    }
}


/* -------------------------------------------------------------------------
 *--- Floored division helper (ANS Forth semantics)
 *-------------------------------------------------------------------------*/
static inline void floored_divmod(long a, long b, long *q, long *r)
/*  Ensures:
 *      a = b*q + r
 *      0 ≤ |r| < |b|
 *      sign(r) == sign(b)
 */
{
    long q0 = a / b;          /* C truncates toward zero            */
    long r0 = a % b;

    /* If remainder sign differs from divisor sign, adjust */
    if (r0 != 0 && ((r0 < 0) != (b < 0))) {
        r0 += b;
        q0 -= 1;
    }
    *q = q0;
    *r = r0;
}

// Arithmetic
static void w_add() { if (sp < 2) { printf("Error: + requires 2 items\r\n"); return; } push(pop() + pop()); }

static void w_sub() { if (sp < 2) { printf("Error: - requires 2 items\r\n"); return; } long b = pop(); push(pop() - b); }

static void w_mul() { if (sp < 2) { printf("Error: * requires 2 items\r\n"); return; } push(pop() * pop()); }

static void w_div() {
    if (sp < 2) { printf("Error: / requires 2 items\r\n"); return; }
    long b = pop(); if (b == 0) { printf("Error: division by zero\r\n"); push(0); } else push(pop() / b);
}

static void w_dot() { if (sp < 1) { printf("Error: . requires 1 item\r\n"); return; } printf("%ld\r\n", pop()); }

static void w_dot_s() {
    printf("<%d> ", sp);
    for (int i = 0; i < sp; i++) printf("%ld ", stack[i]);
    printf("\r\n");
    fflush(stdout);  // Ensure output is flushed immediately
}

static void w_dup() { if (sp < 1) { printf("Error: DUP requires 1 item\r\n"); return; } push(stack[sp - 1]); }

static void w_drop() { if (sp < 1) { printf("Error: DROP requires 1 item\r\n"); return; } sp--; }

static void w_swap() { if (sp < 2) { printf("Error: SWAP requires 2 items\r\n"); return; } long a = pop(), b = pop(); push(a); push(b); }

static void w_over() { if (sp < 2) { printf("Error: OVER requires 2 items\r\n"); return; } push(stack[sp - 2]); }

static void w_rot() { if (sp < 3) { printf("Error: ROT requires 3 items\r\n"); return; } long a = pop(), b = pop(), c = pop(); push(b); push(a); push(c); }

/* MOD ( n1 n2 -- nrem )  — remainder only */
static void w_mod(void)
{
    if (sp < 2) { printf("Error: MOD requires 2 items\r\n"); return; }

    long b = pop();           /* divisor  */
    long a = pop();           /* dividend */

    if (b == 0) { printf("Error: division by zero\r\n"); return; }

    long q, r;
    floored_divmod(a, b, &q, &r);
    push(r);                  /* leave just the remainder */
}

/* /MOD ( n1 n2 -- nrem nquot ) */
static void w_divmod(void)
{
    if (sp < 2) { printf("Error: /MOD requires 2 items\r\n"); return; }

    long b = pop();           /* divisor  */
    long a = pop();           /* dividend */

    if (b == 0) { printf("Error: division by zero\r\n"); return; }

    long q, r;
    floored_divmod(a, b, &q, &r);
    push(r);                  /* remainder  (lower on stack) */
    push(q);                  /* quotient   (top-of-stack)   */
}

// Memory
static void w_store() {
    if (sp < 2) { printf("Error: ! requires 2 items\r\n"); return; }
    long addr = pop(); long val = pop();
    if (addr >= 0 && addr < MEM_SIZE) memory[addr] = val;
    else printf("Error: invalid store address %ld\r\n", addr);
}
static void w_fetch() {
    if (sp < 1) { printf("Error: @ requires 1 item\r\n"); return; }
    long addr = pop();
    if (addr >= 0 && addr < MEM_SIZE) push(memory[addr]);
    else printf("Error: invalid fetch address %ld\r\n", addr);
}

// VARIABLE
static void w_variable() {
    if (var_count >= VAR_LIMIT) { printf("Error: max VARIABLES reached\r\n"); return; }
    char *name = strtok(NULL, " \t\r\n");
    if (!name) { printf("Error: VARIABLE needs a name\r\n"); return; }
    strncpy(vars[var_count].name, name, WORD_BUF);
    vars[var_count].addr = VAR_BASE + var_count;
#ifdef FORTH_DEBUG
    printf("[DEBUG] define variable %s → %d\n", name, vars[var_count].addr);
#endif
    var_count++;
    dict[dict_len++] = (word_t){ .name = name, .fn = NULL };
}

// CONSTANT
static void const_dispatcher(void) {
    for (int i = 0; i < const_count; i++) {
        if (strcmp(current_lookup_word, consts[i].name) == 0) {
            push(consts[i].value);
            return;
        }
    }
    printf("Error: constant dispatch failed for %s\r\n", current_lookup_word);
}

static void w_constant() {
    if (sp < 1) { printf("Error: CONSTANT requires a value\r\n"); return; }
    char *name = strtok(NULL, " \t\r\n");
    if (!name || const_count >= CONST_LIMIT) {
        printf("Error: invalid constant declaration\r\n");
        return;
    }
    strncpy(consts[const_count].name, name, WORD_BUF);
    consts[const_count].value = pop();
    const_count++;
    dict[dict_len++] = (word_t){ .name = consts[const_count - 1].name, .fn = const_dispatcher };
}

// Core dictionary
static void init_primitives() {
    dict[dict_len++] = (word_t){"+", w_add};
    dict[dict_len++] = (word_t){"-", w_sub};
    dict[dict_len++] = (word_t){"*", w_mul};
    dict[dict_len++] = (word_t){"/", w_div};
    dict[dict_len++] = (word_t){".", w_dot};
    dict[dict_len++] = (word_t){".S", w_dot_s};  // uppercase version
    dict[dict_len++] = (word_t){".s", w_dot_s};  // lowercase alias
    dict[dict_len++] = (word_t){"DUP", w_dup};
    dict[dict_len++] = (word_t){"DROP", w_drop};
    dict[dict_len++] = (word_t){"SWAP", w_swap};
    dict[dict_len++] = (word_t){"OVER", w_over};
    dict[dict_len++] = (word_t){"ROT", w_rot};
    dict[dict_len++] = (word_t){"!", w_store};
    dict[dict_len++] = (word_t){"@", w_fetch};
    dict[dict_len++] = (word_t){"VARIABLE", w_variable};
    dict[dict_len++] = (word_t){"CONSTANT", w_constant};
    dict[dict_len++] = (word_t){"MOD", w_mod};
    dict[dict_len++] = (word_t){"/MOD", w_divmod};
}

// Eval
static void eval(const char *tok) {
    char uword[WORD_BUF];
    int len = strlen(tok);
    if (len >= WORD_BUF) len = WORD_BUF - 1;
    strncpy(uword, tok, len);
    uword[len] = '\0';

    if (compiling) {                      /* inside a definition  */
        if (strcmp(tok, ";") == 0) {
            thread[thread_len++] = OP_EXIT;
            compiling = 0;
            return;
        }
        compile_word(tok);
        return;
    }

    if (strcmp(tok, ":") == 0) {          /* start new definition */
        char *name = strtok(NULL, " \t\r\n");
        if (!name) { printf("Error: : requires a name\r\n"); return; }
        current_def_idx  = dict_len;
        dict[dict_len++] = (word_t){ .name = strdup(name), .fn = colon_dispatch };
        colon_ip[current_def_idx] = thread_len;
        compiling = 1;
        return;
    }


    for (int i = 0; i < dict_len; i++) {
        if (strcmp(uword, dict[i].name) == 0) {
            current_lookup_word = dict[i].name;
#ifdef FORTH_DEBUG
            printf("[DEBUG] matched word: %s\n", uword);
#endif
            if (dict[i].fn) {
                dict[i].fn();
            } else {
                for (int j = 0; j < var_count; j++) {
                    if (strcmp(uword, vars[j].name) == 0) {
#ifdef FORTH_DEBUG
                        printf("[DEBUG] variable %s → %d\n", uword, vars[j].addr);
#endif
                        push(vars[j].addr);
                        return;
                    }
                }
                printf("Error: unresolved word %s\r\n", uword);
            }
            return;
        }
    }

    char *end;
    long v = strtol(tok, &end, 10);
    if (*end == '\0') { push(v); return; }
    printf("? %s\r\n", tok);
}

// REPL
int forth_main_loop(void) {
    char input[INPUT_BUF];
    init_primitives();
    printf("Simple Forth Interpreter - Phase 2 (Variables & Constants)\r\n");

    while (1) {
        printf("ok> "); fflush(stdout);
        int len = 0;
        while (1) {
            int c = getchar(); if (c < 0) continue;
            if (c == '\r' || c == '\n') { putchar('\r'); putchar('\n'); break; }
            else if (c == 0x7f || c == '\b') {
                if (len > 0) { putchar('\b'); putchar(' '); putchar('\b'); len--; }
            }
            else if (len < INPUT_BUF - 1) {
                putchar(c); fflush(stdout); input[len++] = (char)c;
            }
        }
        input[len] = '\0';

        char *tok = strtok(input, " \t");
        while (tok) {
            eval(tok);
            tok = strtok(NULL, " \t");
        }
        printf("\r\n"); fflush(stdout);
    }
    return 0;
}

