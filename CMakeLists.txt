cmake_minimum_required(VERSION 3.12)

project(ebus C)

set(SRC_LIST src/ebus.c src/ebus_msg.c src/rbtree.c)
set(LIBECO "/home/huang/learn/install/libeco")




add_library(ebus SHARED ${SRC_LIST})
target_include_directories(ebus PUBLIC ${LIBECO}/include)
target_link_directories(ebus PUBLIC ${LIBECO}/lib)
target_link_libraries(ebus eco)


add_executable(ebusd src/ebusd.c)
target_link_libraries(ebusd ebus)




add_subdirectory(example)

install(TARGETS ebus LIBRARY DESTINATION lib)
install(FILES src/ebus.h  DESTINATION include)
install(TARGETS ebusd RUNTIME DESTINATION sbin)
