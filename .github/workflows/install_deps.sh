#!/bin/bash

os=$(uname -s)
if [[ $os == Linux ]]; then
  sudo apt-get update
  sudo apt-get install -y ninja-build

  DEFAULT_CLANG_VERSION=$(echo | clang -dM -E - | grep __clang_major | awk '{print $3}')
  CLANG_VERSION=17
  if ((DEFAULT_CLANG_VERSION >= CLANG_VERSION)); then
    echo "Default clang version is $DEFAULT_CLANG_VERSION, which equal or larger than wanted version $CLANG_VERSION. Aborting!"
    exit 1
  fi

  wget https://apt.llvm.org/llvm.sh
  chmod +x llvm.sh
  sudo ./llvm.sh $CLANG_VERSION
  sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-$CLANG_VERSION 100
  sudo update-alternatives --set clang /usr/bin/clang-$CLANG_VERSION

elif [[ $os == Darwin ]]; then
  brew update --quiet
  brew install ninja
fi
