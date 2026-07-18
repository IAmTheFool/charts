# flux-package.cmake
#
# Consumed by flux's own top-level CMakeLists.txt via:
#   file(GLOB FLUX_PACKAGE_FILES CONFIGURE_DEPENDS external/*/flux-package.cmake)
#   foreach(pkg_file ${FLUX_PACKAGE_FILES})
#       include(${pkg_file})
#   endforeach()
#
# include() does NOT change CMAKE_CURRENT_SOURCE_DIR, so CMAKE_CURRENT_SOURCE_DIR
# here resolves to the consuming flux project's root — same as the
# fmt/nlohmann-json registry fragments (add_subdirectory(external/fmt)).
#
# The path is wrapped in $<BUILD_INTERFACE:...> rather than passed bare.
# flux's own CMakeLists.txt does install(TARGETS ... EXPORT fluxTargets ...),
# and CMake validates at every configure (not just `cmake --install`) that a
# PUBLIC/INTERFACE include dir on an exported target isn't a bare path inside
# the source tree - that path won't exist for someone consuming an installed
# flux later. A plain "external/charts/include" trips that check with:
#   "Target \"flux\" INTERFACE_INCLUDE_DIRECTORIES property contains path
#    ... which is prefixed in the source directory."
# Only BUILD_INTERFACE is used (no INSTALL_INTERFACE) because charts' headers
# are NOT part of flux's own install() rules (only include/, external/stb,
# external/dr get installed) - adding an INSTALL_INTERFACE path here would
# promise a location that silently doesn't exist post-install.
#
# charts is header-only: no add_subdirectory, no new target, no linking.
target_include_directories(flux PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/charts/include>
)