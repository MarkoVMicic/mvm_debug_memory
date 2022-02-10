@echo off

set CommonCompilerFlags= -MTd -Gm- -GR- -WX -nologo -Od -Oi -Zi -DMVM_DEBUG_MEMORY=1
set CommonLinkerFlags=/INCREMENTAL:NO 

set CompiledFiles=..\mvm_debug_memory_test.c

IF NOT EXIST .\build mkdir .\build
pushd .\build
del *.pdb > NUL 2> NUL
cl %CommonCompilerFlags% %CompiledFiles% /link %CommonLinkerFlags% 
popd
