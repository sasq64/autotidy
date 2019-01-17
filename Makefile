all : debug

builds/debug :
	cmake -Bbuilds/debug -H. -DCMAKE_BUILD_TYPE=Debug

builds/release :
	cmake -Bbuilds/release -H. -DCMAKE_BUILD_TYPE=Release

compile_commands.json : builds/debug/compile_commands.json
	rm -f compile_commands.json
	ln -s builds/debug/compile_commands.json .

debug : builds/debug compile_commands.json
	make -j12 -C builds/debug

release : builds/release
	make -j12 -C builds/release

runtest : debug
	builds/debug/tidytest

clean :
	rm -rf builds/debug

clean_all :
	rm -rf builds


