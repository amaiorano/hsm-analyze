cmake_minimum_required(VERSION 3.4.3)
project(hsm-analyze)

option(USE_RELEASE_LIBS_IN_DEBUG
	"Set to ON to link against release llvm/clang libs in Debug." OFF)

if (USE_RELEASE_LIBS_IN_DEBUG)
	set(LLVM_USE_CRT_DEBUG "MD")
	add_compile_options(/U_DEBUG)
endif()

# Find ClangConfig.cmake which will find_package(LLVM)
# On Windows: -DCMAKE_PREFIX_PATH="C:\Program Files\LLVM\lib\cmake\clang"
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using ClangConfig.cmake in: ${CLANG_CMAKE_DIR}")

# HandleLLVMOptions sets up compilation flags (disable warnings, etc.)
list(APPEND CMAKE_MODULE_PATH ${LLVM_DIR})
include(HandleLLVMOptions)

add_executable(hsm-analyze HsmAnalyze.cpp)
include_directories(${LLVM_INCLUDE_DIRS})
target_link_libraries(hsm-analyze PRIVATE clangTooling)