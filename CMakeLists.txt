project (qpc)

cmake_minimum_required (VERSION 3.1.0)

set(CMAKE_MACOSX_RPATH 1)

include(${WORKSPACE}/WORKSPACE.cmake)

include_directories(".")

# add_compile_options(-stdlib=libc++)
# add_compile_options(-std=c++14)
add_compile_options(-pthread)

# required for libuv
# add_compile_options(-D_LARGEFILE_SOURCE)
# add_compile_options(-D_FILE_OFFSET_BITS=64)

SET( CPP_FILES
    ${PROJECT_SOURCE_DIR}/examples/posix/blinky/blinky.c
    ${PROJECT_SOURCE_DIR}/examples/posix/blinky/bsp.c
    ${PROJECT_SOURCE_DIR}/examples/posix/blinky/main.c
)
SET( H_FILES
    ${PROJECT_SOURCE_DIR}/examples/posix/blinky/blinky.h
    ${PROJECT_SOURCE_DIR}/examples/posix/blinky/bsp.h
)

SET( SOURCE_FILES ${CPP_FILES} ${H_FILES})

SET( BUILD_FILES
  ${PROJECT_SOURCE_DIR}/BUILD
  ${PROJECT_SOURCE_DIR}/PACKAGE.cmake
)

include (${WORKSPACE}/qpc/PACKAGE.cmake)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

add_executable(blinky ${SOURCE_FILES} ${BUILD_FILES})
target_link_libraries(blinky ${QPC_LIB})
target_link_libraries(blinky ${CMAKE_THREAD_LIBS_INIT})

add_custom_target (FORMAT ALL
    /usr/local/bin/clang-format -style=Google -i ${SOURCE_FILES}
)
