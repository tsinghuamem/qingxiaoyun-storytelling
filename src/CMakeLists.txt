set(COMMON_LIBS asound websockets wiringPi dl pthread m atomic pv_recorder pv_speaker curl)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG -g")
    message(STATUS "Build type is ${CMAKE_BUILD_TYPE}")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
    message(STATUS "Build type is not Debug")
endif()

file(GLOB_RECURSE SOURCE_LIST "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

add_executable(ai_story ${SOURCE_LIST})
target_include_directories(ai_story PRIVATE ${INC_DIR})
target_link_libraries(ai_story ${COMMON_LIBS})
target_link_directories(ai_story PRIVATE ${LIB_DIR})

if(DEFINED OUTPUT_DIR)
    add_custom_command(TARGET ai_story POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:ai_story>
            ${OUTPUT_DIR}
            COMMENT "Copying exe to output directory.")
endif()