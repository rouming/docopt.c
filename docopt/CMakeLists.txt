get_filename_component(EXEC_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
string(REPLACE " " "_" EXEC_NAME "${EXEC_NAME}")

# include(GenerateExportHeader)
if (APPLE)
  # brew
  if (IS_DIRECTORY "/usr/local/opt/bison")
    list(APPEND CMAKE_PREFIX_PATH  "/usr/local/opt/bison/bin")
    list(APPEND CMAKE_LIBRARY_PATH "/usr/local/opt/bison/lib")
  endif ()

  if (IS_DIRECTORY "/usr/local/opt/flex")
    list(APPEND CMAKE_PREFIX_PATH  "/usr/local/opt/flex/bin")
    list(APPEND CMAKE_INCLUDE_PATH "/usr/local/opt/flex/include")
    list(APPEND CMAKE_LIBRARY_PATH "/usr/local/opt/flex/lib")
  endif ()
endif (APPLE)

include(FindFLEX)
include(FindBISON)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

BISON_TARGET(MyParser "docopt.y" ${CMAKE_CURRENT_BINARY_DIR}/parser.c)
FLEX_TARGET(MyScanner "docopt.l" ${CMAKE_CURRENT_BINARY_DIR}/lexer.c)
ADD_FLEX_BISON_DEPENDENCY(MyScanner MyParser)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(Header_Files "docopt.h") #  "${CMAKE_BINARY_DIR}/config/${PROJECT_NAME}Config.h"
source_group("Header Files" FILES "${Header_Files}")

set(Source_Files "docopt.c")
source_group("Source Files" FILES "${Source_Files}")

add_executable(
        "${EXEC_NAME}"
        main.c
        "${BISON_MyParser_OUTPUTS}"
        "${FLEX_MyScanner_OUTPUTS}"
        "${Header_Files}"
        "${Source_Files}"
)

target_link_libraries("${EXEC_NAME}" PRIVATE "${FLEX_LIBRARIES}")

target_include_directories(
        "${EXEC_NAME}"
        INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:include>"
)

target_link_libraries("${EXEC_NAME}" PUBLIC "${PROJECT_LOWER_NAME}_compiler_flags")

set_target_properties(
        "${EXEC_NAME}"
        PROPERTIES
        LINKER_LANGUAGE
        C
)

#set(_export_file "${CMAKE_CURRENT_SOURCE_DIR}/${EXEC_NAME}_export.h")
#generate_export_header("${EXEC_NAME}" EXPORT_FILE_NAME "${_export_file}")

# setup the version numbering
set_property(TARGET "${EXEC_NAME}" PROPERTY VERSION "1.0.0")
set_property(TARGET "${EXEC_NAME}" PROPERTY SOVERSION "1")

# install rules
set(installable_libs "${EXEC_NAME}" "${PROJECT_LOWER_NAME}_compiler_flags")
if (TARGET "${DEPENDANT_LIBRARY}")
  list(APPEND installable_libs "${DEPENDANT_LIBRARY}")
endif ()
install(TARGETS ${installable_libs}
        DESTINATION "bin"
        EXPORT "${EXEC_NAME}Targets")
#install(FILES "${_export_file}" "${Header_Files}" DESTINATION "include")
