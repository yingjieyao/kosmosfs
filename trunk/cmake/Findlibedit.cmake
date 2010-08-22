FIND_PATH(LIBEDIT_INCLUDE_DIR readline.h /usr/include/editline /usr/local/include/editline)

FIND_LIBRARY(LIBEDIT_LIBRARY NAMES edit PATH /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64) 

IF (LIBEDIT_INCLUDE_DIR AND LIBEDIT_LIBRARY)
   SET(LIBEDIT_FOUND TRUE)
ENDIF (LIBEDIT_INCLUDE_DIR AND LIBEDIT_LIBRARY)

IF (LIBEDIT_FOUND)
   IF (NOT libedit_FIND_QUIETLY)
      MESSAGE(STATUS "Found libedit: ${LIBEDIT_LIBRARY}")
   ENDIF (NOT libedit_FIND_QUIETLY)
   ADD_DEFINITIONS(-DHAVE_LIBEDIT=1)
ELSE (LIBEDIT_FOUND)
   IF (libedit_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find libedit")
   ENDIF (libedit_FIND_REQUIRED)
ENDIF (LIBEDIT_FOUND)
