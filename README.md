# Autotidy

Simplifies running clang-tidy on your code.

![Screenshot](/extra/screenshot.png?raw=true "Screenshot")

### Building

```
git submodule update --init
make
sudo cp builds/debug/autotidy /usr/local/bin/
```

### Using

Make sure you have a _compile_commands.json_ for your project. Then;

```
autotidy -s myfile.cpp 
```

This will create a _.clang-tidy_ file with all checks turned on if you
don't have one already.

Now you get the following options for each found issue;
```
[a] = Apply the shown patch, if this issue has a Fix
[i] = Ignore this check, add it to list of ignored checks in .clang-tidy
[s] = Skip this issue
[S] = Skip all issues in this file
[n] = Add a NOLINT comment to the line where the issue appears
[N] = As above, but only for the current check
[d] = Show documentation on the current check
[t] = Add a TODO comment to the line where the issue appears
[q] = Quit autotidy
```
