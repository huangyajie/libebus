cmake_minimum_required(VERSION 3.12)

project(ebus C)

set(SRC_LIST ebus.c ebus_msg.c rbtree.c)
set(LIBECO "/home/huang/learn/install/libeco")




# add_library(ebus SHARED ${SRC_LIST})
# target_include_directories(ebus PUBLIC ${LIBECO}/include)
# # target_link_directories(ebus PUBLIC ${LIBECO}/lib)
# target_link_libraries(ebus eco)


# add_executable(example_rbtree rbtree.c example_rbtree.c)
add_executable(ebusd ${SRC_LIST} ebusd.c)
target_include_directories(ebusd PUBLIC ${LIBECO}/include)
target_link_directories(ebusd PUBLIC ${LIBECO}/lib)
target_link_libraries(ebusd eco)

add_executable(example_ebus_echosrv ${SRC_LIST} example_ebus_echosrv.c)
target_include_directories(example_ebus_echosrv PUBLIC ${LIBECO}/include)
target_link_directories(example_ebus_echosrv PUBLIC ${LIBECO}/lib)
target_link_libraries(example_ebus_echosrv eco)

add_executable(example_ebus_echocli ${SRC_LIST} example_ebus_echocli.c)
target_include_directories(example_ebus_echocli PUBLIC ${LIBECO}/include)
target_link_directories(example_ebus_echocli PUBLIC ${LIBECO}/lib)
target_link_libraries(example_ebus_echocli eco)

