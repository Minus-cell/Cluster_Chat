# 设置当前目录源文件
aux_source_directory(. SRC_LIST)

# 添加头文件搜索路径（指向服务端目录，那里存放了公共头文件）
include_directories(${PROJECT_SOURCE_DIR}/src/server)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)

# 生成可执行文件
add_executable(ChatClient ${SRC_LIST})

# 链接 pthread 库
target_link_libraries(ChatClient pthread)