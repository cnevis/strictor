add_executable(StrictorTests
	Source/App/CommandLineTests.cpp
	Source/App/RequestTests.cpp
	Source/Clang/ClangFormatTests.cpp
	Source/Clang/CompileDatabaseTests.cpp
	Source/Clang/TranslationUnitTests.cpp
	Source/Format/FormatterTests.cpp
	Source/Format/LineDeltaTests.cpp
	Source/Lua/ScriptTests.cpp
	Source/Rules/DesignTests.cpp
	Source/Rules/LayoutTests.cpp
	Source/Rules/NameStyleTests.cpp
	Source/Rules/NamingTests.cpp
	Source/Style/CascadeTests.cpp
	Source/Style/StylesheetTests.cpp
	Source/Support/Fixtures.cpp
	Source/Support/Fixtures.h
	Source/Support/TempDir.cpp
	Source/Support/TempDir.h
	Source/Support/WorkingDir.cpp
	Source/Support/WorkingDir.h
	Source/System/GitTests.cpp
	Source/System/ProcessTests.cpp
	Source/System/TextTests.cpp
)

target_include_directories(StrictorTests PRIVATE Source)
target_link_libraries(StrictorTests PRIVATE StrictorCore Catch2::Catch2WithMain)
add_dependencies(StrictorTests strictor)
