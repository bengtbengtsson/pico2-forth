#ifndef FORTH_CORE_H
#define FORTH_CORE_H

// Runs the interactive Forth loop on stdin/stdout
// Returns when stdin hits EOF (host) or never returns (Pico).
int forth_main_loop(void);

#endif // FORTH_CORE_H
