# SPDX-License-Identifier: MIT
#!/bin/bash
LLVM_DIR=bin/llvm

build_llvm() {
  mkdir -p bin/llvm
  pushd bin/llvm
  cmake ../../llvm -DLLVM_TARGETS_TO_BUILD=X86 -DCMAKE_BUILD_TYPE=Release
  make -j$(nproc)
  popd
}

build_llvm
