CMAKE_MINIMUM_REQUIRED (VERSION 2.8)
PROJECT (cppcms_tmpl_ccpp)
add_definitions(-std=c++11)
SET (cppcms_tmpl_ccpp_SOURCES
	src/expr.cpp
	src/parser_source.cpp
	src/parser.cpp
	src/main.cpp
	src/ast.cpp
	src/errors.cpp
	src/generator.cpp)

add_executable(cppcms_tmpl_ccpp ${cppcms_tmpl_ccpp_SOURCES})
ENABLE_TESTING()
add_test(NAME alltests COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runtests ${CMAKE_CURRENT_BINARY_DIR}/cppcms_tmpl_ccpp)
