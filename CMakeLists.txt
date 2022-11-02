cmake_minimum_required(VERSION 3.1)
project(atemd VERSION 0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SOURCES
    pgm/args.hpp pgm/args.ipp
    src/connection.hpp
    src/main.cpp
    src/server.hpp
)

include(GNUInstallDirs)

add_executable(${PROJECT_NAME} ${SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
target_compile_definitions(${PROJECT_NAME} PRIVATE VERSION="${PROJECT_VERSION}")
target_link_libraries(${PROJECT_NAME} PRIVATE atem++)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})