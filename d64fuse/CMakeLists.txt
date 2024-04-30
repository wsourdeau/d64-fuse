add_executable(d64fuse d64fuse.c operations.c)

pkg_check_modules(FUSE3 REQUIRED fuse3)

target_compile_options(d64fuse PRIVATE -Wall -Wextra -Werror -pedantic)
target_include_directories(d64fuse PRIVATE ../DiskImagery64-base ${FUSE3_INCLUDE_DIRS})
target_link_libraries(d64fuse PRIVATE di64base ${FUSE3_LIBRARIES})

add_dependencies(d64fuse di64base)

install(TARGETS d64fuse)
