cmake_minimum_required(VERSION 3.0.0)

project(mini_python VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

aux_source_directory(src/ SRC_LIST)

add_executable(${PROJECT_NAME} ${SRC_LIST})

set(CMAKE_CXX_FLAGS "-Wall")

if (NOT CMAKE_SYSTEM_NAME MATCHES ".*Win.*")
    target_link_libraries(${PROJECT_NAME} -ltbb -lpthread)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wpedantic")
endif()

# add_definitions()

