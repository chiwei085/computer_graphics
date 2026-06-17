find_package(Freetype REQUIRED)

add_executable(
  lab12_shadow
  src/main.cpp
  src/app.cpp
)
target_compile_features(lab12_shadow PRIVATE cxx_std_17)
target_include_directories(lab12_shadow PRIVATE include)
target_link_libraries(lab12_shadow PRIVATE cg_gl Freetype::Freetype)
