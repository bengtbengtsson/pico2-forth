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
#define VAR_BASE 100

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
}

// Eval
static void eval(const char *tok) {
    char uword[WORD_BUF];
    int len = strlen(tok);
    if (len >= WORD_BUF) len = WORD_BUF - 1;
    strncpy(uword, tok, len);
    uword[len] = '\0';

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

