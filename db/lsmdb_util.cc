//
// Created by 刘文景 on 2021/3/25.
//

#include <cstdio>
#include <string>

namespace lsmdb {}

static void Usage() {
  std::fprintf(stderr,
               "Usage: lsmdbutil command...\n"
               "   dump files...       -- dump contents of specified files\n");
}

int main(int argc, char** argv) {
  bool ok = true;
  if (argc < 2) {
    Usage();
    ok = false;
  } else {
    std::string command = argv[1];
    if (command == "dump") {
    } else {
      Usage();
      ok = false;
    }
  }
  return (ok ? 0 : 1);
}