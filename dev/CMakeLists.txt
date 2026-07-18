cmake_minimum_required(VERSION 3.21)
project(flux_charts_dev LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# This directory IS the "flux app project" as far as flux's own build
# system is concerned — CMAKE_SOURCE_DIR is fixed to whatever the outermost
# invoked CMakeLists.txt is, and every platform subdir inside flux
# (linux/CMakeList.txt, windows/CMakeList.txt, ...) resolves
# ${CMAKE_SOURCE_DIR}/lib/main.cpp and ${CMAKE_SOURCE_DIR}/config/AppConfig.cmake
# against THIS project, not the flux submodule. So dev/lib/main.cpp and
# dev/config/ are picked up automatically once flux is pulled in below.
# ============================================================================

add_subdirectory(external/flux)

# ── Attach charts headers to the app target flux just created ──────────────
# flux_app is defined deep inside external/flux/linux/CMakeList.txt (or the
# equivalent for whichever platform this is configured for), but CMake
# target names are globally visible after creation, so we can extend it
# from here instead of forking anything inside the flux submodule itself.
if(TARGET flux_app)
    target_include_directories(flux_app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
else()
    message(WARNING
        "flux_charts_dev: flux_app target was not created by external/flux — "
        "check that this platform is supported by the flux submodule's "
        "top-level CMakeLists.txt (FLUX_NEED_DESKTOP_LIB / ANDROID guards).")
endif()