# Run at build time: cmake -DPROJECT_SOURCE_DIR=... -DPROJECT_BINARY_DIR=... -P cmake/WriteVersionHeader.cmake
# Reads VERSION.md and writes version.h with APP_VERSION_STRING.
file(READ "${PROJECT_SOURCE_DIR}/VERSION.md" VERSION_FILE)
string(STRIP "${VERSION_FILE}" VERS)
if(NOT VERS)
  set(VERS "0.0.0")
endif()
file(WRITE "${PROJECT_BINARY_DIR}/version.h" "#ifndef LLCONNECT3_VERSION_H\n#define LLCONNECT3_VERSION_H\n#define APP_VERSION_STRING \"${VERS}\"\n#endif\n")
