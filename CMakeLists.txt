cmake_minimum_required(VERSION 3.10.0)
project(Migration VERSION 0.1.0 LANGUAGES C CXX)
# 定义图标文件路径
set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/appicon.ico")

# 自动生成 .rc 文件
if(WIN32)
    # 创建临时的 .rc 文件内容
    set(RC_CONTENT "IDI_ICON1 ICON \"${APP_ICON}\"\n")
    
    # 将 .rc 内容写入文件
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc" "${RC_CONTENT}")
    
    # 设置生成的 .rc 文件路径
    set(ICON_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
endif()

add_executable(Migration main.cpp ${ICON_RC_FILE})

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

# 链接系统库（对应 -ladvapi32 -lshell32 -luser32）
target_link_libraries(Migration 
    advapi32
    shell32
    user32
)

# 静态链接选项（对应 -static）
set_target_properties(Migration PROPERTIES
    LINK_FLAGS "-static"
)

set_target_properties(Migration PROPERTIES
    COMPILE_FLAGS "-finput-charset=UTF-8 -fexec-charset=UTF-8")