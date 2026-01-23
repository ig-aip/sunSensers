# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\SolarSensors_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\SolarSensors_autogen.dir\\ParseCache.txt"
  "SolarSensors_autogen"
  )
endif()
