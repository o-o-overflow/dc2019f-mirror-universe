cmake_minimum_required(VERSION 3.9)
project(usim C)
set(CMAKE_C_STANDARD 11)

find_package(X11 REQUIRED)
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

add_definitions(-DVERSION="0.9-ams")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(${X11_INCLUDE_DIR})
link_directories(${X11_LIBRARIES})

add_executable(usim usim.c ucode.c mem.c iob.c mouse.c kbd.c tv.c x11.c chaos.c disk.c ini.c ucfg.c trace.c disass.c syms.c misc.c)
target_link_libraries(usim ${X11_LIBRARIES})

add_executable(readmcr readmcr.c disass.c misc.c syms.c)
add_executable(diskmaker diskmaker.c misc.c)
add_executable(lmfs lmfs.c misc.c)
add_executable(lod lod.c disass.c misc.c syms.c)

bison_target(ccy ccy.y ${CMAKE_CURRENT_BINARY_DIR}/ccy.c)
flex_target(ccl ccl.l  ${CMAKE_CURRENT_BINARY_DIR}/ccl.c COMPILE_FLAGS -d)
add_flex_bison_dependency(ccl ccy)
include_directories(${FLEX_INCLUDE_DIRS})
add_executable(cc ${BISON_ccy_OUTPUTS} ${FLEX_ccl_OUTPUTS} cc.c disass.c lcadrd.c misc.c syms.c)
