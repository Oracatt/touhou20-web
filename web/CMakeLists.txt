cmake_minimum_required(VERSION 3.20)

project(th20_web_runtime LANGUAGES CXX)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure this project with emcmake so it targets WebAssembly")
endif()

add_executable(th20_web
  src/web_runtime.cpp
  ../source_reconstruction/archive/archive.cpp
)

target_compile_features(th20_web PRIVATE cxx_std_20)
target_include_directories(th20_web PRIVATE
  ../source_reconstruction/archive
)
target_compile_options(th20_web PRIVATE
  -Wall
  -Wextra
  -Wpedantic
  -fexceptions
  -fno-fast-math
  -ffp-contract=off
)

set_target_properties(th20_web PROPERTIES
  OUTPUT_NAME "th20_web"
  SUFFIX ".js"
)

target_link_options(th20_web PRIVATE
  "SHELL:-sWASM=1"
  "SHELL:-sMODULARIZE=1"
  "SHELL:-sEXPORT_NAME=createTH20Module"
  "SHELL:-sENVIRONMENT=web,node,worker"
  "SHELL:-sALLOW_MEMORY_GROWTH=1"
  "SHELL:-sINITIAL_MEMORY=536870912"
  "SHELL:-sMAXIMUM_MEMORY=2147483648"
  "SHELL:-sSTACK_SIZE=8388608"
  "SHELL:-sNO_EXIT_RUNTIME=1"
  "SHELL:-sDISABLE_EXCEPTION_CATCHING=0"
  "SHELL:-sEXCEPTION_STACK_TRACES=1"
  "SHELL:-sASSERTIONS=2"
  "SHELL:-sSTACK_OVERFLOW_CHECK=2"
  "SHELL:-sEXPORTED_FUNCTIONS=['_main','_malloc','_free']"
  "SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','HEAPU8']"
)

foreach(asset IN ITEMS index.html app.js style.css smoke_test.cjs)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${asset}"
                 "${CMAKE_CURRENT_BINARY_DIR}/${asset}" COPYONLY)
endforeach()

# Compile the platform-independent recovered gameplay translation units as a
# separate, explicit coverage target. It is excluded from the default resource
# runtime build while the browser graphics/audio/window backends are connected.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../SOURCE_BUILD_VERIFICATION.json" th20_source_manifest)
string(JSON th20_source_count LENGTH "${th20_source_manifest}" compiled_sources)
math(EXPR th20_source_last "${th20_source_count} - 1")
set(th20_logic_sources)
set(th20_logic_directories
  archive audio_runtime bomb_system bullet_system card_system core_scheduler damage_regions
  ecl_vm effect_system ending_scene game_session gameplay help_system hud_system input
  item_system key_config laser_system notice_system options_system overlay_system pause_system
  platform_services platform_window player_entity program_entry progress_state replay_system
  runtime_core runtime_state screen_effect small_score special_state sprite_renderer
  stage_background stage_clear stage_completion startup_scene stone_menu text_renderer
  title_system trophy_system
)
foreach(index RANGE 0 ${th20_source_last})
  string(JSON relative_path GET "${th20_source_manifest}" compiled_sources ${index})
  if(relative_path MATCHES "^source_reconstruction/([^/]+)/.*\\.cpp$")
    if(CMAKE_MATCH_1 IN_LIST th20_logic_directories)
      list(APPEND th20_logic_sources "${CMAKE_CURRENT_SOURCE_DIR}/../${relative_path}")
    endif()
  elseif(relative_path MATCHES "^src/(binary|ecl|pe)\\.cpp$")
    list(APPEND th20_logic_sources "${CMAKE_CURRENT_SOURCE_DIR}/../${relative_path}")
  endif()
endforeach()

add_library(th20_web_logic OBJECT EXCLUDE_FROM_ALL ${th20_logic_sources})
target_compile_features(th20_web_logic PRIVATE cxx_std_20)
file(GLOB th20_module_paths LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/../source_reconstruction/*")
set(th20_module_directories)
foreach(module_path IN LISTS th20_module_paths)
  if(IS_DIRECTORY "${module_path}")
    list(APPEND th20_module_directories "${module_path}")
  endif()
endforeach()
target_include_directories(th20_web_logic PRIVATE
  compat
  ../include
  ../native_recovered
  ../source_reconstruction
  ${th20_module_directories}
)
target_compile_definitions(th20_web_logic PRIVATE TH20_WEB=1)
target_compile_options(th20_web_logic PRIVATE
  -Wall
  -Wextra
  -Wno-invalid-offsetof
  -Wno-unused-private-field
  -fexceptions
  -fno-fast-math
  -ffp-contract=off
  $<$<CXX_COMPILER_ID:Clang>:-msse>
  $<$<CXX_COMPILER_ID:Clang>:-msse2>
  $<$<CXX_COMPILER_ID:Clang>:-msimd128>
)

# Full recovered game link target. Kept out of the default resource-browser
# build until every native platform dependency has a browser implementation.
add_executable(th20_web_game EXCLUDE_FROM_ALL
  src/game_main.cpp
  src/web_platform.cpp
  src/web_d3d9.cpp
  src/web_dsound.cpp
  $<TARGET_OBJECTS:th20_web_logic>
)
target_compile_features(th20_web_game PRIVATE cxx_std_20)
target_include_directories(th20_web_game PRIVATE
  compat
  ../source_reconstruction
)
target_compile_definitions(th20_web_game PRIVATE TH20_WEB=1)
set_target_properties(th20_web_game PROPERTIES
  OUTPUT_NAME "th20_game"
  SUFFIX ".js"
)
target_link_options(th20_web_game PRIVATE
  "SHELL:-g1"
  "SHELL:-Wl,--error-limit=0"
  "SHELL:-sWASM=1"
  "SHELL:-sMODULARIZE=1"
  "SHELL:-sEXPORT_NAME=createTH20Game"
  "SHELL:-sENVIRONMENT=web,worker"
  "SHELL:-sALLOW_MEMORY_GROWTH=1"
  "SHELL:-sINITIAL_MEMORY=536870912"
  "SHELL:-sMAXIMUM_MEMORY=2147483648"
  "SHELL:-sSTACK_SIZE=8388608"
  "SHELL:-sNO_EXIT_RUNTIME=1"
  "SHELL:-sDISABLE_EXCEPTION_CATCHING=0"
  "SHELL:-sEXCEPTION_STACK_TRACES=1"
  "SHELL:-sASSERTIONS=2"
  "SHELL:-sSTACK_OVERFLOW_CHECK=2"
  "SHELL:-sASYNCIFY=1"
  "SHELL:-sUSE_WEBGL2=1"
  "SHELL:-sFULL_ES3=1"
  "SHELL:-lidbfs.js"
  "SHELL:-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_th20_web_key_event','_th20_web_clear_keys','_th20_web_set_gamepad','_th20_web_set_graphics_options','_th20_web_window_focus','_th20_web_window_close']"
  "SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','FS','IDBFS','HEAPU8','addRunDependency','removeRunDependency']"
)

foreach(asset IN ITEMS game.html game.js game.css)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${asset}"
                 "${CMAKE_CURRENT_BINARY_DIR}/${asset}" COPYONLY)
endforeach()
