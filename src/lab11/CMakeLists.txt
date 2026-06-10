find_package(Freetype REQUIRED)

add_executable(
  lab11_creative_object
  src/main.cpp
  src/app.cpp
)
target_compile_features(lab11_creative_object PRIVATE cxx_std_17)
target_include_directories(lab11_creative_object PRIVATE include)
target_link_libraries(lab11_creative_object PRIVATE cg_gl Freetype::Freetype)
