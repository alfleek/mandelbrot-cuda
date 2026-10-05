cmake_minimum_required(VERSION 3.18)
project(MandelbrotCUDA CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force consistent runtime library settings
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Source files
set(SOURCES
    main.cpp
    gpu_detector.cpp
    mandelbrot_cpu.cpp
    renderer_sfml.cpp
)

# Create executable
add_executable(mandelbrot_cuda ${SOURCES})

# Set CUDA paths manually (avoiding find_package issues)
set(CUDA_ROOT "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3")
set(CUDA_INCLUDE_DIRS "${CUDA_ROOT}/include")
set(CUDA_LIBRARIES "${CUDA_ROOT}/lib/x64/cuda.lib;${CUDA_ROOT}/lib/x64/cudart.lib")

# Include directories
target_include_directories(mandelbrot_cuda PRIVATE
    ${CUDA_INCLUDE_DIRS}
)

# Find SFML (local installation)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SFML")
    set(SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SFML/lib/cmake/SFML")
    set(SFML_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SFML/include")
    set(SFML_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SFML/lib")
    
    find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)
    
    if(SFML_FOUND)
        message(STATUS "SFML found: ${SFML_VERSION}")
        message(STATUS "SFML include dir: ${SFML_INCLUDE_DIR}")
        message(STATUS "SFML library dir: ${SFML_LIBRARY_DIR}")
    else()
        message(FATAL_ERROR "SFML not found! Please run install_sfml.ps1 first.")
    endif()
else()
    message(FATAL_ERROR "SFML not found! Please run install_sfml.ps1 to install SFML locally.")
endif()

# Link libraries
target_link_libraries(mandelbrot_cuda
    ${CUDA_LIBRARIES}
    sfml-graphics
    sfml-window
    sfml-system
)

# Platform-specific settings
if(WIN32)
    # Basic Windows libraries only
    target_link_libraries(mandelbrot_cuda
        kernel32
        user32
    )
    
    # Set output directory
    set_target_properties(mandelbrot_cuda PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# Add custom command to compile CUDA file with consistent runtime settings
set(CUDA_FILE "${CMAKE_CURRENT_SOURCE_DIR}/mandelbrot_cuda.cu")
set(CUDA_OBJ "${CMAKE_CURRENT_BINARY_DIR}/mandelbrot_cuda.obj")

add_custom_command(
    OUTPUT ${CUDA_OBJ}
    COMMAND nvcc -c ${CUDA_FILE} -o ${CUDA_OBJ} -I${CUDA_INCLUDE_DIRS} -O3 -arch=sm_50 -Xcompiler "/MT"
    DEPENDS ${CUDA_FILE}
    COMMENT "Compiling CUDA kernel"
)

# Add the CUDA object to the executable
target_sources(mandelbrot_cuda PRIVATE ${CUDA_OBJ})

# Print configuration info
message(STATUS "CUDA include dirs: ${CUDA_INCLUDE_DIRS}")
message(STATUS "Runtime library: ${CMAKE_MSVC_RUNTIME_LIBRARY}")