#file(GLOB SOURCES *.cc)
set(SOURCES
"constraint_DB.cc"
"debug_helper.cc"
"decision_heuristic.cc"
"decision_heuristic_SGDB.cc"
"decision_heuristic_VMTF_deplearn.cc"
"decision_heuristic_VMTF_prefix.cc"
"decision_heuristic_VSIDS_deplearn.cc"
"dependency_manager_rrs.cc"
"dependency_manager_upure.cc"
"dependency_manager_watched.cc"
"main.cc"
"model_generator.cc"
"model_generator_simple.cc"
"model_generator_weighted.cc"
"parser.cc"
"qcdcl.cc"
"simple_tracer.cc"
"standard_learning_engine.cc"
"three_watched_literal_propagator.cc"
"variable_data.cc"
"watched_literal_propagator.cc"
)

add_executable(qute ${SOURCES})
set_target_properties(qute PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
target_include_directories(qute PUBLIC "/home/fs/docopt.cpp")

target_link_libraries(qute ${Boost_LIBRARIES})
target_link_libraries(qute ${CMAKE_THREAD_LIBS_INIT})
# change the following line to enable static linking (also see ../CMakeLists.txt)
target_link_libraries(qute docopt)
#target_link_libraries(qute docopt_s)

# locate and load SMS if available
find_path(SMS_INCLUDE_DIR "sms.hpp" PATH_SUFFIXES "sms")
find_library(CADICAL_LIBRARY
	NAMES smscadical
)
find_library(SMS_LIBRARY
  	NAMES libsms.a sms
)

if(NOT CADICAL_LIBRARY OR NOT SMS_LIBRARY OR NOT SMS_INCLUDE_DIR)
	message(STATUS "SMS library not found, continuing without")
else()
	message(STATUS "Found SMS at ${SMS_LIBRARY}")
	message(STATUS "Found SMS-ready CaDiCaL at ${CADICAL_LIBRARY}")
	include_directories("${SMS_INCLUDE_DIR}")
	target_compile_definitions(qute PUBLIC USE_SMS=1)
	add_library(sms STATIC IMPORTED GLOBAL)
	add_library(smscadical STATIC IMPORTED GLOBAL)
	set_target_properties(smscadical PROPERTIES IMPORTED_LOCATION ${CADICAL_LIBRARY})
	set_target_properties(sms PROPERTIES IMPORTED_LOCATION ${SMS_LIBRARY})
endif()
