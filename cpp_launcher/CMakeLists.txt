cmake_minimum_required(VERSION 3.15)
project(OpenClawLauncher VERSION 2.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W3 /O2 /source-charset:utf-8 /execution-charset:gbk)
else()
    add_compile_options(-Wall -O2)
endif()

add_executable(OpenClawLauncher WIN32
    main.cpp
    res/resource.rc
)

target_link_libraries(OpenClawLauncher PRIVATE
    user32
    gdi32
    shell32
    ole32
    oleaut32
    comctl32
    winhttp
    advapi32
    dwmapi
    comdlg32
)

if(MSVC)
    set_target_properties(OpenClawLauncher PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()

install(TARGETS OpenClawLauncher DESTINATION bin)
