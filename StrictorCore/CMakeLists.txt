add_library(StrictorCore STATIC
	Source/App/Request.cpp
	Source/App/Request.h
	Source/App/Runner.cpp
	Source/App/Runner.h
	Source/Clang/ClangFormat.cpp
	Source/Clang/ClangFormat.h
	Source/Clang/ClangString.cpp
	Source/Clang/ClangString.h
	Source/Clang/CompileDatabase.cpp
	Source/Clang/CompileDatabase.h
	Source/Clang/SyntaxTree.cpp
	Source/Clang/SyntaxTree.h
	Source/Clang/Toolchain.cpp
	Source/Clang/Toolchain.h
	Source/Clang/TranslationUnit.cpp
	Source/Clang/TranslationUnit.h
	Source/Format/Formatter.cpp
	Source/Format/Formatter.h
	Source/Format/LineDelta.cpp
	Source/Format/LineDelta.h
	Source/Lua/Script.cpp
	Source/Lua/Script.h
	Source/Rules/Design.cpp
	Source/Rules/Design.h
	Source/Rules/Diagnostic.h
	Source/Rules/Layout.cpp
	Source/Rules/Layout.h
	Source/Rules/Naming.cpp
	Source/Rules/Naming.h
	Source/Rules/NameStyle.cpp
	Source/Rules/NameStyle.h
	Source/Rules/Properties.h
	Source/Rules/SourceText.cpp
	Source/Rules/SourceText.h
	Source/Style/Cascade.cpp
	Source/Style/Cascade.h
	Source/Style/Node.h
	Source/Style/Stylesheet.cpp
	Source/Style/Stylesheet.h
	Source/System/Git.cpp
	Source/System/Git.h
	Source/System/Process.cpp
	Source/System/Process.h
	Source/System/Text.cpp
	Source/System/Text.h
)

target_compile_features(StrictorCore PUBLIC cxx_std_23)
target_compile_definitions(StrictorCore PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX SOL_PRINT_ERRORS=0)
target_compile_options(StrictorCore PUBLIC /W4 /sdl /permissive- /utf-8 /bigobj /external:W0)
target_include_directories(StrictorCore PUBLIC Source)
target_include_directories(StrictorCore SYSTEM PUBLIC "${LLVM_PATH}/include" ${LUA_INCLUDE_DIR})
target_link_libraries(StrictorCore PUBLIC "${LLVM_PATH}/lib/libclang.lib" ${LUA_LIBRARIES} sol2::sol2 delayimp shell32 ole32)
target_link_options(StrictorCore INTERFACE /DELAYLOAD:libclang.dll)
