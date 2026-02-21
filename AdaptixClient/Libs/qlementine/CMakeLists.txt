project("qlementine"
  LANGUAGES CXX
  VERSION 1.4.2.0
  DESCRIPTION "Modern QStyle for desktop Qt applications."
)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg)

add_subdirectory(lib)
