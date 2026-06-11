set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(ARM_GCC_ROOT "" CACHE PATH "GNU Arm Embedded install root")
set(TOOLCHAIN_PREFIX arm-none-eabi CACHE STRING "GNU Arm Embedded toolchain prefix")

if(ARM_GCC_ROOT)
    set(N32_ARM_GCC_BIN "${ARM_GCC_ROOT}/bin")
    list(APPEND CMAKE_PROGRAM_PATH "${N32_ARM_GCC_BIN}")
endif()

find_program(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc REQUIRED)
find_program(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}-gcc REQUIRED)
find_program(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}-objcopy REQUIRED)
find_program(CMAKE_SIZE ${TOOLCHAIN_PREFIX}-size REQUIRED)

foreach(tool IN ITEMS CMAKE_ASM_COMPILER CMAKE_OBJCOPY CMAKE_SIZE)
    if(NOT EXISTS "${${tool}}")
        message(FATAL_ERROR "Missing ${tool}: ${${tool}}")
    endif()
endforeach()

set(CMAKE_EXECUTABLE_SUFFIX_C ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_ASM ".elf")
