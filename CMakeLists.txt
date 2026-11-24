cmake_minimum_required(VERSION 3.16)
project(GeMSE_Muon_Simulation)

find_package(Geant4 REQUIRED)
include(${Geant4_USE_FILE})


#find_package(ROOT REQUIRED)
#include(${ROOT_USE_FILE})

include_directories(${PROJECT_SOURCE_DIR}/include)

file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hh)

add_executable(Muon_Simulation Muon_Simulation.cc ${sources} ${headers})
target_link_libraries(Muon_Simulation ${Geant4_LIBRARIES} ${ROOT_LIBRARIES})



#----------------------------------------------------------------------------
# Copy all scripts to the build directory, i.e. the directory in which we
# build B1. This is so that we can run the executable directly because it
# relies on these scripts being in the current working directory. -> From ExampleB1, we can adjust the list of scripts as needed.
#

file(GLOB MACRO_FILES
    ${PROJECT_SOURCE_DIR}/macros/*.mac
)
file(GLOB MUON_DATA
    ${PROJECT_SOURCE_DIR}/data/*.dat
)
file(GLOB SCAN_SCRIPT
    ${PROJECT_SOURCE_DIR}/zenith_scan.sh
    ${PROJECT_SOURCE_DIR}/optical_scan.sh
    ${PROJECT_SOURCE_DIR}/overburden_scan.sh
    ${PROJECT_SOURCE_DIR}/soft_component_scan.sh
)
file(COPY ${MACRO_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/macros)
file(COPY ${MUON_DATA} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)
file(COPY ${SCAN_SCRIPT} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})


#----------------------------------------------------------------------------
# Optional: CRY cosmic-ray shower generator
# Build with:  cmake -DWITH_CRY=ON -DCRY_DIR=$CRYHOME ..
#
option(WITH_CRY "Build with the CRY shower generator" OFF)

if(WITH_CRY)
    set(CRY_DIR "$ENV{CRYHOME}" CACHE PATH "CRY installation directory")

    find_path(CRY_INCLUDE_DIR   NAMES CRYGenerator.h HINTS ${CRY_DIR}/src)
    find_library(CRY_LIBRARY    NAMES CRY           HINTS ${CRY_DIR}/lib)

    if(NOT CRY_INCLUDE_DIR OR NOT CRY_LIBRARY)
        message(FATAL_ERROR
            "CRY not found. Set CRY_DIR (or the CRYHOME environment variable) "
            "to the directory containing src/ and lib/libCRY.a")
    endif()

    target_include_directories(Muon_Simulation PRIVATE ${CRY_INCLUDE_DIR})
    target_link_libraries(Muon_Simulation ${CRY_LIBRARY})
    target_compile_definitions(Muon_Simulation PRIVATE
        USE_CRY
        CRY_DATA_DIR="${CRY_DIR}/data")

    message(STATUS "CRY enabled: ${CRY_LIBRARY}")
endif()

