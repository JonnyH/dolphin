set(SRCS
		${CMAKE_SOURCE_DIR}/Externals/mtlpp/mtlpp.mm
		main.cpp
)

set(LIBS
  videocommon
  common
)

find_library(METAL_LIBRARY Metal)
include_directories(${CMAKE_SOURCE_DIR}/Externals/mtlpp)

add_dolphin_library(videomtl "${SRCS}" "${LIBS}")
target_link_libraries(videomtl PRIVATE ${METAL_LIBRARY})

