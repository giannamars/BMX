#[=======================================================================[.rst:
FindLibSBML
-----------

Find a libSBML installation and define an imported target.

This module looks for a pkg-config or manual installation and exposes:

  LibSBML_FOUND
  LibSBML_INCLUDE_DIRS
  LibSBML_LIBRARIES
  LibSBML::LibSBML

Cache hints:

  LIBSBML_ROOT
  BMX_LIBSBML_ROOT
#]=======================================================================]

find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
   pkg_check_modules(PC_LIBSBML QUIET libsbml)
endif ()

set(_libsbml_root_hints)
if (DEFINED BMX_LIBSBML_ROOT AND NOT BMX_LIBSBML_ROOT STREQUAL "")
   list(APPEND _libsbml_root_hints "${BMX_LIBSBML_ROOT}")
endif ()
if (DEFINED LIBSBML_ROOT AND NOT LIBSBML_ROOT STREQUAL "")
   list(APPEND _libsbml_root_hints "${LIBSBML_ROOT}")
endif ()
if (DEFINED ENV{LIBSBML_ROOT} AND NOT "$ENV{LIBSBML_ROOT}" STREQUAL "")
   list(APPEND _libsbml_root_hints "$ENV{LIBSBML_ROOT}")
endif ()

find_path(LibSBML_INCLUDE_DIR
   NAMES sbml/SBMLDocument.h sbml/SBMLTypes.h
   HINTS ${_libsbml_root_hints} ${PC_LIBSBML_INCLUDE_DIRS}
   PATH_SUFFIXES include
   )

find_library(LibSBML_LIBRARY
   NAMES sbml libsbml
   HINTS ${_libsbml_root_hints} ${PC_LIBSBML_LIBRARY_DIRS}
   PATH_SUFFIXES lib lib64
   )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibSBML
   REQUIRED_VARS LibSBML_INCLUDE_DIR LibSBML_LIBRARY
   )

if (LibSBML_FOUND)
   set(LibSBML_INCLUDE_DIRS ${LibSBML_INCLUDE_DIR})
   set(LibSBML_LIBRARIES ${LibSBML_LIBRARY})

   if (NOT TARGET LibSBML::LibSBML)
      add_library(LibSBML::LibSBML UNKNOWN IMPORTED)
      set_target_properties(LibSBML::LibSBML PROPERTIES
         IMPORTED_LOCATION "${LibSBML_LIBRARY}"
         INTERFACE_INCLUDE_DIRECTORIES "${LibSBML_INCLUDE_DIR}"
         )

      if (PC_LIBSBML_LIBRARIES)
         set_target_properties(LibSBML::LibSBML PROPERTIES
            INTERFACE_LINK_LIBRARIES "${PC_LIBSBML_LIBRARIES}"
            )
      endif ()
   endif ()
endif ()

mark_as_advanced(LibSBML_INCLUDE_DIR LibSBML_LIBRARY)
