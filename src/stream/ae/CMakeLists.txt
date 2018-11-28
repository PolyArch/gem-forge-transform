

add_executable(stream-ae.exe
  AddressEngine.cpp
)
set_target_properties(stream-ae.exe PROPERTIES 
    COMPILE_FLAGS "-std=c++14 -fno-rtti"
)

# Link to LLVM components.
# llvm_config does not work, which is equivalent to llvm_map_components_to_libnames
# and does not expand the dependence.
llvm_map_components_to_libnames(LLVM_LIBS interpreter irreader)
# message(STATUS ${LLVM_LIBS})
llvm_expand_dependencies(LLVM_DEP_LIBS ${LLVM_LIBS})
# message(STATUS ${LLVM_DEP_LIBS})


# Inorder to use the actual header of interpreter, which is not part of the installed llvm headers.
target_include_directories(stream-ae.exe PUBLIC ${LLVM_SRC_LIB_DIR})

target_link_libraries(stream-ae.exe ${LLVM_DEP_LIBS})
