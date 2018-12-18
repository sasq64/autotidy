# Autotidy

### Building

`git submodule update --init`
`make`

### Using

```
clang-tidy -dump-config > .clang-tidy
```
* Edit _.clang-tidy_ so that  `Checks:` are set to `'*'` (all checks enabled).
* Make sure you have a _compile_commands.json_ for your project.
* Assumes your sources are in _src/_

```
clang-tidy -export-fixes=fixes.yaml src/*.cpp > tidy.log
builds/debug/autotidy -f fixes.yaml tidy.log
```

Now you can _Ignore_ checks you don't want for your project, _Apply_ fixes if there
are any, _Edit_ the problem or add a `//NOLINT` comment.

