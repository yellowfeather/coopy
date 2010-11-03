SET(CPACK_PACKAGE_VERSION_MAJOR "${COOPY_VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${COOPY_VERSION_MINOR}")
SET(CPACK_PACKAGE_VERSION_PATCH "${COOPY_VERSION_PATCH}${COOPY_VERSION_MODIFIER}")

SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Coopy")
SET(CPACK_PACKAGE_NAME "coopy")
SET(CPACK_PACKAGE_VENDOR "DCP")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.txt")
SET(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.txt")
SET(CPACK_RESOURCE_FILE_WELCOME "${CMAKE_SOURCE_DIR}/conf/WELCOME.txt")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/GPL.txt")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
IF (WIN32 OR APPLE)
  SET(CPACK_PACKAGE_FILE_NAME 
  "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
ELSE ()
  SET(CPACK_PACKAGE_FILE_NAME 
  "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-bin")
ENDIF ()

SET(CPACK_CREATE_DESKTOP_LINKS "coopy")
SET(CPACK_NSIS_DISPLAY_NAME "Coopy")
SET(CPACK_NSIS_URL_INFO_ABOUT "http://coopy.sourceforge.net")

SET(CPACK_STRIP_FILES ON)
SET(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "coopy")
SET(CPACK_SOURCE_IGNORE_FILES ${CPACK_SOURCE_IGNORE_FILES} "~$" ".svn$" ".svn/.*$" ".git$" ".git/.*$")

IF (WIN32)
  SET(CPACK_GENERATOR "NSIS;ZIP")
ELSEIF (WIN32)
  SET(CPACK_GENERATOR "TGZ")
ENDIF (WIN32)

SET(CPACK_PACKAGE_EXECUTABLES "coopy;coopy;ssmerge_gui;ssmerge_gui")
SET(CPACK_NSIS_INSTALLED_ICON_NAME "bin/coopy.exe")
SET(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/src/gui/icon/appicon.ico")

SET(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
\\\${registerExtension} \\\"\\\$INSTDIR\\\\bin\\\\coopy.exe\\\" \\\".coopy\\\" \\\"Coopy repository\\\"
  ;; refresh shell icons, RefreshShellIcons
  !define SHCNE_ASSOCCHANGED 0x08000000
  !define SHCNF_IDLIST 0
  System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v (\\\${SHCNE_ASSOCCHANGED}, \\\${SHCNF_IDLIST}, 0, 0)'
")
SET(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
\\\${unregisterExtension} \\\".coopy\\\" \\\"Coopy repository\\\"
")

# we need to override the NSIS setup a bit
set(CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/nsis" ${CMAKE_MODULE_PATH})
set(AT "@")
configure_file(${CMAKE_SOURCE_DIR}/conf/NSIS.template.in
               ${CMAKE_BINARY_DIR}/nsis/NSIS.template.in @ONLY IMMEDIATE)
INCLUDE(CPack)
