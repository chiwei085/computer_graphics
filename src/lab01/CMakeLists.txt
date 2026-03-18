add_executable(lab01_triangle triangle.cpp)
target_compile_features(lab01_triangle PRIVATE cxx_std_17)
target_link_libraries(lab01_triangle PRIVATE cg_gl)

add_executable(lab01_rectangle rectangle.cpp)
target_compile_features(lab01_rectangle PRIVATE cxx_std_17)
target_link_libraries(lab01_rectangle PRIVATE cg_gl)

add_executable(lab01_fox fox.cpp)
target_compile_features(lab01_fox PRIVATE cxx_std_17)
target_link_libraries(lab01_fox PRIVATE cg_gl)
