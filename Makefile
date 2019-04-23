
CMAKE_FLAGS = -Wno-deprecated
#BUILD_SYS = -GNinja
BUILD_SYS = -G"Unix Makefiles"

all : submods debug

submods :
	git submodule update --init

builds/debug/cmake_install.cmake :
	rm -rf builds/debug
	cmake -Bbuilds/debug -H. $(BUILD_SYS) $(CMAKE_FLAGS) -DCMAKE_BUILD_TYPE=Debug

builds/release/cmake_install.cmake :
	rm -rf builds/release
	cmake -Bbuilds/release -H. $(BUILD_SYS) $(CMAKE_FLAGS) -DCMAKE_BUILD_TYPE=Release

compile_commands.json : builds/debug/compile_commands.json
	rm -f compile_commands.json
	ln -s builds/debug/compile_commands.json .

debug : builds/debug/cmake_install.cmake compile_commands.json
	cmake --build builds/debug -- -j8

release : builds/release/cmake_install.cmake compile_commands.json
	cmake --build builds/release -- -j8

clean :
	rm -rf builds/debug

clean_all :
	rm -rf builds

