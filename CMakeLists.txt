cmake_minimum_required(VERSION 3.19)
project(GeoDataStudio LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Charts DataVisualization Concurrent)

qt_standard_project_setup()

qt_add_executable(GeoDataStudio
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    nanoflann.hpp
)

target_link_libraries(GeoDataStudio
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Charts
        Qt6::DataVisualization
        Qt6::Concurrent 
)

install(TARGETS GeoDataStudio
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET GeoDataStudio
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})