cmake_minimum_required(VERSION 3.21)
project(qtconsole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Network WebSockets)

set(APP_WIN_RESOURCE)
if(WIN32)
  set(APP_WIN_RESOURCE windows/app_icon.rc)
endif()

add_executable(qtconsole
  ${APP_WIN_RESOURCE}
  src/main.cc
  src/app_icon.cc
  src/main_window.cc
  src/measurement_model.cc
  src/numeric_ratio_widget.cc
  src/title_line_widget.cc
  src/time_series_widget.cc
  src/statistics_widget.cc
  src/receiver_control_widget.cc
  src/data_receiver.cc
)

set_target_properties(qtconsole PROPERTIES
  WIN32_EXECUTABLE ON
)

target_include_directories(qtconsole PRIVATE src third_party/exprtk)

# exprtk compiles into a very large translation unit; MSVC needs /bigobj.
if(MSVC)
  set_source_files_properties(src/measurement_model.cc PROPERTIES COMPILE_OPTIONS "/bigobj")
endif()

target_link_libraries(qtconsole PRIVATE
  Qt6::Widgets
  Qt6::Charts
  Qt6::Network
  Qt6::WebSockets
)
