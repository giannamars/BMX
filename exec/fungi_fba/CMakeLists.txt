#
#     Copyright (c) 2013 Battelle Memorial Institute
#     Licensed under modified BSD License. A copy of this license can be found
#     in the LICENSE file in the top level directory of this distribution.
#

configure_file(
  ${PROJECT_SOURCE_DIR}/exec/fungi_fba/input_fungi_fba
  ${CMAKE_CURRENT_BINARY_DIR}/input_fungi_fba
)

configure_file(
  ${PROJECT_SOURCE_DIR}/exec/fungi_fba/MODEL1604280024.xml
  ${CMAKE_CURRENT_BINARY_DIR}/MODEL1604280024.xml
  COPYONLY
)

configure_file(
  ${PROJECT_SOURCE_DIR}/exec/fungi/fungi_init_cfg.dat
  ${CMAKE_CURRENT_BINARY_DIR}/fungi_init_cfg.dat
)

