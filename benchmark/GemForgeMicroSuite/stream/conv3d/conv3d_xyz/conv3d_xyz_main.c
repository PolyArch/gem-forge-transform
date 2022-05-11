#ifndef OFFSET_BYTES
#define OFFSET_BYTES 0
#endif

int main(int argc, char *argv[]) {

  int64_t Nx = 256;
  int64_t Ny = 256;
  int64_t Ni = 16;
  int64_t Nn = 64;
  int64_t Kx = 3;
  int64_t Ky = 3;

  int numThreads = 1;
  int check = 0;
  int warm = 0;
  int argx = 2;
  if (argc >= argx) {
    numThreads = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    Nx = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    Ny = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    Ni = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    Nn = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    Kx = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    Ky = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    check = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    warm = atoi(argv[argx - 1]);
  }
  argx++;

  size_t sizeI = Nx * Ny * Ni * sizeof(Value);
  size_t sizeK = Nn * Kx * Ky * Ni * sizeof(Value);
  size_t sizeO = Nx * Ny * Nn * sizeof(Value);

  printf("Number of Threads: %d.\n", numThreads);
  printf("Input %lukB, Weight %lukB, Output %lukB.\n", sizeI / 1024,
         sizeK / 1024, sizeO / 1024);

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  uint64_t totalBytes = sizeI + sizeK + sizeO;
  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  Value *buffer = (Value *)aligned_alloc(PAGE_SIZE, numPages * PAGE_SIZE);

  // Initialize separately so that their physical address is not interleaved
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++) {
    volatile Value v = buffer[i * elementsPerPage];
  }

  Value *I = buffer + 0;
  Value *O = I + Ni * Nx * Ny + (OFFSET_BYTES / sizeof(Value));
  Value *K = O + Nn * Nx * Ny;

  // We pick the dimension as Width-Height-Channel.

  gf_stream_nuca_region("gfm.conv3d.i", I, sizeof(I[0]), Nx, Ny, Ni);
  gf_stream_nuca_region("gfm.conv3d.o", O, sizeof(O[0]), Nx, Ny, Nn);
  gf_stream_nuca_region("gfm.conv3d.k", K, sizeof(K[0]), Kx * Ky * Ni * Nn);
  gf_stream_nuca_align(I, I, 1);
  gf_stream_nuca_align(I, I, Nx);
  gf_stream_nuca_align(I, I, Ny * Nx);
  gf_stream_nuca_align(O, O, 1);
  gf_stream_nuca_align(O, O, Nx);
  gf_stream_nuca_align(O, O, Ny * Nx);
  gf_stream_nuca_remap();

  gf_detail_sim_start();

  if (warm) {
    gf_warm_array("gfm.conv3d.i", I, sizeof(I[0]) * Nx * Ny * Ni);
    gf_warm_array("gfm.conv3d.o", O, sizeof(O[0]) * Nx * Ny * Nn);
    gf_warm_array("gfm.conv3d.k", K, sizeof(K[0]) * Kx * Ky * Ni * Nn);
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = ((uint8_t *)I)[tid];
  }
#endif

  gf_reset_stats();
  volatile Value c = driver(I, K, O, Nx, Ny, Ni, Nn, Kx, Ky);
  gf_detail_sim_end();

  if (check) {
    assert(!check && "Check not implemented yet.");
  }

  return 0;
}
