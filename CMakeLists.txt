cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(PhoenixShell VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "PhoenixShell is a WinPE/Windows shell and must be built on Windows with Qt6.")
endif()

option(PHOENIXSHELL_DEPLOY_QT "Deploy Qt DLLs and plugins with windeployqt after build" ON)
option(PHOENIXSHELL_COPY_RUNTIME_DLLS "Copy target runtime DLL dependencies into the build output" ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

qt_add_executable(PhoenixShell WIN32
    src/ApplicationBootstrapper.cpp
    src/ApplicationBootstrapper.h
    src/ApplicationLauncher.cpp
    src/ApplicationLauncher.h
    src/DataLoader.cpp
    src/DataLoader.h
    src/DesktopManager.cpp
    src/DesktopManager.h
    src/DiagnosticsModule.cpp
    src/DiagnosticsModule.h
    src/ExternalAppHost.cpp
    src/ExternalAppHost.h
    src/FileManager.cpp
    src/FileManager.h
    src/InstallerModule.cpp
    src/InstallerModule.h
    src/Logging.cpp
    src/Logging.h
    src/PartitionScanner.cpp
    src/PartitionScanner.h
    src/PathUtils.cpp
    src/PathUtils.h
    src/ProgramManager.cpp
    src/ProgramManager.h
    src/SettingsPage.cpp
    src/SettingsPage.h
    src/ShellMainWindow.cpp
    src/ShellMainWindow.h
    src/ShellWindow.cpp
    src/ShellWindow.h
    src/StartMenu.cpp
    src/StartMenu.h
    src/Taskbar.cpp
    src/Taskbar.h
    src/ToolRegistry.cpp
    src/ToolRegistry.h
    src/WallpaperManager.cpp
    src/WallpaperManager.h
    src/WindowManager.cpp
    src/WindowManager.h
    src/main.cpp
)

target_compile_features(PhoenixShell PRIVATE cxx_std_20)

target_compile_definitions(PhoenixShell PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(PhoenixShell PRIVATE
    Qt6::Widgets
    Qt6::Network
    Advapi32
    Iphlpapi
    Ole32
    Setupapi
    Shell32
    User32
    Wbemuuid
)

if(MSVC)
    target_link_options(PhoenixShell PRIVATE
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
    )

    set_property(TARGET PhoenixShell PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

add_custom_command(TARGET PhoenixShell POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PhoenixShell>/config"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PhoenixShell>/Wallpapers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/tools.json"
        "$<TARGET_FILE_DIR:PhoenixShell>/config/tools.json"
    COMMENT "Copying PhoenixShell runtime config"
    VERBATIM
)

if(PHOENIXSHELL_COPY_RUNTIME_DLLS)
    add_custom_command(TARGET PhoenixShell POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:PhoenixShell>
            "$<TARGET_FILE_DIR:PhoenixShell>"
        COMMAND_EXPAND_LISTS
        COMMENT "Copying target runtime DLL dependencies"
        VERBATIM
    )
endif()

if(PHOENIXSHELL_DEPLOY_QT)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${QT_BIN_DIR}"
    )

    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "windeployqt was not found. Check CMAKE_PREFIX_PATH and Qt installation.")
    endif()

    add_custom_command(TARGET PhoenixShell POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            $<IF:$<CONFIG:Debug>,--debug,--release>
            --compiler-runtime
            --no-translations
            "$<TARGET_FILE:PhoenixShell>"
        COMMENT "Deploying Qt runtime and plugins"
        VERBATIM
    )
endif()

if(MSVC)
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)

    foreach(RUNTIME_DLL ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
        add_custom_command(TARGET PhoenixShell POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RUNTIME_DLL}"
                "$<TARGET_FILE_DIR:PhoenixShell>"
            COMMENT "Copying MSVC runtime: ${RUNTIME_DLL}"
            VERBATIM
        )
    endforeach()
endif()

set_target_properties(PhoenixShell PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
