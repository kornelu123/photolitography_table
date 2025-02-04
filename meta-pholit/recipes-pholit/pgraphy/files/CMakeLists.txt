cmake_minimum_required(VERSION 3.10)

project(pgraphy C CXX)

set(CMAKE_C_STANDARD 11)

add_executable(${PROJECT_NAME}
    main.cpp
    image.cpp
    hal/ti_gpio.c
    hal/ti_i2c.c
    )

find_package(OpenCV REQUIRED core imgcodecs highgui)

include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
include_directories(${LIBGPIOD_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})
target_link_libraries(${PROJECT_NAME} i2c)
target_link_libraries(${PROJECT_NAME} gpiod)
