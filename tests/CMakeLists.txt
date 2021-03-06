include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(
    libtest 
    src/sptree.cpp
    src/tsne.cpp
    src/interpolate.cpp
    src/gaussian.cpp
    src/symmetrize.cpp
    src/utils.cpp
)

target_link_libraries(
    libtest
    gtest_main
    knncolle
    qdtsne
)

# Test the custom parallelization capability.
add_executable(
    cuspartest 
    src/tsne.cpp
)

target_link_libraries(
    cuspartest
    gtest_main
    knncolle
    qdtsne
)

target_compile_definitions(cuspartest PRIVATE CUSTOM_PARALLEL_TEST=1)

set(CODE_COVERAGE "Enable coverage testing" OFF)
if(CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(libtest PRIVATE -O0 -g --coverage)
    target_link_options(libtest PRIVATE --coverage)
endif()

set(USE_OPENMP OFF CACHE BOOL "Compile with OpenMP support")
if (USE_OPENMP)
    find_package(OpenMP)
    target_link_libraries(libtest OpenMP::OpenMP_CXX)
endif()

include(GoogleTest)
gtest_discover_tests(libtest)
gtest_discover_tests(cuspartest)
