#define FFT_SIZE 1024
#define twoPI 6.28318530717959

////////////////////////////////////////////////////////////////////////////////
// Test harness interface code.

struct bench_args_t {
  double real[FFT_SIZE];
  double img[FFT_SIZE];
  double real_twid[FFT_SIZE / 2];
  double img_twid[FFT_SIZE / 2];
};

void m5_llvm_trace_map(const char* base, void* vaddr);
void m5_llvm_trace_replay(const char* trace, void* vaddr);

const char* trace = "fft";
const char* abase = "real";
const char* bbase = "img";
const char* cbase = "real_twid";
const char* dbase = "img_twid";

void fft(double real[FFT_SIZE], double img[FFT_SIZE],
         double real_twid[FFT_SIZE / 2], double img_twid[FFT_SIZE / 2]) {
  m5_llvm_trace_map(abase, real);
  m5_llvm_trace_map(bbase, img);
  m5_llvm_trace_map(cbase, real_twid);
  m5_llvm_trace_map(dbase, img_twid);
  int ret = 0;
  m5_llvm_trace_replay(trace, &ret);
}

static struct bench_args_t args;
int main() {
  fft(args.real, args.img, args.real_twid, args.img_twid);
  return 0;
}