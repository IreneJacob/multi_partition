cmake_minimum_required (VERSION 2.8.4)
project (multipartition)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

message(${CMAKE_MODULE_PATH})
# find library
find_package(RocksDB REQUIRED)
find_package(DPDK REQUIRED)
find_package(Readline)

# include header of library
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${ROCKSDB_INCLUDE_DIRS})
include_directories(${DPDK_INCLUDE_DIR})
include_directories(${Readline_INCLUDE_DIR})

# rocksdb flags
set (PLATFORM_LDFLAGS stdc++ pthread rt snappy gflags z bz2 lz4 zstd numa m)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mssse3 -O3 -Wall -Wextra -Wno-unused-parameter -fPIC -g")

set (LIBS ${LIBS} ${ROCKSDB_LIBRARIES} ${DPDK_LIBRARIES} ${Readline_LIBRARY} ${PLATFORM_LDFLAGS})
aux_source_directory (src SRCS) # include every file in src directory
message (STATUS "This is CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS})
message (STATUS "This is CMAKE_SOURCE_DIR " ${CMAKE_SOURCE_DIR})

add_executable(run ${SRCS})
target_link_libraries (run ${LIBS})
set_target_properties (run PROPERTIES COMPILE_FLAGS ${CMAKE_CXX_FLAGS})
