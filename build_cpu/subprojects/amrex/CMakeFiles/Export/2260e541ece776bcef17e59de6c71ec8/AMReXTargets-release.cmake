#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "AMReX::amrex" for configuration "Release"
set_property(TARGET AMReX::amrex APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(AMReX::amrex PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libamrex.a"
  )

list(APPEND _cmake_import_check_targets AMReX::amrex )
list(APPEND _cmake_import_check_files_for_AMReX::amrex "${_IMPORT_PREFIX}/lib/libamrex.a" )

# Import target "AMReX::fboxinfo" for configuration "Release"
set_property(TARGET AMReX::fboxinfo APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(AMReX::fboxinfo PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/fboxinfo"
  )

list(APPEND _cmake_import_check_targets AMReX::fboxinfo )
list(APPEND _cmake_import_check_files_for_AMReX::fboxinfo "${_IMPORT_PREFIX}/bin/fboxinfo" )

# Import target "AMReX::fcompare" for configuration "Release"
set_property(TARGET AMReX::fcompare APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(AMReX::fcompare PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/fcompare"
  )

list(APPEND _cmake_import_check_targets AMReX::fcompare )
list(APPEND _cmake_import_check_files_for_AMReX::fcompare "${_IMPORT_PREFIX}/bin/fcompare" )

# Import target "AMReX::fextract" for configuration "Release"
set_property(TARGET AMReX::fextract APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(AMReX::fextract PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/fextract"
  )

list(APPEND _cmake_import_check_targets AMReX::fextract )
list(APPEND _cmake_import_check_files_for_AMReX::fextract "${_IMPORT_PREFIX}/bin/fextract" )

# Import target "AMReX::fextrema" for configuration "Release"
set_property(TARGET AMReX::fextrema APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(AMReX::fextrema PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/fextrema"
  )

list(APPEND _cmake_import_check_targets AMReX::fextrema )
list(APPEND _cmake_import_check_files_for_AMReX::fextrema "${_IMPORT_PREFIX}/bin/fextrema" )

# Import target "AMReX::fnan" for configuration "Release"
set_property(TARGET AMReX::fnan APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(AMReX::fnan PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/fnan"
  )

list(APPEND _cmake_import_check_targets AMReX::fnan )
list(APPEND _cmake_import_check_files_for_AMReX::fnan "${_IMPORT_PREFIX}/bin/fnan" )

# Import target "AMReX::fsnapshot" for configuration "Release"
set_property(TARGET AMReX::fsnapshot APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(AMReX::fsnapshot PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/fsnapshot"
  )

list(APPEND _cmake_import_check_targets AMReX::fsnapshot )
list(APPEND _cmake_import_check_files_for_AMReX::fsnapshot "${_IMPORT_PREFIX}/bin/fsnapshot" )

# Import target "AMReX::ftime" for configuration "Release"
set_property(TARGET AMReX::ftime APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(AMReX::ftime PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/ftime"
  )

list(APPEND _cmake_import_check_targets AMReX::ftime )
list(APPEND _cmake_import_check_files_for_AMReX::ftime "${_IMPORT_PREFIX}/bin/ftime" )

# Import target "AMReX::fvarnames" for configuration "Release"
set_property(TARGET AMReX::fvarnames APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(AMReX::fvarnames PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/fvarnames"
  )

list(APPEND _cmake_import_check_targets AMReX::fvarnames )
list(APPEND _cmake_import_check_files_for_AMReX::fvarnames "${_IMPORT_PREFIX}/bin/fvarnames" )

# Import target "AMReX::fvolumesum" for configuration "Release"
set_property(TARGET AMReX::fvolumesum APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(AMReX::fvolumesum PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/fvolumesum"
  )

list(APPEND _cmake_import_check_targets AMReX::fvolumesum )
list(APPEND _cmake_import_check_files_for_AMReX::fvolumesum "${_IMPORT_PREFIX}/bin/fvolumesum" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
