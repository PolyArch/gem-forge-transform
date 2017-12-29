#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>

static const char* LLVM_TRACE_CPU_FILE = "/dev/llvm_trace_cpu";

static const unsigned long REQUEST_MAP = 0;
static const unsigned long REQUEST_REPLAY = 1;

void mapVirtualAddr(const char* name, const void* vaddr) {
    int flags = 2048;
    int fd = open(LLVM_TRACE_CPU_FILE, flags);
    uint64_t args[2];
    args[0] = (uint64_t)name;
    args[1] = (uint64_t)vaddr;
    ioctl(fd, REQUEST_MAP, args);
}

void replay(const char* trace) {
    int flags = 2048;
    int fd = open(LLVM_TRACE_CPU_FILE, flags);
    volatile char finished_tag = 0;
    uint64_t args[2];
    args[0] = (uint64_t)trace;
    args[1] = (uint64_t)&finished_tag;
    ioctl(fd, REQUEST_REPLAY, args);
    // Spin until it finished.
    while (finished_tag == 0) {
        // printf("Replaying %s\n", trace);
    }
    // printf("Finished replay %s\n", trace);
}