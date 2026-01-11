cmake_minimum_required(VERSION 3.24)

project(MycoDBWorkbench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets REQUIRED COMPONENTS core base aui stc)
include(${wxWidgets_USE_FILE})

if (WIN32)
  add_executable(MycoDBWorkbench WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.hpp
    src/MicroSwarmBridge.cpp
    src/MicroSwarmBridge.hpp
  )
else()
  add_executable(MycoDBWorkbench
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.hpp
    src/MicroSwarmBridge.cpp
    src/MicroSwarmBridge.hpp
  )
endif()

target_include_directories(MycoDBWorkbench PRIVATE src)
target_link_libraries(MycoDBWorkbench PRIVATE ${wxWidgets_LIBRARIES})
