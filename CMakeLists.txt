cmake_minimum_required(VERSION 3.17)

project(SecureChat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================
# Common Sources
# =========================

set(COMMON_SOURCES
    network/Message.cpp
    network/Socket.cpp
    crypto/Cipher.cpp
    crypto/Rsakeyexchange.cpp
    crypto/PasswordHasher.cpp
    auth/AuthManager.cpp
    db/Database.cpp
    db/UserRepository.cpp
)

# =========================
# Client
# =========================

add_executable(client
    client/client_main.cpp

    ui/ChatWindow.cpp
    ui/AuthWindow.cpp
    ui/Button.cpp
    ui/Textinputbox.cpp
    ui/Scrollabletextarea.cpp

    ${COMMON_SOURCES}
)

# =========================
# Server
# =========================

add_executable(server
    server/server_main.cpp

    ${COMMON_SOURCES}
    db/ChatHistoryRepository.cpp
)

# =========================
# Include Directories
# =========================

target_include_directories(client PRIVATE .)
target_include_directories(server PRIVATE .)

# =========================
# OpenSSL
# =========================

find_package(OpenSSL REQUIRED)

target_link_libraries(client PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_link_libraries(server PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
)

# =========================
# SQLite
# =========================

find_package(SQLite3 REQUIRED)

target_link_libraries(client PRIVATE SQLite3::SQLite3)
target_link_libraries(server PRIVATE SQLite3::SQLite3)

# =========================
# Windows Networking
# =========================

if(WIN32)
    target_link_libraries(client PRIVATE ws2_32)
    target_link_libraries(server PRIVATE ws2_32)
endif()

# =========================
# SFML (Client Only)
# =========================

find_package(SFML 3 REQUIRED COMPONENTS
    Graphics
    Window
    System
)

target_link_libraries(client PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
)
