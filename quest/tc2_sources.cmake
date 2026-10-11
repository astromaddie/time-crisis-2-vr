set(TC "${ROOT}/upstream/tc2")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(TC2_OVERLAY "${CMAKE_BINARY_DIR}/tc2-overlay")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${ROOT}/tools/tc2_overlay.py" "${TC2_OVERLAY}" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ROOT}/tools/tc2_overlay.py")
set(TC2_EXTRA_INCLUDES "${ROOT}/link")
file(GLOB LIFTED "${TC}/gen/tc2_lifted_*.c")
set_source_files_properties(${LIFTED} PROPERTIES COMPILE_OPTIONS "-O1;-w;-include;${TC}/include/tc2_mips_rt.h")
set_source_files_properties("${TC2_OVERLAY}/tc2_main.c" PROPERTIES COMPILE_DEFINITIONS "main=tc_game_main;SDL_MAIN_HANDLED")
set(TC2_SOURCES
  "${ROOT}/link/tc2_link.c"
  "${TC}/src/tc2_cpu.c" "${TC}/src/tc2_mem.c" "${TC}/src/tc2_ctl.c"
  "${TC2_OVERLAY}/tc2_chips.c" "${TC}/src/tc2_screen.c" "${TC}/src/tc2_text.c"
  "${TC2_OVERLAY}/tc2_main.c" "${TC}/src/tc2_input.c" "${TC}/src/tc2_c435.c"
  "${TC}/src/tc2_render.c" "${TC}/src/h8_periph.c" "${TC2_OVERLAY}/tc2_h8.c"
  "${TC}/src/tc2_audio.c" "${TC}/src/tc2_rom.c" "${TC}/src/eng/romzip.c"
  "${TC}/src/eng/c352.c" "${TC}/gen/tc2_sub.c" "${TC}/gen/tc2_io.c" ${LIFTED}
  "${ROOT}/quest/tc2_window.c" "${ROOT}/quest/tc2_gl.c"
  "${ROOT}/quest/quest_host.c" "${ROOT}/quest/quest_gl.c" "${ROOT}/quest/tc2_scene.c"
  "${ROOT}/quest/quest_gun.c" "${ROOT}/quest/quest_options.c" "${ROOT}/quest/quest_cover.c" "${ROOT}/quest/quest_ui.c")
