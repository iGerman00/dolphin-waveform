cmake_minimum_required(VERSION 3.24)

project(dolphin-waveform VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Installation prefix" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(BUILD_SHARED_LIBS ON)
include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6ColorScheme REQUIRED)
find_package(KF6KIO REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswresample)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(KDE_INSTALL_PLUGINDIR "${CMAKE_INSTALL_LIBDIR}/qt6/plugins")

kcoreaddons_add_plugin(dolphinwaveform SOURCES src/audiothumbnailer.cpp INSTALL_NAMESPACE "kf6/thumbcreator")
target_link_libraries(dolphinwaveform PRIVATE
    Qt6::Core
    Qt6::Gui
    KF6::KIOGui
    KF6::ColorScheme
    PkgConfig::FFMPEG
)

install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES dolphin-waveform.conf.example DESTINATION ${CMAKE_INSTALL_DATADIR}/dolphin-waveform)
