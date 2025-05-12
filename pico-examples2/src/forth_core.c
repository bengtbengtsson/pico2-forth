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

static long stack[STACK_SIZE];
static int sp = 0;
static long memory[MEM_SIZE];

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
static void w_add() {
    if (sp < 2) {
        printf("Error: + requires 2 items\r\n");
        return;
    }
    long b = pop(), a = pop();
    push(a + b);
}
static void w_sub() {
    if (sp < 2) {
        printf("Error: - requires 2 items\r\n");
        return;
    }
    long b = pop(), a = pop();
    push(a - b);
}
static void w_mul() {
    if (sp < 2) {
        printf("Error: * requires 2 items\r\n");
        return;
    }
    long b = pop(), a = pop();
    push(a * b);
}
static void w_div() {
    if (sp < 2) {
        printf("Error: / requires 2 items\r\n");
        return;
    }
    long b = pop(), a = pop();
    if (b == 0) {
        printf("Error: division by zero\r\n");
        push(0);
    } else {
        push(a / b);
    }
}

// Stack ops
static void w_dot() {
    if (sp < 1) {
        printf("Error: . requires 1 item\r\n");
        return;
    }
    printf("%ld\r\n", pop());
}
static void w_dot_s() {
    printf("<%d> ", sp);
    for (int i = 0; i < sp; i++) printf("%ld ", stack[i]);
    printf("\r\n");
}
static void w_dup() {
    if (sp < 1) {
        printf("Error: DUP requires 1 item\r\n");
        return;
    }
    push(stack[sp - 1]);
}
static void w_drop() {
    if (sp < 1) {
        printf("Error: DROP requires 1 item\r\n");
        return;
    }
    sp--;
}
static void w_swap() {
    if (sp < 2) {
        printf("Error: SWAP requires 2 items\r\n");
        return;
    }
    long a = pop(), b = pop();
    push(a);
    push(b);
}
static void w_over() {
    if (sp < 2) {
        printf("Error: OVER requires 2 items\r\n");
        return;
    }
    push(stack[sp - 2]);
}
static void w_rot() {
    if (sp < 3) {
        printf("Error: ROT requires 3 items\r\n");
        return;
    }
    long a = pop(), b = pop(), c = pop();
    push(b);
    push(a);
    push(c);
}

// Memory
static void w_store() {
    if (sp < 2) {
        printf("Error: ! requires 2 items\r\n");
        return;
    }
    long addr = pop();
    long val = pop();
#ifdef FORTH_DEBUG
    printf("[DEBUG] store: addr=%ld, val=%ld\n", addr, val);
#endif
    if (addr >= 0 && addr < MEM_SIZE) memory[addr] = val;
    else printf("Error: invalid store address %ld\r\n", addr);
}
static void w_fetch() {
    if (sp < 1) {
        printf("Error: @ requires 1 item\r\n");
        return;
    }
    long addr = pop();
    if (addr >= 0 && addr < MEM_SIZE) {
        long val = memory[addr];
#ifdef FORTH_DEBUG
        printf("[DEBUG] fetch: addr=%ld, val=%ld\n", addr, val);
#endif
        push(val);
    } else {
        printf("Error: invalid fetch address %ld\r\n", addr);
    }
}

typedef void (*word_fn)();
typedef struct { const char *name; word_fn fn; } word_t;

static word_t dict[] = {
    {"+", w_add}, {"-", w_sub}, {"*", w_mul}, {"/", w_div},
    {".", w_dot}, {".S", w_dot_s},
    {"DUP", w_dup}, {"DROP", w_drop}, {"SWAP", w_swap},
    {"OVER", w_over}, {"ROT", w_rot},
    {"!", w_store}, {"@", w_fetch},
};

static const int dict_len = sizeof(dict) / sizeof(dict[0]);

static void eval(const char *tok) {
    char uword[WORD_BUF];
    int len = strlen(tok);
    if (len >= WORD_BUF) len = WORD_BUF - 1;
    for (int i = 0; i < len; i++) uword[i] = toupper((unsigned char)tok[i]);
    uword[len] = '\0';

    for (int i = 0; i < dict_len; i++) {
        if (strcmp(uword, dict[i].name) == 0) {
#ifdef FORTH_DEBUG
            printf("[DEBUG] match: %s\n", dict[i].name);
#endif
            dict[i].fn();
            return;
        }
    }

    char *end;
    long v = strtol(tok, &end, 10);
    if (*end == '\0') {
        push(v);
        return;
    }

    printf("? %s\r\n", tok);
}

int forth_main_loop(void) {
    char input[INPUT_BUF];
    printf("Simple Forth Interpreter - Phase 1 (Stack Ops)\r\n");

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

