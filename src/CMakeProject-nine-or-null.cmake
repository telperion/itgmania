if(NOT MSVC)
  return()
endif()

set(NINE_OR_NULL_DIR "${SM_SRC_DIR}/nine-or-null")

list(APPEND NINE_OR_NULL_SRC
  "${NINE_OR_NULL_DIR}/cli.cpp"
  "${NINE_OR_NULL_DIR}/gui.cpp"
  "Slap.cpp"
)
list(APPEND NINE_OR_NULL_HDR
  "Slap.h"
)

source_group("nine-or-null standalone"
  FILES
  ${NINE_OR_NULL_SRC}
  ${NINE_OR_NULL_HDR}
)

add_executable("nine-or-null-cli"
  "${NINE_OR_NULL_DIR}/cli.cpp"
)
add_executable("nine-or-null-gui"
  "${NINE_OR_NULL_DIR}/gui.cpp"
	spectrogram.cpp
)

find_package(OpenGL REQUIRED)

target_link_libraries(gui OpenGL::GL glfw)

target_include_directories(gui PUBLIC ${imgui_SOURCE_DIR})
target_sources(
	gui
	PRIVATE
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

set_target_properties("nine-or-null"
                      PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                 "${SM_PROGRAM_DIR}"
                                 RUNTIME_OUTPUT_DIRECTORY_RELEASE
                                 "${SM_PROGRAM_DIR}"
                                 RUNTIME_OUTPUT_DIRECTORY_DEBUG
                                 "${SM_PROGRAM_DIR}"
                                 RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL
                                 "${SM_PROGRAM_DIR}"
                                 RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO
                                 "${SM_PROGRAM_DIR}")

set_target_properties("nine-or-null"
                      PROPERTIES OUTPUT_NAME
                                 "nine-or-null"
                                 RELEASE_OUTPUT_NAME
                                 "nine-or-null"
                                 DEBUG_OUTPUT_NAME
                                 "nine-or-null"
                                 MINSIZEREL_OUTPUT_NAME
                                 "nine-or-null"
                                 RELWITHDEBINFO_OUTPUT_NAME
                                 "nine-or-null")
