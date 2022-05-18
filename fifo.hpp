#ifndef FIFO_H
#define FIFO_H

typedef int (*f_callback)();

int initFIFO(f_callback off_callback, f_callback on_callback);

int cleanupFIFO();

void registerOffCallback(f_callback callback);

void registerOnCallback(f_callback callback);

#endif
