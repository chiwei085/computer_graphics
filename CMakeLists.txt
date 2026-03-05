cmake_minimum_required(VERSION 3.16)

project(ComputerGraphicsLab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenGL REQUIRED)
find_package(FreeGLUT CONFIG QUIET)
if(NOT FreeGLUT_FOUND)
  find_package(GLUT REQUIRED)
endif()

add_library(cg_gl INTERFACE)
target_link_libraries(cg_gl INTERFACE OpenGL::GL)

if(TARGET GLUT::GLUT)
  target_link_libraries(cg_gl INTERFACE GLUT::GLUT)
elseif(TARGET FreeGLUT::freeglut)
  target_link_libraries(cg_gl INTERFACE FreeGLUT::freeglut)
else()
  message(FATAL_ERROR "No GLUT target found (GLUT::GLUT or FreeGLUT::freeglut)")
endif()

if(TARGET OpenGL::GLU)
  target_link_libraries(cg_gl INTERFACE OpenGL::GLU)
elseif(WIN32)
  target_link_libraries(cg_gl INTERFACE glu32)
else()
  target_link_libraries(cg_gl INTERFACE GLU)
endif()

file(GLOB LAB_DIRS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/lab*")

foreach(LAB_DIR IN LISTS LAB_DIRS)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/${LAB_DIR}/CMakeLists.txt")
    add_subdirectory("src/${LAB_DIR}")
  endif()
endforeach()
