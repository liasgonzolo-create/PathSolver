cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(gdsolver VERSION 0.1.0 LANGUAGES CXX)

# gdsolver is one repository with two build products that share sources:
#   dp/   the reachability-DP solver core and its CLI (leveldp.exe), built
#         anywhere with a C++ compiler -- no Geode or GD required
#   src/  the Geode mod, which drives GD's own physics as the replay verifier
#         and (eventually) hosts the solver loop in-process
option(GDSOLVER_BUILD_DP "Build the DP core library and the leveldp CLI" ON)
option(GDSOLVER_BUILD_MOD "Build the Geode mod (requires GEODE_SDK)" ON)

# The search is throughput-bound, and RelWithDebInfo differed from Release here only in how it
# links: incrementally, which routes calls through jump thunks and keeps /OPT:REF and /OPT:ICF off.
# Link it the Release way (debug info kept). The code generation is unchanged, so the output is
# too; measured on leveldp, the same objects linked both ways used 7% less CPU over three
# official levels (421 -> 391 s, single thread) with byte-identical plans and traces.
if (MSVC)
    foreach (kind EXE SHARED)
        string(REGEX REPLACE "/INCREMENTAL[^ ]*" "" CMAKE_${kind}_LINKER_FLAGS_RELWITHDEBINFO
               "${CMAKE_${kind}_LINKER_FLAGS_RELWITHDEBINFO}")
        string(APPEND CMAKE_${kind}_LINKER_FLAGS_RELWITHDEBINFO " /INCREMENTAL:NO /OPT:REF /OPT:ICF")
    endforeach()
endif()

# The solver core's library (gdsolver_dpcore) is always added: the mod links it too. Only the
# leveldp CLI and its drivers are gated on GDSOLVER_BUILD_DP.
add_subdirectory(dp)

if (GDSOLVER_BUILD_MOD)
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
    # The second copy of the core is not a mod source: it is compiled as the CLI's code is,
    # below, without the mod's options and prelude.
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/mod/dp_bridge2.cpp)
    add_library(${PROJECT_NAME} SHARED ${SOURCES})
    # src/mod/*.hpp include each other and the solver/ headers by path from src/
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    # The mod's DP must behave exactly like the dev CLI -- that equality is an acceptance
    # criterion, not an aspiration -- so the mod does not compile the solver at all. Its first
    # copy is the CLI's own compiled core (gdsolver_dpcore), and its second copy
    # (src/mod/dp_bridge2.cpp, the solver's headers again under a renamed namespace) is a library
    # of its own with the same options. The mod sees dp/src/dpcore.hpp through
    # src/mod/dp_bridge.hpp; the only other dp/ header it includes is dp/band_rows.hpp, a file
    # format shared with dptest that compiles no solver code.
    add_library(gdsolver_dpcore2 STATIC src/mod/dp_bridge2.cpp)
    target_include_directories(gdsolver_dpcore2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(gdsolver_dpcore2 PRIVATE gdsolver_dp)
    set_target_properties(gdsolver_dpcore2 PROPERTIES POSITION_INDEPENDENT_CODE ON)
    gdsolver_dp_options(gdsolver_dpcore2)

    # Plain signature on purpose: Geode's setup_geode_mod links without keywords, and CMake
    # refuses to mix the two forms on one target.
    target_link_libraries(${PROJECT_NAME} gdsolver_dpcore gdsolver_dpcore2)

    if (NOT DEFINED ENV{GEODE_SDK})
        message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode (or configure with -DGDSOLVER_BUILD_MOD=OFF)")
    else()
        message(STATUS "Found Geode: $ENV{GEODE_SDK}")
    endif()

    add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

    setup_geode_mod(${PROJECT_NAME})
endif()
