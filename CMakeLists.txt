cmake_minimum_required(VERSION 3.20)
project(NovaArcadeHD VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------- SDL3
# The browser build uses Emscripten's SDL3 port. Elsewhere, an installed SDL3 is used if CMake can
# find one (point CMAKE_PREFIX_PATH at it); otherwise SDL3 is downloaded and built statically, so the
# Mac app has no outside dependencies.
option(NOVA_WEB_SDL_PORT "Browser build: use Emscripten's SDL3 port (off = find an SDL3 built for wasm)" ON)
if(EMSCRIPTEN AND NOVA_WEB_SDL_PORT)
  add_library(SDL3_port INTERFACE)
  target_compile_options(SDL3_port INTERFACE --use-port=sdl3)
  target_link_options(SDL3_port INTERFACE --use-port=sdl3)
  set(SDL_LIB SDL3_port)
else()
  find_package(SDL3 CONFIG QUIET)
  if(SDL3_FOUND)
    if(TARGET SDL3::SDL3-static)
      set(SDL_LIB SDL3::SDL3-static)
    else()
      set(SDL_LIB SDL3::SDL3)
    endif()
    message(STATUS "Using installed SDL3 (${SDL_LIB})")
  else()
    message(STATUS "Fetching SDL3")
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.2.24 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
    set(SDL_LIB SDL3::SDL3-static)
  endif()
endif()

# ---------------------------------------------------------------- the engine and the games
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/games/*/game.cpp)
add_library(novacore STATIC
  engine/gfx.cpp
  engine/audio.cpp
  engine/system.cpp
  games/games.cpp
  ${GAME_SOURCES}
)
target_include_directories(novacore PUBLIC engine games)
target_link_libraries(novacore PUBLIC ${SDL_LIB})
if(NOT MSVC)
  target_compile_options(novacore PRIVATE -Wall -Wno-unused-function)
endif()

set(ASSETS_DIR ${CMAKE_SOURCE_DIR}/assets)

# ---------------------------------------------------------------- the app
if(EMSCRIPTEN)
  add_executable(nova engine/platform.cpp)
  target_link_libraries(nova PRIVATE novacore)
  set_target_properties(nova PROPERTIES SUFFIX ".js")
  target_link_options(nova PRIVATE
    --preload-file ${ASSETS_DIR}@/assets
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=134217728
    -sENVIRONMENT=web
    -sEXPORTED_RUNTIME_METHODS=['stringToNewUTF8']
  )
  # relink (repack nova.data) whenever an asset changes
  file(GLOB_RECURSE ASSET_FILES CONFIGURE_DEPENDS ${ASSETS_DIR}/*)
  set_target_properties(nova PROPERTIES LINK_DEPENDS "${ASSET_FILES}")
elseif(APPLE)
  set(MACOSX_BUNDLE_BUNDLE_NAME "Nova Arcade HD")
  set(MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.drhammadkhan.nova-arcade-hd")
  set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
  set(MACOSX_BUNDLE_ICON_FILE NovaArcadeHD.icns)
  set(ICON ${CMAKE_SOURCE_DIR}/platform/mac/NovaArcadeHD.icns)
  set_source_files_properties(${ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  add_executable(nova MACOSX_BUNDLE engine/platform.cpp ${ICON})
  set_target_properties(nova PROPERTIES
    OUTPUT_NAME "Nova Arcade HD"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/platform/mac/Info.plist.in)
  target_link_libraries(nova PRIVATE novacore)
  # the assets folder goes inside the app: Contents/Resources/assets (SDL_GetBasePath points there)
  add_custom_command(TARGET nova POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIR} "$<TARGET_BUNDLE_CONTENT_DIR:nova>/Resources/assets")
else()
  add_executable(nova engine/platform.cpp)
  target_link_libraries(nova PRIVATE novacore)
  add_custom_command(TARGET nova POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIR} $<TARGET_FILE_DIR:nova>/assets)
endif()

# ---------------------------------------------------------------- tests (headless: no window, no sound)
if(NOT EMSCRIPTEN)
  add_executable(test_pixelpeaks tests/test_pixelpeaks.cpp engine/headless.cpp)
  target_link_libraries(test_pixelpeaks PRIVATE novacore)
  add_executable(render_music tests/render_music.cpp engine/headless.cpp)
  target_link_libraries(render_music PRIVATE novacore)
  target_compile_definitions(test_pixelpeaks PRIVATE ASSETS="${ASSETS_DIR}")
  target_compile_definitions(render_music PRIVATE ASSETS="${ASSETS_DIR}")
  add_executable(test_games tests/test_games.cpp engine/headless.cpp)
  target_link_libraries(test_games PRIVATE novacore)
  target_compile_definitions(test_games PRIVATE ASSETS="${ASSETS_DIR}")
endif()
