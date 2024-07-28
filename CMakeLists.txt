cmake_minimum_required(VERSION 3.12)

project(ebus C)

set(SRC_LIST ebus.c ebus_msg.c rbtree.c)
set(LIBECO "/home/huang/learn/install/libeco")



# add_library(ebus SHARED ${SRC_LIST})
# target_include_directories(ebus PUBLIC ${LIBECO}/include)
# # target_link_directories(ebus PUBLIC ${LIBECO}/lib)
# target_link_libraries(ebus eco)


# add_executable(example_rbtree rbtree.c example_rbtree.c)
add_executable(example_ebus_cli ${SRC_LIST} example_ebus_cli.c)
target_include_directories(example_ebus_cli PUBLIC ${LIBECO}/include)
target_link_directories(example_ebus_cli PUBLIC ${LIBECO}/lib)
target_link_libraries(example_ebus_cli eco)
