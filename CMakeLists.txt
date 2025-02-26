cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(FUSEEFS LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)

# set(CMAKE_BUILD_TYPE Release)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Boost REQUIRED)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(src)
include_directories(third-party/eRPC/src third-party/eRPC/third_party/asio/include build/third-party/eRPC/src)
include_directories(third-party/flatbuffers/include)
include_directories(third-party/rapidjson/include)


add_subdirectory(src)
add_subdirectory(third-party/eRPC)
add_subdirectory(third-party/flatbuffers)

# add_subdirectory(ycsb-test)
# add_subdirectory(crash-recover-test)
# add_subdirectory(micro-test)

# enable_testing()
# add_subdirectory(tests)

set(FB_SCHEMA
  fs/fbs/mds_request.fbs
)
build_flatbuffers("${FB_SCHEMA}" "" fbschemas "" fs/fbs_h "" "")
add_library(flatbuffers_proto INTERFACE)
add_dependencies(flatbuffers_proto fbschemas)

set (
  FUSEEFS_DEPS
  pthread
  gflags
  numa
  ibverbs
  mlx5
)

# build common
file(GLOB CC_FUSEEFS_COMMON fs/common/*.cc)
add_library(fuseefs_common ${CC_FUSEEFS_COMMON})

# build mds
set(CC_FUSEEFS_MDS_SERVER mds_main.cc)

file(GLOB CC_FUSEEFS_MDS fs/mds/*.cc)
list(REMOVE_ITEM CC_MDS fs/mds/${CC_FUSEEFS_MDS_SERVER})
add_library(fuseefs_mds ${CC_FUSEEFS_MDS})
add_dependencies(fuseefs_mds flatbuffers_proto)
target_compile_definitions(fuseefs_mds PUBLIC -DERPC_INFINIBAND=true)

add_executable(fuseefs_mds_server fs/mds/${CC_FUSEEFS_MDS_SERVER})
target_link_libraries(fuseefs_mds_server fuseefs_mds fuseefs_common flatbuffers_proto erpc::erpc libddckv ${FUSEEFS_DEPS})

# build central
set(CC_FUSEEFS_CENTRAL_SERVER central_main.cc)

file(GLOB CC_FUSEEFS_CENTRAL fs/central/*.cc)
list(REMOVE_ITEM CC_FUSEEFS_CENTRAL fs/central/${CC_FUSEEFS_CENTRAL_SERVER})
add_library(fuseefs_central ${CC_FUSEEFS_CENTRAL})
target_compile_definitions(fuseefs_central PUBLIC -DERPC_INFINIBAND=true)

add_executable(fuseefs_central_server fs/central/${CC_FUSEEFS_CENTRAL_SERVER})
target_link_libraries(fuseefs_central_server fuseefs_central fuseefs_common erpc::erpc ${FUSEEFS_DEPS})

# build pmpool
set(CC_FUSEEFS_PMPOOL_SERVER pmpool_main.cc)
add_executable(fuseefs_pmpool_server fs/pmpool/${CC_FUSEEFS_PMPOOL_SERVER})
target_link_libraries(fuseefs_pmpool_server libddckv ${FUSEEFS_DEPS})

# build client
add_library(fuseefs_client fs/client/posix_wrapper.cc)
target_link_libraries(fuseefs_client erpc::erpc ${FUSEEFS_DEPS})
target_compile_options(fuseefs_client PUBLIC -fPIC)
target_compile_definitions(fuseefs_client PUBLIC -DERPC_INFINIBAND=true)
add_dependencies(fuseefs_client flatbuffers_proto)

add_library(fuseefs_hook SHARED fs/client/hook.c)
target_compile_options(fuseefs_hook PUBLIC -fPIC)
target_link_libraries(fuseefs_hook fuseefs_client fuseefs_common)

# build simple test
add_executable(fuseefs_simple_test fuseefs-test/simple-test/simple-test.cc)