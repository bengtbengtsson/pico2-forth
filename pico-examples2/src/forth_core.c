#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "forth_core.h"

#define STACK_SIZE 64      // deep enough for simple Forth
#define INPUT_BUF 128      // max input line length
#define WORD_BUF 32        // max token length

// Data stack
static long stack[STACK_SIZE];
static int sp = 0;

static void push(long v) {
    if (sp < STACK_SIZE) stack[sp++] = v;
    else printf("Error: stack overflow\r\n");
}
static long pop() {
    if (sp > 0) return stack[--sp];
    printf("Error: stack underflow\r\n");
    return 0;
}

// Primitive word functions
static void w_add()   { if (sp<2){ printf("Error: + requires 2 items\r\n"); return;} long b=pop(), a=pop(); push(a+b); }
static void w_sub()   { if (sp<2){ printf("Error: - requires 2 items\r\n"); return;} long b=pop(), a=pop(); push(a-b); }
static void w_mul()   { if (sp<2){ printf("Error: * requires 2 items\r\n"); return;} long b=pop(), a=pop(); push(a*b); }
static void w_div()   { if (sp<2){ printf("Error: / requires 2 items\r\n"); return;} long b=pop(), a=pop(); if(b==0){ printf("Error: division by zero\r\n"); push(0);} else push(a/b); }
static void w_dot()   { if (sp<1){ printf("Error: . requires 1 item\r\n"); return;} long v=pop(); printf("%ld ", v); }
static void w_dot_s() { printf("<%d> ", sp); for(int i=0;i<sp;i++) printf("%ld ", stack[i]); printf("\r\n"); }

// Stack-manipulation
static void w_dup()   { if(sp<1){ printf("Error: DUP requires 1 item\r\n"); return;} push(stack[sp-1]); }
static void w_drop()  { if(sp<1){ printf("Error: DROP requires 1 item\r\n"); return;} sp--; }
static void w_swap()  { if(sp<2){ printf("Error: SWAP requires 2 items\r\n"); return;} long a=pop(), b=pop(); push(a); push(b); }
static void w_over()  { if(sp<2){ printf("Error: OVER requires 2 items\r\n"); return;} push(stack[sp-2]); }
static void w_rot()   { if(sp<3){ printf("Error: ROT requires 3 items\r\n"); return;} long a=pop(), b=pop(), c=pop(); push(b); push(a); push(c); }

// Dictionary entry
typedef void (*word_fn)();
typedef struct { const char *name; word_fn fn; } word_t;

static word_t dict[] = {
    // arithmetic
    {"+",   w_add},
    {"-",   w_sub},
    {"*",   w_mul},
    {"/",   w_div},
    // output
    {".",   w_dot},
    {".S",  w_dot_s},
    // stack ops
    {"DUP", w_dup},
    {"DROP",w_drop},
    {"SWAP",w_swap},
    {"OVER",w_over},
    {"ROT", w_rot},
};
static const int dict_len = sizeof(dict)/sizeof(dict[0]);

// Evaluate one token (case-insensitive)
static void eval(const char *tok) {
    char uword[WORD_BUF];
    int len=strlen(tok);
    if(len>=WORD_BUF) len=WORD_BUF-1;
    for(int i=0;i<len;i++) uword[i]=toupper((unsigned char)tok[i]);
    uword[len]='\0';

    for(int i=0;i<dict_len;i++){
        if(strcmp(uword, dict[i].name)==0){ dict[i].fn(); return; }
    }
    char *end;
    long v=strtol(tok,&end,10);
    if(end!=tok) push(v);
    else printf("? %s\r\n", tok);
}

// Main loop (blocks on stdin/stdout)
int forth_main_loop(void) {
    char input[INPUT_BUF];
    printf("Simple Forth Interpreter - Phase 1 (Stack Ops)\r\n");
    while(1) {
        printf("ok> "); fflush(stdout);
        int len=0;
        while(1) {
            int c=getchar(); if(c<0) continue;
            if(c=='\r'||c=='\n'){ putchar('\r'); putchar('\n'); break; }
            else if(c==0x7f||c=='\b'){ if(len>0){ putchar('\b'); putchar(' '); putchar('\b'); len--; }}
            else if(len<INPUT_BUF-1){ putchar(c); fflush(stdout); input[len++]=(char)c; }
        }
        input[len]='\0';
        char *tok=strtok(input," \t");
        while(tok){ eval(tok); tok=strtok(NULL," \t"); }
        printf("\r\n"); fflush(stdout);
    }
    return 0;
}
