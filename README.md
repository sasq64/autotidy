# Autotidy

1. Start with a `.clang-tidy` file that has all checks on ('*')
2. Run autotidy and either fix or ignore each problem.
3. .clang-tidy will be updated with your selected set of ignored checks.

## Quickstart

```
git clone --recurse-submodules https://github.com/sasq64/autotidy.git
CXX=/usr/bin/clang++-7 cmake -S . -B _build
cmake --build _build --config Release -j 4
./_build/tidy --help
```
