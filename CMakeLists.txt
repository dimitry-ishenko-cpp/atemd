cmake_minimum_required(VERSION 3.1)
project(atemd VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SOURCES
    pgm/args.hpp pgm/args.ipp
    src/connection.hpp
    src/main.cpp
    src/server.hpp
    win/service.hpp win/service.cpp
)

include(GNUInstallDirs)

add_executable(${PROJECT_NAME} ${SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
target_compile_definitions(${PROJECT_NAME} PRIVATE VERSION="${PROJECT_VERSION}")
target_link_libraries(${PROJECT_NAME} PRIVATE atem++)

if(CYGWIN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601 __USE_W32_SOCKETS)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 wsock32)
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
