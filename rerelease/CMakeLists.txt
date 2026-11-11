cmake_minimum_required(VERSION 3.24)
project(q2rpg_game CXX)

# Builds game_x64.dll for the Quake 2 rerelease.
# Linux: cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake

option(Q2_USE_STD_FORMAT "Use C++20 std::format instead of the fmt library (stock custom formatters fail with libstdc++)" OFF)
set(Q2_GAME_DIR "$ENV{HOME}/.local/share/Steam/steamapps/common/Quake 2/rerelease"
	CACHE PATH "Quake 2 rerelease install dir (contains baseq2)")
set(Q2_MOD_NAME "q2rpg" CACHE STRING "Mod game dir name")

include(FetchContent)

set(JSONCPP_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_PKGCONFIG_SUPPORT OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_CMAKE_PACKAGE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_OBJECT_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(jsoncpp
	GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
	GIT_TAG 1.9.6
	GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(jsoncpp)

if(NOT Q2_USE_STD_FORMAT)
	FetchContent_Declare(fmt
		GIT_REPOSITORY https://github.com/fmtlib/fmt.git
		GIT_TAG 10.2.1
		GIT_SHALLOW TRUE)
	FetchContent_MakeAvailable(fmt)
endif()

file(GLOB GAME_SOURCES CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bots/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ctf/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/rogue/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/xatrix/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/q2rpg/*.cpp)

add_library(game SHARED ${GAME_SOURCES})
set_target_properties(game PROPERTIES
	OUTPUT_NAME game_x64
	PREFIX ""
	CXX_EXTENSIONS OFF)
target_include_directories(game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(game PRIVATE
	KEX_Q2_GAME KEX_Q2GAME_EXPORTS NO_FMT_SOURCE KEX_Q2GAME_DYNAMIC _CRT_SECURE_NO_WARNINGS)

# q2rpg: mod version from the repo-root VERSION file (edit it to bump; cmake re-runs on change)
set(Q2RPG_VERSION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${Q2RPG_VERSION_FILE})
file(STRINGS ${Q2RPG_VERSION_FILE} Q2RPG_VERSION LIMIT_COUNT 1)
target_compile_definitions(game PRIVATE Q2RPG_VERSION="${Q2RPG_VERSION}")

if(Q2_USE_STD_FORMAT)
	target_compile_features(game PRIVATE cxx_std_20)
	target_compile_definitions(game PRIVATE USE_CPP20_FORMAT=1)
	target_link_libraries(game PRIVATE jsoncpp_static)
else()
	# match the MSVC project (stdcpp17); GCC 16 defaults to C++20
	set_target_properties(game PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
	target_link_libraries(game PRIVATE jsoncpp_static fmt::fmt)
endif()

if(MINGW)
	# No MinGW runtime DLLs needed next to the game DLL.
	target_link_options(game PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--kill-at $<$<CONFIG:Release>:-s>)
	target_compile_options(game PRIVATE -Wno-unknown-pragmas -Wno-multichar
		-include ${CMAKE_CURRENT_SOURCE_DIR}/q2rpg/mingw_compat.h)
endif()

add_custom_target(deploy
	COMMAND ${CMAKE_COMMAND} -E make_directory "${Q2_GAME_DIR}/${Q2_MOD_NAME}"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:game> "${Q2_GAME_DIR}/${Q2_MOD_NAME}/"
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../mod/${Q2_MOD_NAME}" "${Q2_GAME_DIR}/${Q2_MOD_NAME}"
	DEPENDS game
	COMMENT "Deploying to ${Q2_GAME_DIR}/${Q2_MOD_NAME}")
