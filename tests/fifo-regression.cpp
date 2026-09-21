#include "fifo.hpp"
#include <cassert>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>

int on_count = 0;
int off_count = 0;
int on() { return ++on_count; }
int off() { return ++off_count; }

int main() {
    const char* path = "/tmp/p-cec-fix";
    // Refuse to touch another running instance's pipe.
    struct stat info;
    if (lstat(path, &info) == 0) {
        std::fprintf(stderr, "Test requires %s to be absent\n", path);
        return 1;
    }
    assert(initFIFO(off, on) == 1);
    int writer = open(path, O_WRONLY | O_NONBLOCK);
    assert(writer >= 0);
    assert(write(writer, "1\n0\n1\0", 6) == 6);
    usleep(20000);
    assert(on_count == 0 && off_count == 0); // No signal-handler dispatch.
    assert(processFIFO(100) == 0);
    assert(on_count == 1 && off_count == 0);
    for (int i = 0; i < 5; ++i) assert(processFIFO(100) == 0);
    assert(on_count == 2 && off_count == 1);
    close(writer);
    assert(processFIFO(10) == 0);
    assert(cleanupFIFO() == 0);
    // Restart after an unclean exit must tolerate a leftover owned FIFO.
    assert(mkfifo(path, 0600) == 0);
    assert(initFIFO(off, on) == 1);
    assert(cleanupFIFO() == 0);
    // Never consume or remove an existing regular file.
    writer = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
    assert(writer >= 0);
    close(writer);
    assert(initFIFO(off, on) == -1);
    assert(lstat(path, &info) == 0 && S_ISREG(info.st_mode));
    assert(unlink(path) == 0);
    std::puts("FIFO regression tests passed");
}
