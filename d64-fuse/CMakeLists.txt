add_executable(d64-fuse d64-fuse.c d64fuse_context.c common_operations.c dir_operations.c file_operations.c operations.c)

pkg_check_modules(FUSE3 REQUIRED fuse3)

target_compile_options(d64-fuse PRIVATE -Wall -Wextra -Werror -pedantic -DFUSE_USE_VERSION=35 -D_GNU_SOURCE=1)
target_include_directories(d64-fuse PRIVATE ../DiskImagery64-base ${FUSE3_INCLUDE_DIRS})
target_link_libraries(d64-fuse PRIVATE di64base ${FUSE3_LIBRARIES})

add_dependencies(d64-fuse di64base)

install(TARGETS d64-fuse)
