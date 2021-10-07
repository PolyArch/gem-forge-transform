# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
message(STATUS ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
                 ${CMAKE_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${gtest_SOURCE_DIR}/include")
endif()

# Add the TestDir macro.
add_definitions(-DTestDir="${CMAKE_CURRENT_BINARY_DIR}")

# Now simply link against gtest or gtest_main as needed. Eg
add_executable(GemForgeTest
  TestSample.cpp
  TestSetup.cpp
  ReplayPassTestFixture.cpp
  stream/StreamPassTestFixture.cpp
  stream/StepInstDetectionTest.cpp
  stream/StreamTransformPlanTest.cpp
  stream/StreamInstDependenceTest.cpp
  stream/StreamCoalesceTest.cpp
  loop_unroller/UnrollDependenceTest.cpp
  adfa/ADFADependenceTest.cpp
)

# target_compile_definitions(GemForgeTest
#   PRIVATE TestDir="${CMAKE_CURRENT_SOURCE_DIR}"
# )
target_include_directories(GemForgeTest
    PUBLIC ${CMAKE_BINARY_DIR}/src/trace
    PUBLIC ${CMAKE_BINARY_DIR}/src
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(GemForgeTest PROPERTIES
  COMPILE_FLAGS "-std=c++14 -fno-rtti -DGOOGLE_PROTOBUF_NO_RTTI -DGTEST_HAS_RTTI=0 -Wno-init-list-lifetime"
)

llvm_map_components_to_libnames(LLVM_LIBS demangle irreader)
llvm_expand_dependencies(LLVM_DEP_LIBS ${LLVM_LIBS})

target_link_libraries(GemForgeTest LLVMTDGPass ${LLVM_DEP_LIBS} gtest_main)

# Copy all the test cases to the binary folder.
file(GLOB TestInputFiles ${CMAKE_CURRENT_SOURCE_DIR}/*/inputs/*)
# message(STATUS ${TestInputFiles})
foreach(TestInputFile ${TestInputFiles})
  file(RELATIVE_PATH RelativePath ${CMAKE_CURRENT_SOURCE_DIR} ${TestInputFile})
  # message(STATUS ${RelativePath})
  configure_file(${TestInputFile} ${CMAKE_CURRENT_BINARY_DIR}/${RelativePath} COPYONLY)
endforeach(TestInputFile ${TestInputFiles})
