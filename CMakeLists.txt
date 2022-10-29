cmake_minimum_required(VERSION 3.14)
set(CMAKE_C_STANDARD 11)

set(ENVSDK $ENV{PLAYDATE_SDK_PATH})

if (NOT ${ENVSDK} STREQUAL "")
	# Convert path from Windows
	file(TO_CMAKE_PATH ${ENVSDK} SDK)
else()
	execute_process(
			COMMAND bash -c "egrep '^\\s*SDKRoot' $HOME/.Playdate/config"
			COMMAND head -n 1
			COMMAND cut -c9-
			OUTPUT_VARIABLE SDK
			OUTPUT_STRIP_TRAILING_WHITESPACE
	)
endif()

if (NOT EXISTS ${SDK})
	message(FATAL_ERROR "SDK Path not found; set ENV value PLAYDATE_SDK_PATH")
	return()
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_XCODE_GENERATE_SCHEME TRUE)

# Game Name Customization
set(PLAYDATE_GAME_NAME playing-with-blocks)
set(PLAYDATE_GAME_DEVICE playing-with-blocks_DEVICE)

# Source files
set(GAME_SRC_FILES 
	src/main.c 
	src/asset.c 
	src/game.c 
	src/scenes/board/boardScene.c
	src/scenes/board/matrix.c
	src/scenes/title/titleScene.c
)

project(${PLAYDATE_GAME_NAME} C ASM)

include_directories(${CMAKE_SOURCE_DIR}/src)

if (TOOLCHAIN STREQUAL "armgcc")
	add_executable(${PLAYDATE_GAME_DEVICE} ${SDK}/C_API/buildsupport/setup.c ${GAME_SRC_FILES})
	set(PLAYDATE_TARGET ${PLAYDATE_GAME_DEVICE})
else()
	add_library(${PLAYDATE_GAME_NAME} SHARED ${GAME_SRC_FILES})
	set(PLAYDATE_TARGET ${PLAYDATE_GAME_NAME})
endif()

# Suppress Playdate SDK warnings
target_include_directories(${PLAYDATE_TARGET} SYSTEM PUBLIC ${SDK}/C_API/)

include(${SDK}/C_API/buildsupport/playdate_game.cmake)