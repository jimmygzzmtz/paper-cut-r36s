# Toolchain for RK3326 (Cortex-A35 / aarch64 Linux) with clang-21 and debian sysroot
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# The sysroot: $RK3326_SYSROOT (an environment variable, so CMake's try_compile runs see it too),
# else /opt/rk3326-sysroot. An Ubuntu 24.04 arm64 one (glibc 2.39) matches dArkOS-class firmware.
if(DEFINED ENV{RK3326_SYSROOT})
    set(RK3326_SYSROOT "$ENV{RK3326_SYSROOT}")
else()
    set(RK3326_SYSROOT /opt/rk3326-sysroot)
endif()
set(CMAKE_SYSROOT ${RK3326_SYSROOT})

set(CMAKE_C_COMPILER /usr/bin/clang-21)
set(CMAKE_CXX_COMPILER /usr/bin/clang++-21)

# GCC's headers and runtime (libstdc++, crtbegin) from the sysroot's own GCC; the host needs no
# aarch64 GCC. ld.lld-21 when installed (fast), else the default linker (binutils' aarch64 ld).
set(TARGET_FLAGS "--target=aarch64-linux-gnu --gcc-toolchain=${RK3326_SYSROOT}/usr -mcpu=cortex-a35 -fsigned-char -fno-math-errno -pipe")
if(EXISTS /usr/bin/ld.lld-21)
    set(TARGET_LINK_FLAGS "--ld-path=/usr/bin/ld.lld-21")
endif()

set(CMAKE_C_FLAGS_INIT "${TARGET_FLAGS} -O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_INIT "${TARGET_FLAGS} -O3 -DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${TARGET_FLAGS} ${TARGET_LINK_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${TARGET_FLAGS} ${TARGET_LINK_FLAGS}")

set(CMAKE_FIND_ROOT_PATH ${RK3326_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# PortMaster RPATH
set(CMAKE_BUILD_RPATH "$ORIGIN:$ORIGIN/libs.aarch64")
set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/libs.aarch64")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

set(Rust_CARGO_TARGET "aarch64-unknown-linux-gnu" CACHE STRING "")
