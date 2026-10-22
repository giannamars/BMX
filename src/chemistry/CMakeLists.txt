#
#     Copyright (c) 2013 Battelle Memorial Institute
#     Licensed under modified BSD License. A copy of this license can be found
#     in the LICENSE file in the top level directory of this distribution.
#
target_include_directories( bmxcore
   PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>)

if (BMX_USE_LIBSBML)
   find_package(LibSBML REQUIRED MODULE)
   target_link_libraries(bmxcore PUBLIC LibSBML::LibSBML)
   target_compile_definitions(bmxcore PUBLIC BMX_USE_LIBSBML)
endif ()

if (BMX_USE_ORTOOLS)
   if (NOT BMX_ORTOOLS_ROOT STREQUAL "")
      list(PREPEND CMAKE_PREFIX_PATH "${BMX_ORTOOLS_ROOT}")
   endif ()
   find_package(ortools CONFIG REQUIRED
      HINTS
      "${BMX_ORTOOLS_ROOT}"
      "${BMX_ORTOOLS_ROOT}/lib/cmake/ortools"
      "${BMX_ORTOOLS_ROOT}/share/ortools/cmake")
   target_link_libraries(bmxcore PUBLIC ortools::ortools)
   target_compile_definitions(bmxcore PUBLIC BMX_USE_ORTOOLS)
   target_compile_features(bmxcore PUBLIC cxx_std_20)
endif ()

target_sources( bmxcore
   PRIVATE
   bmx_chem.H
   bmx_chem_K.H
   bmx_cell_interaction.H
   bmx_cell_interaction_K.H
   bmx_chem.cpp
   bmx_fba.cpp
   bmx_cell_interaction.cpp
   )
