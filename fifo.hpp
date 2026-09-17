#ifndef FIFO_H
#define FIFO_H

typedef int (*f_callback)();

int initFIFO(f_callback off_callback, f_callback on_callback);

int cleanupFIFO();

// Call from the main loop, never from a signal handler. Timeout is in milliseconds.
int processFIFO(int timeout_ms);

void registerOffCallback(f_callback callback);

void registerOnCallback(f_callback callback);

#endif
