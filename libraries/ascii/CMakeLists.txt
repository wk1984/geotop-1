PROJECT(ascii)

IF(COMMAND CMAKE_POLICY)
        CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

ADD_LIBRARY( ascii ${LINKING_MODE}
		rw_maps.cc
		rw_maps.h   
		tabs.cc 
		tabs.h
)

TARGET_LINK_LIBRARIES( ascii
	${FLUIDTURTLE_LIBRARIES}
	${METEOIO_LIBRARIES}
	${METEOIO_PLUGIN_LIBRARIES}
#	${GEOTOP_COMMON_LIBRARIES}
#	${GT_UTILITIES_LIBRARIES}
#	${NETCFD_LIBRARIES}
#	${WRF_LIBRARIES}
)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

