cmake_minimum_required(VERSION 3.16)
# Prefer g++ so we link libstdc++ (c++ may be clang without a stdlib path).
if(NOT CMAKE_CXX_COMPILER)
    find_program(PEERDESK_GXX g++)
    if(PEERDESK_GXX)
        set(CMAKE_CXX_COMPILER "${PEERDESK_GXX}" CACHE FILEPATH "" FORCE)
    endif()
endif()
project(peerdesk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenSSL REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED x11 xtst)
pkg_check_modules(JPEG REQUIRED libjpeg)
pkg_check_modules(ARGON2 REQUIRED libargon2)

add_library(peerdesk_shared STATIC
    shared/src/protocol.cpp
    shared/src/auth.cpp
    shared/src/map.cpp
    shared/src/jpeg.cpp
    shared/src/tls.cpp
)
target_include_directories(peerdesk_shared PUBLIC
    ${CMAKE_SOURCE_DIR}/shared/include
    ${JPEG_INCLUDE_DIRS}
    ${ARGON2_INCLUDE_DIRS}
)
target_link_libraries(peerdesk_shared PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    ${JPEG_LIBRARIES}
    ${ARGON2_LIBRARIES}
)

add_library(peerdesk_host STATIC
    server/users.cpp
    server/capture.cpp
    server/inject.cpp
    server/host_server.cpp
)
target_include_directories(peerdesk_host PUBLIC
    ${CMAKE_SOURCE_DIR}/server
    ${X11_INCLUDE_DIRS}
)
target_link_libraries(peerdesk_host PUBLIC
    peerdesk_shared
    ${X11_LIBRARIES}
    pthread
)

add_executable(peerdesk-server server/main.cpp)
target_link_libraries(peerdesk-server PRIVATE peerdesk_host)

add_executable(peerdesk-client
    client/main.cpp
    client/MainWindow.cpp
    client/SessionWorker.cpp
    client/keymap.cpp
)
target_include_directories(peerdesk-client PRIVATE ${CMAKE_SOURCE_DIR}/client)
target_link_libraries(peerdesk-client PRIVATE peerdesk_shared Qt6::Widgets pthread)

add_executable(peerdesk-smoke tests/peerdesk_smoke.cpp)
target_include_directories(peerdesk-smoke PRIVATE ${CMAKE_SOURCE_DIR}/server)
target_link_libraries(peerdesk-smoke PRIVATE peerdesk_host)

add_executable(peerdesk-client-test
    tests/client_test.cpp
    client/MainWindow.cpp
    client/SessionWorker.cpp
    client/keymap.cpp
)
target_include_directories(peerdesk-client-test PRIVATE ${CMAKE_SOURCE_DIR}/client)
target_link_libraries(peerdesk-client-test PRIVATE peerdesk_host Qt6::Widgets Qt6::Test pthread)

enable_testing()
add_test(NAME peerdesk-smoke COMMAND peerdesk-smoke)
add_test(NAME peerdesk-client-test COMMAND peerdesk-client-test)
set_tests_properties(peerdesk-client-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
