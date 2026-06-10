set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(ARM_GCC_ROOT "D:/tools/arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi" CACHE PATH "GNU Arm Embedded install root")
set(TOOLCHAIN_PREFIX arm-none-eabi CACHE STRING "GNU Arm Embedded toolchain prefix")
set(N32_ARM_GCC_BIN "${ARM_GCC_ROOT}/bin")

list(APPEND CMAKE_PROGRAM_PATH "${N32_ARM_GCC_BIN}")

find_program(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc REQUIRED)
set(CMAKE_ASM_COMPILER "${N32_ARM_GCC_BIN}/${TOOLCHAIN_PREFIX}-gcc.exe" CACHE FILEPATH "GNU Arm assembler compiler" FORCE)
set(CMAKE_OBJCOPY "${N32_ARM_GCC_BIN}/${TOOLCHAIN_PREFIX}-objcopy.exe" CACHE FILEPATH "GNU Arm objcopy" FORCE)
set(CMAKE_SIZE "${N32_ARM_GCC_BIN}/${TOOLCHAIN_PREFIX}-size.exe" CACHE FILEPATH "GNU Arm size" FORCE)

foreach(tool IN ITEMS CMAKE_ASM_COMPILER CMAKE_OBJCOPY CMAKE_SIZE)
    if(NOT EXISTS "${${tool}}")
        message(FATAL_ERROR "Missing ${tool}: ${${tool}}")
    endif()
endforeach()

set(CMAKE_EXECUTABLE_SUFFIX_C ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_ASM ".elf")
