file(GLOB_RECURSE SOURCES Utils.cpp PrefixSumKernel.cpp)
add_library(radixsort STATIC ${SOURCES})
target_include_directories(radixsort PUBLIC .)

target_link_libraries(radixsort PUBLIC dawn::webgpu_dawn)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
