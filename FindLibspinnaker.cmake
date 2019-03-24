# Copyright (C) 2019  Christian Berger
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

###########################################################################
# Find libspinnaker.
FIND_PATH(SPINNAKER_INCLUDE_DIR
          NAMES Spinnaker.h
          PATHS /usr/include/spinnaker/)
MARK_AS_ADVANCED(SPINNAKER_INCLUDE_DIR)

FIND_LIBRARY(SPINNAKER_LIB
             NAMES Spinnaker
             PATHS ${LIBSPINNAKERDIR}/lib/
                    /usr/lib/
                    /usr/lib64/)
MARK_AS_ADVANCED(SPINNAKER_LIB)

###########################################################################
IF (SPINNAKER_INCLUDE_DIR
    AND SPINNAKER_LIB)
    SET(SPINNAKER_FOUND 1)
    SET(SPINNAKER_LIBRARIES ${SPINNAKER_LIB})
    SET(SPINNAKER_INCLUDE_DIRS ${SPINNAKER_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(SPINNAKER_LIBRARIES)
MARK_AS_ADVANCED(SPINNAKER_INCLUDE_DIRS)

IF (SPINNAKER_FOUND)
    MESSAGE(STATUS "Found libSpinnaker: ${SPINNAKER_INCLUDE_DIRS}, ${SPINNAKER_LIBRARIES}")
ELSE ()
    MESSAGE(STATUS "Could not find libSpinnaker")
ENDIF()
