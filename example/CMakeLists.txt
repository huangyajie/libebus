
include_directories(${PROJECT_SOURCE_DIR}/src)



add_executable(example_rbtree example_rbtree.c)
add_executable(example_ebus_echosrv example_ebus_echosrv.c)
add_executable(example_ebus_echocli example_ebus_echocli.c)


target_link_libraries(example_rbtree ebus)
target_link_libraries(example_ebus_echosrv ebus)
target_link_libraries(example_ebus_echocli ebus)


install(TARGETS example_rbtree RUNTIME DESTINATION bin)
install(TARGETS example_ebus_echosrv RUNTIME DESTINATION bin)
install(TARGETS example_ebus_echocli RUNTIME DESTINATION bin)
