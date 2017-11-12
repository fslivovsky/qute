file(GLOB SOURCES *.cc)

add_executable(qute ${SOURCES})
set_target_properties(qute PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

target_link_libraries(qute ${Boost_LIBRARIES} Threads::Threads)
