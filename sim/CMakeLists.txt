cmake_minimum_required(VERSION 3.16)
project(PsiUnitUiSim LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../firmware/Psi_Unit_Firmware")

# In cmake-gui: set the folder where you unpacked SDL2-devel-*-VC.zip
# (must contain include/, lib/, and cmake/SDL2Config.cmake).
set(SDL2_ROOT "" CACHE PATH "SDL2 devel root (unzipped SDL2-devel-VC.zip)")
if(SDL2_ROOT)
  list(APPEND CMAKE_PREFIX_PATH "${SDL2_ROOT}")
endif()

find_package(SDL2 REQUIRED)

add_executable(psi_ui_sim
  main.cpp
  sim_host.cpp
  arduino_shim.cpp
  sim_datetime.cpp
  sim_storage.cpp
  sim_rtc.cpp
  sim_domain.cpp
  sim_system_info.cpp
  sim_platform.cpp
  app_sim_setup.cpp
  app_sim_loop.cpp
  u8g2_font_6x10_tr.c
  u8g2_font_draw.cpp
  u8g2_font.c
  u8g2_sim_glue.c
  display_backend_sdl.cpp
  input_backend_sim.cpp
  "${FW_DIR}/display_ui.cpp"
  "${FW_DIR}/ui_input.cpp"
  "${FW_DIR}/settings_field.cpp"
  "${FW_DIR}/settings_runtime.cpp"
  "${FW_DIR}/settings_access.cpp"
  "${FW_DIR}/app_snapshot.cpp"
  "${FW_DIR}/app_commands.cpp"
  "${FW_DIR}/app_effects.cpp"
  "${FW_DIR}/wifi_manager.cpp"
  "${FW_DIR}/wifi_qr.cpp"
  "${FW_DIR}/app_state.cpp"
  "${FW_DIR}/logic.cpp"
)

target_include_directories(psi_ui_sim PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${FW_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

set(SIM_CONFIG_SIM_H "${CMAKE_CURRENT_SOURCE_DIR}/config_sim.h")
target_compile_options(psi_ui_sim PRIVATE
  "$<$<CXX_COMPILER_ID:MSVC>:/FI\"${SIM_CONFIG_SIM_H}\">"
  "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include \"${SIM_CONFIG_SIM_H}\">"
)

target_link_libraries(psi_ui_sim PRIVATE SDL2::SDL2)

if(MSVC)
  target_compile_options(psi_ui_sim PRIVATE /utf-8)
endif()

if(WIN32)
  add_custom_command(TARGET psi_ui_sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:SDL2::SDL2>
      $<TARGET_FILE_DIR:psi_ui_sim>)
endif()
