#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/ioctl.h>

static const char* LLVM_TRACE_CPU_FILE = "/dev/llvm_trace_cpu";

static const unsigned long REQUEST_REPLAY = 0;

static const uint64_t MAX_NUM_MAPPED_ARGS = 10;

void replay(const char* trace, uint64_t num_maps, ...) {
  
  va_list args;
  va_start(args, num_maps * 2);
  
  if (num_maps > MAX_NUM_MAPPED_ARGS) {
    exit(1);
  }
  
  int flags = 2048;
  int fd = open(LLVM_TRACE_CPU_FILE, flags);
  volatile char finished_tag = 0;
  uint64_t io_args[MAX_NUM_MAPPED_ARGS * 2 + 3];
  io_args[0] = (uint64_t)trace;
  io_args[1] = (uint64_t)&finished_tag;
  io_args[2] = (uint64_t)num_maps;
  for (uint64_t i = 0; i < num_maps; ++i) {
    io_args[i * 2 + 3] = va_arg(args, uint64_t);
    io_args[i * 2 + 4] = va_arg(args, uint64_t);
  }
  va_end(args);

  // The system will suspend this thread until
  // replay is done.
  // No more need to spin, which introduces a lot of overhead
  // to read the memory.
  ioctl(fd, REQUEST_REPLAY, io_args);

}