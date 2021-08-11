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

BISON_TARGET(
        MyParser
        "${CMAKE_CURRENT_SOURCE_DIR}/docopt.y"
        "${CMAKE_CURRENT_BINARY_DIR}/docopt.tab.c"
        COMPILE_FLAGS "-v -gdocopt.grm.dot --defines --debug"
)
FLEX_TARGET(
        MyScanner
        "${CMAKE_CURRENT_SOURCE_DIR}/docopt.l"
        "${CMAKE_CURRENT_BINARY_DIR}/docopt.lex.c"
)

ADD_FLEX_BISON_DEPENDENCY(MyScanner MyParser)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

set(Header_Files "docopt.h") #  "${CMAKE_BINARY_DIR}/config/${PROJECT_NAME}Config.h"
source_group("Header Files"
             FILES
             "${CMAKE_BINARY_DIR}/docopt.tab.c" "${CMAKE_BINARY_DIR}/docopt.lex.c" "docopt.c")

set(Source_Files "docopt.c")
source_group("Source Files" FILES "${Source_Files}")

# TODO: Remove this hack
file(GLOB all_c_files *.h *.c)
file(COPY ${all_c_files} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

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
install(FILES "${Header_Files}" DESTINATION "include")
