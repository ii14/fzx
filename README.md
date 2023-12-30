# fzx

A fuzzy finder, based on [fzy](https://github.com/jhawthorn/fzy).

- Streaming input
- Improved performance, 2x-3x faster than fzy on x86_64 processors
- **Work in progress**, things are subject to change

## Build

### Neovim plugin

```sh
cmake -B build
cmake --build build --target install-plugin
```
Add the local repository to your runtime path (or install with your plugin manager) and
run `:Fzx files`.

### TUI

TUI is still under development, and is disabled in the build system by default.
```sh
cmake -B build -D FZX_BUILD_EXECUTABLE=ON
cmake --build build
ls | ./build/bin/fzx
```
