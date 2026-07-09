@echo off

set cc=cl
set opts=-Zi -std:c++23preview -nologo -Oi -Od /EHsc -W4 -fsanitize=address -w14062 -w14388
set libs=kernel32.lib
set my_include=-I../code/

pushd build
@echo on
%cc% %opts% ../unity.cpp %my_include% -Fe:compiler.exe %libs%
@echo off
popd