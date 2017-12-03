int foo(int ctrl, int in) {
  for (int i = 0; i < ctrl; ++i) {
    in++;
  }
  return in;
}

int main(int argc, char* argv[]) {
    return foo(10, argc);
}
