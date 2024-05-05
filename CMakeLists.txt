cmake_minimum_required(VERSION 3.16)
project(
  dsbexec
  HOMEPAGE_URL https://github.com/mrclksr/DSBExec
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PROGRAM ${CMAKE_PROJECT_NAME})

if(NOT DATADIR)
  set(DATADIR ${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME})
endif(NOT DATADIR)
set(PATH_DOC_URL "https://github.com/mrclksr/DSBExec/blob/master/README.adoc")
set(PATH_LOCALE "${DATADIR}")
set(PATH_DSBSU ${CMAKE_INSTALL_PREFIX}/bin/dsbsu)
if(PROJECT_VERSION)
  set(PROGRAM_VERSION ${PROJECT_VERSION})
else()
  execute_process(
    COMMAND sh -c "git describe --abbrev=12 --always --tags | tr -d '\n'"
    OUTPUT_VARIABLE PROGRAM_VERSION)
  set(PROGRAM_VERSION "git hash: ${PROGRAM_VERSION}")
endif()

set(SOURCE_FILES
    src/mainwin.cpp
    src/mainwin.h
    src/main.cpp
    lib/qt-helper/qt-helper.cpp
    lib/qt-helper/qt-helper.h
    lib/libdsbexec.c
    lib/libdsbexec.h)

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools)

qt_standard_project_setup()

set(TS_FILES locale/dsbexec_de.ts locale/dsbexec_fr.ts)

add_executable(dsbexec ${SOURCE_FILES})

qt_add_translations(dsbexec TS_FILES ${TS_FILES} QM_FILES_OUTPUT_VARIABLE
                    QM_FILES)

target_include_directories(
  dsbexec PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
                 "${CMAKE_CURRENT_SOURCE_DIR}/lib")

configure_file(defs.h.in defs.h @ONLY)

set(LIBS Qt6::Widgets pthread util)

target_link_libraries(dsbexec PRIVATE ${LIBS})

install(TARGETS dsbexec DESTINATION bin)
install(FILES "${CMAKE_PROJECT_NAME}.desktop" DESTINATION share/applications)
install(FILES ${QM_FILES} DESTINATION share/${CMAKE_PROJECT_NAME})
