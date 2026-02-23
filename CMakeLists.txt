cmake_minimum_required(VERSION 3.21)
project(qtconsole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Network WebSockets)

add_executable(qtconsole
  src/main.cc
  src/main_window.cc
  src/measurement_model.cc
  src/numeric_ratio_widget.cc
  src/time_series_widget.cc
  src/statistics_widget.cc
  src/receiver_control_widget.cc
  src/data_receiver.cc
)

target_include_directories(qtconsole PRIVATE src)

target_link_libraries(qtconsole PRIVATE
  Qt6::Widgets
  Qt6::Charts
  Qt6::Network
  Qt6::WebSockets
)
