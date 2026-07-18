# flux-package.cmake
#
# Consumed by flux's own top-level CMakeLists.txt via:
#   file(GLOB FLUX_PACKAGE_FILES CONFIGURE_DEPENDS external/*/flux-package.cmake)
#   foreach(pkg_file ${FLUX_PACKAGE_FILES})
#       include(${pkg_file})
#   endforeach()
#
# include() does NOT change CMAKE_CURRENT_SOURCE_DIR, so the relative path
# below resolves against the consuming flux project's root — same as the
# fmt/nlohmann-json registry fragments (add_subdirectory(external/fmt)).
#
# charts is header-only: no add_subdirectory, no new target, no linking.
# Every FluxXChartWidget lives entirely in headers under include/flux_charts/,
# so attaching the include path to the existing `flux` target is the whole
# integration.
target_include_directories(flux PUBLIC external/charts/include)