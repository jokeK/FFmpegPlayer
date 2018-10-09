#关闭openssl 不支持rtmps
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNO_CRYPTO")
#所有源文件放入rtmp_source 变量
file(GLOB rtmp_source *.c)

add_library(
    rtmp
    STATIC
    ${rtmp_source}
)
