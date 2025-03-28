# PO3's file.
cmake_minimum_required(VERSION 3.20)
set(NAME "ALYSLC" CACHE STRING "")
set(VERSION 1.0.0 CACHE STRING "")

# ---- Options ----

option(COPY_BUILD "Copy the build output to the mod's plugin directory." TRUE)
option(BUILD_SKYRIMVR "Build for Skyrim VR" OFF)
option(BUILD_SKYRIMAE "Build for Skyrim AE" OFF)

# ---- Cache build vars ----

macro(set_from_environment VARIABLE)
	if (NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif ()
endmacro()

macro(find_commonlib_path)
	if (CommonLibName AND NOT ${CommonLibName} STREQUAL "")
		# Check extern
		find_path(CommonLibPath
		include/REL/Relocation.h
		PATHS extern/${CommonLibName})
		if (${CommonLibPath} STREQUAL "CommonLibPath-NOTFOUND")
			#Check path
			set_from_environment(${CommonLibName}Path)
			set(CommonLibPath ${${CommonLibName}Path})
		endif()
	endif()
endmacro()

set_from_environment(VCPKG_ROOT)
if(BUILD_SKYRIMAE)
	add_compile_definitions(SKYRIM_AE)
	add_compile_definitions(SKYRIM_SUPPORT_AE)
	set(CommonLibName "CommonLibSSE")
	set_from_environment(SkyrimAEPath)
	set(SkyrimPath ${SkyrimAEPath})
	set(SkyrimVersion "Skyrim AE")
	if (NOT DEFINED ALYSLCPluginPath AND DEFINED ENV{ALYSLCPluginPathAE})
		set(ALYSLCPluginPath $ENV{ALYSLCPluginPathAE})
	else()
		message(
			WARNING
			"Variable ALYSLCPluginPathAE is not set. Will not copy plugin to directory after building."
		)
	endif()
	set(NAME "ALYSLC")
elseif(BUILD_SKYRIMVR)
	add_compile_definitions(SKYRIMVR)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	set(CommonLibName "CommonLibVR")
	set_from_environment(SkyrimVRPath)
	set(SkyrimPath ${SkyrimVRPath})
	set(SkyrimVersion "Skyrim VR")
else()
	set(CommonLibName "CommonLibSSE")
	set_from_environment(Skyrim64Path)
	set(SkyrimPath ${Skyrim64Path})
	set(SkyrimVersion "Skyrim SSE")
	if (NOT DEFINED ALYSLCPluginPath AND DEFINED ENV{ALYSLCPluginPathSE})
		set(ALYSLCPluginPath $ENV{ALYSLCPluginPathSE})
	else()
		message(
			WARNING
			"Variable ALYSLCPluginPathSE is not set. Will not copy plugin to directory after building."
		)
	endif()
	set(NAME "ALYSLC")
endif()
find_commonlib_path()

message(
	STATUS
	"Building ${NAME} ${VERSION} for ${SkyrimVersion} at ${SkyrimPath} with ${CommonLibName} at ${CommonLibPath}."
)

if (DEFINED VCPKG_ROOT)
	set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
	set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
else ()
	message(
		WARNING
		"Variable VCPKG_ROOT is not set. Continuing without vcpkg."
	)
endif ()

set(Boost_USE_STATIC_RUNTIME OFF CACHE BOOL "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")

# ---- Project ----

project(
	${NAME}
	VERSION ${VERSION}
	LANGUAGES CXX
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in
	${CMAKE_CURRENT_BINARY_DIR}/cmake/Version.h
	@ONLY
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/cmake/Version.rc
	@ONLY
)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(
		FATAL_ERROR
			"In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
	)
endif()

# ---- Globals ----

if (MSVC)
	if (NOT ${CMAKE_GENERATOR} STREQUAL "Ninja")
		add_compile_options(
			/MP	# Build with Multiple Processes
		)
	endif ()
endif ()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

set(Boost_USE_STATIC_LIBS ON)

# ---- Dependencies ----

if (DEFINED CommonLibPath AND NOT ${CommonLibPath} STREQUAL "" AND IS_DIRECTORY ${CommonLibPath})
	add_subdirectory(${CommonLibPath} ${CommonLibName})
else ()
	message(
		FATAL_ERROR
		"Variable ${CommonLibName}Path is not set or in extern/."
	)
endif()

# ---- Add source files ----

# Full credits to doodlum:
# https://github.com/doodlum/skyrim-community-shaders/blob/dev/cmake/AddCXXFiles.cmake
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
function(add_cxx_files TARGET)
	# Group headers in include dir.
	file(GLOB_RECURSE INCLUDE_FILES
		LIST_DIRECTORIES false
		CONFIGURE_DEPENDS
		"include/*.h"
		"include/*.hpp"
		"include/*.hxx"
		"include/*.inl"
	)

	source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include
		PREFIX "Header Files"
		FILES ${INCLUDE_FILES})

	target_sources("${TARGET}" PUBLIC ${INCLUDE_FILES})
	
	# Group source files in src dir.
	file(GLOB_RECURSE SOURCE_FILES
		LIST_DIRECTORIES false
		CONFIGURE_DEPENDS
		"src/*.cpp"
		"src/*.cxx"
	)

	source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src
		PREFIX "Source Files"
		FILES ${SOURCE_FILES})

	target_sources("${TARGET}" PRIVATE ${SOURCE_FILES})
	
	# Copy over version info.
	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in
		${CMAKE_CURRENT_BINARY_DIR}/cmake/Version.h
		@ONLY
	)

	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.rc.in
		${CMAKE_CURRENT_BINARY_DIR}/cmake/Version.rc
		@ONLY
	)

	target_sources(
		"${TARGET}"
		PRIVATE
			${CMAKE_CURRENT_BINARY_DIR}/cmake/Version.h
			${CMAKE_CURRENT_BINARY_DIR}/cmake/Version.rc
			.clang-format
			.editorconfig
	)
endfunction()

add_library("${PROJECT_NAME}" SHARED)
add_cxx_files("${PROJECT_NAME}")

# ---- Create DLL ----

target_compile_features(
	"${PROJECT_NAME}"
	PRIVATE
		cxx_std_23
)

target_compile_definitions(
	${PROJECT_NAME}
	PRIVATE
		_UNICODE
)

target_precompile_headers(
	"${PROJECT_NAME}"
	PRIVATE
		include/PCH.h
)

target_include_directories(
	${PROJECT_NAME}
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/cmake
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Other packages.
find_path(SIMPLEINI_INCLUDE_DIRS "ConvertUTF.c")
target_include_directories(${PROJECT_NAME} PRIVATE ${SIMPLEINI_INCLUDE_DIRS})
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		${CommonLibName}::${CommonLibName}
		glm::glm
		spdlog::spdlog
)

if(COPY_BUILD)
	if(DEFINED ALYSLCPluginPath)
		add_custom_command(
			TARGET ${PROJECT_NAME}
			PRE_BUILD
			COMMAND ${CMAKE_COMMAND} -E rm -rf ${ALYSLCPluginPath}
		)
	else()
		message(
			WARNING
			"Variable ${ALYSLCPluginPath} is not defined. Skipping pre-build cleanup."
		)
	endif()
endif()

# Compile options for the project.
if (MSVC)
	target_compile_options(
		${PROJECT_NAME}
		PUBLIC
			"$<$<CONFIG:RELEASE>:/Zi>"
		PRIVATE
			"/sdl"				# Enable Additional Security Checks
			"/utf-8"			# Set Source and Executable character sets to UTF-8
			"/Zi"				# Debug Information Format
			"/W1"				# Warning Level
			"/WX-"				# Treat Warnings As Errors
			"/permissive-"		# Standards conformance
			"/Zc:preprocessor"	# Enable preprocessor conformance mode

			"/wd4200" 			# nonstandard extension used : zero-sized array in struct/union

			#"$<$<CONFIG:DEBUG>:>"
			"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC->"
	)

	target_link_options(
		${PROJECT_NAME}
		PRIVATE
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif ()

# ---- Post build ----

if(COPY_BUILD)
	if(DEFINED ALYSLCPluginPath)
		add_custom_command(
			TARGET ${PROJECT_NAME}
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E rm -rf ${ALYSLCPluginPath}
			COMMAND ${CMAKE_COMMAND} -E make_directory ${ALYSLCPluginPath}
			COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${ALYSLCPluginPath}
			COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> ${ALYSLCPluginPath}
		)
	else()
		message(
			WARNING
			"Variable ${ALYSLCPluginPath} is not defined. Skipping post-build copy command."
		)
	endif()
endif()