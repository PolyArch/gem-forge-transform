
void foo(unsigned long long ctrl, unsigned long long in, unsigned long long* out) {
  for (unsigned long long i = 0; i < ctrl; ++i) {
    (*out) += in;
    (*out) = ~(*out);
  }
}

void fake(int x);

int main(int argc, char* argv[]) {
    unsigned long long out = 0;
    foo(0x1000 * TDG_WORKLOAD_SIZE, argc, &out);
    // Avoid optimize foo away.
    fake(out);
    return 0;
}
