cmake_minimum_required(VERSION 3.19)
project(LetsShare LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)

# Set the OpenSSL root directory manually
set(OPENSSL_ROOT_DIR "C:/OpenSSL")
set(OPENSSL_INCLUDE_DIR "C:/OpenSSL/include")
set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libcrypto.lib")
set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libssl.lib")

# Find OpenSSL manually
find_package(OpenSSL REQUIRED)

qt_standard_project_setup()

if (WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
    set_source_files_properties(${APP_ICON_RESOURCE_WINDOWS} PROPERTIES LANGUAGE RC)
endif()

qt_add_executable(LetsShare
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    fileserver.h
    fileclient.h
    fileserver.cpp
    fileclient.cpp

    applink.c
    ${APP_ICON_RESOURCE_WINDOWS}
    crypto.h
    crypto.cpp
)

qt_add_resources(LetsShare "resources.qrc")

target_include_directories(LetsShare PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(LetsShare
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        OpenSSL::Crypto
        OpenSSL::SSL
)

include(GNUInstallDirs)

install(TARGETS LetsShare
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET LetsShare
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)

install(SCRIPT ${deploy_script})
