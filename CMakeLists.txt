if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.18)
endif()

project(ptz-controller VERSION 0.1.0)
set(PROJECT_FULL_NAME "PTZ Controller")

set(MACOS_PACKAGE_UUID "D7E1F2A3-4B5C-4D6E-8F90-1A2B3C4D5E6F")
set(MACOS_INSTALLER_UUID "E8F2A3B4-5C6D-4E7F-9012-2B3C4D5E6F70")

add_library(${PROJECT_NAME} MODULE)

configure_file(src/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h)

set(CMAKE_AUTOMOC ON)

target_sources(${PROJECT_NAME} PRIVATE
  src/plugin-main.cpp
  src/ptz-device.hpp
  src/visca-ip.cpp
  src/visca-ip.hpp
  src/ptz-probe.cpp
  src/ptz-probe.hpp
  src/ptz-manager.cpp
  src/ptz-manager.hpp
  src/ndi-runtime.cpp
  src/ndi-runtime.hpp
  src/ndi-device.cpp
  src/ndi-device.hpp
  src/hybrid-device.cpp
  src/hybrid-device.hpp
  src/dock.cpp
  src/dock.hpp)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# NDI SDK headers (runtime is dlopen'd, not linked).
find_path(NDI_INCLUDE_DIR Processing.NDI.Lib.h PATHS "/Library/NDI SDK for Apple/include" "/usr/local/include")
if(NDI_INCLUDE_DIR)
  target_include_directories(${PROJECT_NAME} PRIVATE ${NDI_INCLUDE_DIR})
endif()
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

if(BUILD_OUT_OF_TREE)
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)
  include(cmake/ObsPluginHelpers.cmake)
  set(OBS_FRONTEND_API_NAME "obs-frontend-api")
else()
  set(OBS_FRONTEND_API_NAME "frontend-api")
endif()

find_package(Qt6 COMPONENTS Widgets Core Gui Network REQUIRED)
target_link_libraries(${PROJECT_NAME} Qt6::Widgets Qt6::Core Qt6::Gui Qt6::Network)
set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC ON)

if(OS_MACOS)
  set(MACOS_BUNDLEID "com.dcap.${PROJECT_NAME}")
  set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
  set(MACOSX_PLUGIN_BUNDLE_VERSION "${PROJECT_VERSION}")
  set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
endif()

target_link_libraries(${PROJECT_NAME}
  OBS::${OBS_FRONTEND_API_NAME}
  OBS::libobs)

if(BUILD_OUT_OF_TREE)
  if(NOT LIB_OUT_DIR)
    set(LIB_OUT_DIR "/lib/obs-plugins")
  endif()
  if(NOT DATA_OUT_DIR)
    set(DATA_OUT_DIR "/share/obs/obs-plugins/${PROJECT_NAME}")
  endif()
  set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
  install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
  install(DIRECTORY data/locale DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
  setup_plugin_target(${PROJECT_NAME})
else()
  target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
  if(OBS_CMAKE_VERSION VERSION_GREATER_EQUAL 3.0.0)
    set_target_properties_obs(${PROJECT_NAME} PROPERTIES FOLDER "plugins/dcap" PREFIX "")
  else()
    set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "plugins/dcap")
    setup_plugin_target(${PROJECT_NAME})
  endif()
endif()
