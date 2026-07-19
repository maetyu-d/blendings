cmake_minimum_required(VERSION 3.22)

project(Blendings VERSION 0.7.9 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_PATH "" CACHE PATH "Path to a JUCE checkout")

if(NOT JUCE_PATH AND DEFINED ENV{JUCE_PATH})
    set(JUCE_PATH "$ENV{JUCE_PATH}")
endif()

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE_PATH must point to a JUCE checkout. Current value: ${JUCE_PATH}")
endif()

add_subdirectory("${JUCE_PATH}" JUCE)

set(BLENDINGS_SC_ROOT "" CACHE PATH "Path to the SuperCollider host runtime")

if(NOT BLENDINGS_SC_ROOT AND DEFINED ENV{BLENDINGS_SC_ROOT})
    set(BLENDINGS_SC_ROOT "$ENV{BLENDINGS_SC_ROOT}")
endif()

set(SUPERCOLLIDER_INCLUDE_ROOT "${BLENDINGS_SC_ROOT}/third_party/supercollider/include")

set(PURE_DATA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/src")
set(OTHERWARE_HAS_LIBPD FALSE)

if(EXISTS "${PURE_DATA_SRC_DIR}/z_libpd.c")
    set(OTHERWARE_HAS_LIBPD TRUE)
    set(PD_CORE_SOURCES
        d_arithmetic.c d_array.c d_ctl.c d_dac.c d_delay.c d_fft.c
        d_fft_fftsg.c d_filter.c d_global.c d_math.c d_misc.c d_osc.c
        d_resample.c d_soundfile.c d_soundfile_aiff.c d_soundfile_caf.c
        d_soundfile_next.c d_soundfile_wave.c d_ugen.c
        g_all_guis.c g_array.c g_bang.c g_canvas.c g_clone.c g_editor.c
        g_editor_extras.c g_graph.c g_guiconnect.c g_io.c g_mycanvas.c
        g_numbox.c g_radio.c g_readwrite.c g_rtext.c g_scalar.c g_slider.c
        g_template.c g_text.c g_toggle.c g_traversal.c g_undo.c g_vumeter.c
        m_atom.c m_binbuf.c m_class.c m_conf.c m_glob.c m_memory.c m_obj.c
        m_pd.c m_sched.c
        s_audio.c s_audio_dummy.c s_inter.c s_inter_gui.c s_loader.c s_main.c
        s_net.c s_path.c s_print.c s_utf8.c
        x_acoustics.c x_arithmetic.c x_array.c x_connective.c x_file.c x_gui.c
        x_interface.c x_list.c x_midi.c x_misc.c x_net.c x_scalar.c x_text.c
        x_time.c x_vexp.c x_vexp_if.c x_vexp_fun.c
        s_libpdmidi.c x_libpdreceive.c z_hooks.c z_libpd.c
        z_print_util.c z_queued.c z_ringbuffer.c)
    list(TRANSFORM PD_CORE_SOURCES PREPEND "${PURE_DATA_SRC_DIR}/")

    set(PD_EXTRA_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra/bob~/bob~.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra/bonk~/bonk~.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra/choice/choice.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra/fiddle~/fiddle~.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra/loop~/loop~.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra/lrshift~/lrshift~.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra/pd~/pdsched.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra/pd~/pd~.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra/pique/pique.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra/sigmund~/sigmund~.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra/stdout/stdout.c")

    add_library(otherware_libpd STATIC ${PD_CORE_SOURCES} ${PD_EXTRA_SOURCES})
    target_include_directories(otherware_libpd PUBLIC "${PURE_DATA_SRC_DIR}")
    target_compile_definitions(otherware_libpd PRIVATE
        PD
        USEAPI_DUMMY
        PD_INTERNAL
        HAVE_UNISTD_H
        LIBPD_EXTRA
        PDINSTANCE)

    if(APPLE)
        target_compile_definitions(otherware_libpd PRIVATE
            HAVE_ALLOCA_H
            HAVE_LIBDL
            HAVE_MACHINE_ENDIAN_H
            _DARWIN_C_SOURCE
            _DARWIN_UNLIMITED_SELECT
            FD_SETSIZE=10240)
    elseif(UNIX)
        target_compile_definitions(otherware_libpd PRIVATE
            HAVE_ALLOCA_H
            HAVE_LIBDL
            HAVE_ENDIAN_H)
    endif()

    target_compile_options(otherware_libpd PRIVATE -fPIC -ffast-math)
endif()

juce_add_gui_app(Blendings
    PRODUCT_NAME "Blendings"
    COMPANY_NAME "matd.space"
    COMPANY_COPYRIGHT "Copyright (c) 2026 matd.space"
    BUNDLE_ID "space.matd.blendings"
    VERSION "${PROJECT_VERSION}"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Blendings uses audio input for Pure Data adc~ objects.")

juce_generate_juce_header(Blendings)

target_sources(Blendings PRIVATE
    Source/AppTheme.cpp
    Source/AppTheme.h
    Source/EmbeddedScAudioEngine.cpp
    Source/EmbeddedScAudioEngine.h
    Source/CarouselEditorComponent.cpp
    Source/CarouselEditorComponent.h
    Source/GridEditorComponent.cpp
    Source/GridEditorComponent.h
    Source/GridInterpreter.cpp
    Source/GridInterpreter.h
    Source/GridModel.cpp
    Source/GridModel.h
    Source/InternalEvent.h
    Source/MusicalObjectEditorComponent.cpp
    Source/MusicalObjectEditorComponent.h
    Source/PdAudioEngine.cpp
    Source/PdAudioEngine.h
    Source/PipeWorkspaceComponent.cpp
    Source/PipeWorkspaceComponent.h
    Source/PipeToolSettings.h
    Source/ScSheetFormula.cpp
    Source/ScSheetFormula.h
    Source/ScSheetMath.cpp
    Source/ScSheetMath.h
    Source/ScSheetScore.cpp
    Source/ScSheetScore.h
    Source/ScSheetTableComponent.cpp
    Source/ScSheetTableComponent.h
    Source/ScDiscAudioEngine.cpp
    Source/ScDiscAudioEngine.h
    Source/SuperColliderTokeniser.cpp
    Source/SuperColliderTokeniser.h
    Source/WorkspaceModel.cpp
    Source/WorkspaceModel.h
    Source/WorkspaceInspectors.h
    Source/Main.cpp)

target_compile_definitions(Blendings PRIVATE
    OTHERWARE_HAS_LIBPD=$<BOOL:${OTHERWARE_HAS_LIBPD}>
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    OTHERWARE_PD_EDITOR_HTML="${CMAKE_CURRENT_SOURCE_DIR}/Source/PdPatchEditor.html"
    OTHERWARE_PD_EXTRA_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra"
    GRIDCOLLIDER_ALCHEMY_SC_ROOT="${BLENDINGS_SC_ROOT}")

# The editor is loaded as a runtime resource. Make its timestamp participate in
# the link step so the post-build bundle copy cannot remain stale after HTML-only edits.
set_property(TARGET Blendings APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/PdPatchEditor.html")

add_custom_command(TARGET Blendings POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Blendings>/../Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/PdPatchEditor.html"
        "$<TARGET_FILE_DIR:Blendings>/../Resources/PdPatchEditor.html"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "$<TARGET_FILE_DIR:Blendings>/../Resources/LICENSE"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "$<TARGET_FILE_DIR:Blendings>/../Resources/THIRD_PARTY_NOTICES.md")

# JUCE applies an ad-hoc linker signature before post-build resources are copied.
# Reseal the complete bundle so local and release-candidate app bundles verify.
if(APPLE)
    add_custom_command(TARGET Blendings POST_BUILD
        COMMAND /usr/bin/codesign --force --deep --sign -
            "$<TARGET_FILE_DIR:Blendings>/../..")
endif()

if (EXISTS "${SUPERCOLLIDER_INCLUDE_ROOT}/server/SC_WorldOptions.h")
    target_include_directories(Blendings SYSTEM PRIVATE
        "${SUPERCOLLIDER_INCLUDE_ROOT}/common"
        "${SUPERCOLLIDER_INCLUDE_ROOT}/plugin_interface"
        "${SUPERCOLLIDER_INCLUDE_ROOT}/server")
    target_compile_definitions(Blendings PRIVATE GRIDCOLLIDER_HAS_SC_HEADERS=1)
endif()

target_link_libraries(Blendings PRIVATE
    juce::juce_audio_utils
    juce::juce_data_structures
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(OTHERWARE_HAS_LIBPD)
    target_link_libraries(Blendings PRIVATE otherware_libpd)
endif()

juce_add_console_app(PlaybackEndToEndSmoke
    PRODUCT_NAME "Blendings Playback End-to-End Smoke")
juce_generate_juce_header(PlaybackEndToEndSmoke)
target_sources(PlaybackEndToEndSmoke PRIVATE
    Source/AppTheme.cpp Source/AppTheme.h
    Source/EmbeddedScAudioEngine.cpp Source/EmbeddedScAudioEngine.h
    Source/CarouselEditorComponent.cpp Source/CarouselEditorComponent.h
    Source/GridEditorComponent.cpp Source/GridEditorComponent.h
    Source/GridInterpreter.cpp Source/GridInterpreter.h
    Source/GridModel.cpp Source/GridModel.h
    Source/InternalEvent.h
    Source/MusicalObjectEditorComponent.cpp Source/MusicalObjectEditorComponent.h
    Source/PdAudioEngine.cpp Source/PdAudioEngine.h
    Source/PipeWorkspaceComponent.cpp Source/PipeWorkspaceComponent.h
    Source/ScSheetFormula.cpp Source/ScSheetFormula.h
    Source/ScSheetMath.cpp Source/ScSheetMath.h
    Source/ScSheetScore.cpp Source/ScSheetScore.h
    Source/ScSheetTableComponent.cpp Source/ScSheetTableComponent.h
    Source/ScDiscAudioEngine.cpp Source/ScDiscAudioEngine.h
    Source/SuperColliderTokeniser.cpp Source/SuperColliderTokeniser.h
    Source/WorkspaceModel.cpp Source/WorkspaceModel.h
    Source/PipeToolSettings.h
    Source/WorkspaceInspectors.h
    Source/Main.cpp)
target_compile_definitions(PlaybackEndToEndSmoke PRIVATE
    BLENDINGS_PLAYBACK_SMOKE=1
    OTHERWARE_HAS_LIBPD=$<BOOL:${OTHERWARE_HAS_LIBPD}>
    JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0
    OTHERWARE_PD_EDITOR_HTML="${CMAKE_CURRENT_SOURCE_DIR}/Source/PdPatchEditor.html"
    OTHERWARE_PD_EXTRA_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra"
    GRIDCOLLIDER_ALCHEMY_SC_ROOT="${BLENDINGS_SC_ROOT}")
if (EXISTS "${SUPERCOLLIDER_INCLUDE_ROOT}/server/SC_WorldOptions.h")
    target_include_directories(PlaybackEndToEndSmoke SYSTEM PRIVATE
        "${SUPERCOLLIDER_INCLUDE_ROOT}/common"
        "${SUPERCOLLIDER_INCLUDE_ROOT}/plugin_interface"
        "${SUPERCOLLIDER_INCLUDE_ROOT}/server")
    target_compile_definitions(PlaybackEndToEndSmoke PRIVATE GRIDCOLLIDER_HAS_SC_HEADERS=1)
endif()
target_link_libraries(PlaybackEndToEndSmoke PRIVATE
    juce::juce_audio_utils juce::juce_data_structures juce::juce_gui_extra
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(OTHERWARE_HAS_LIBPD)
    target_link_libraries(PlaybackEndToEndSmoke PRIVATE otherware_libpd)
endif()

enable_testing()
add_test(NAME PlaybackEndToEndSmoke
    COMMAND PlaybackEndToEndSmoke "${CMAKE_CURRENT_SOURCE_DIR}/Demos/Demo1.otherware")

add_executable(SCAudioSmoke EXCLUDE_FROM_ALL
    Source/EmbeddedScAudioEngine.cpp
    Source/EmbeddedScAudioEngine.h
    Source/GridInterpreter.cpp
    Source/GridInterpreter.h
    Source/GridModel.cpp
    Source/GridModel.h
    Source/InternalEvent.h
    Source/PdAudioEngine.cpp
    Source/PdAudioEngine.h
    Source/ScAudioSmoke.cpp
    Source/ScSheetFormula.cpp
    Source/ScSheetFormula.h
    Source/ScSheetMath.cpp
    Source/ScSheetMath.h
    Source/ScSheetScore.cpp
    Source/ScSheetScore.h
    Source/ScDiscAudioEngine.cpp
    Source/ScDiscAudioEngine.h)

target_compile_definitions(SCAudioSmoke PRIVATE
    OTHERWARE_HAS_LIBPD=$<BOOL:${OTHERWARE_HAS_LIBPD}>
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    OTHERWARE_PD_EXTRA_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra"
    GRIDCOLLIDER_ALCHEMY_SC_ROOT="${BLENDINGS_SC_ROOT}")

if (EXISTS "${SUPERCOLLIDER_INCLUDE_ROOT}/server/SC_WorldOptions.h")
    target_include_directories(SCAudioSmoke SYSTEM PRIVATE
        "${SUPERCOLLIDER_INCLUDE_ROOT}/common"
        "${SUPERCOLLIDER_INCLUDE_ROOT}/plugin_interface"
        "${SUPERCOLLIDER_INCLUDE_ROOT}/server")
    target_compile_definitions(SCAudioSmoke PRIVATE GRIDCOLLIDER_HAS_SC_HEADERS=1)
endif()

target_link_libraries(SCAudioSmoke PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

if(OTHERWARE_HAS_LIBPD)
    target_link_libraries(SCAudioSmoke PRIVATE otherware_libpd)
endif()

add_executable(AudioRobustnessSmoke EXCLUDE_FROM_ALL
    Source/AudioRobustnessSmoke.cpp
    Source/EmbeddedScAudioEngine.cpp
    Source/EmbeddedScAudioEngine.h
    Source/GridInterpreter.cpp
    Source/GridInterpreter.h
    Source/GridModel.cpp
    Source/GridModel.h
    Source/InternalEvent.h
    Source/PdAudioEngine.cpp
    Source/PdAudioEngine.h
    Source/ScDiscAudioEngine.cpp
    Source/ScDiscAudioEngine.h
    Source/ScSheetFormula.cpp
    Source/ScSheetFormula.h
    Source/ScSheetMath.cpp
    Source/ScSheetMath.h
    Source/ScSheetScore.cpp
    Source/ScSheetScore.h)
target_compile_definitions(AudioRobustnessSmoke PRIVATE
    OTHERWARE_HAS_LIBPD=$<BOOL:${OTHERWARE_HAS_LIBPD}>
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    OTHERWARE_PD_EXTRA_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/third_party/pure-data/extra"
    GRIDCOLLIDER_ALCHEMY_SC_ROOT="${BLENDINGS_SC_ROOT}")
if (EXISTS "${SUPERCOLLIDER_INCLUDE_ROOT}/server/SC_WorldOptions.h")
    target_include_directories(AudioRobustnessSmoke SYSTEM PRIVATE
        "${SUPERCOLLIDER_INCLUDE_ROOT}/common"
        "${SUPERCOLLIDER_INCLUDE_ROOT}/plugin_interface"
        "${SUPERCOLLIDER_INCLUDE_ROOT}/server")
    target_compile_definitions(AudioRobustnessSmoke PRIVATE GRIDCOLLIDER_HAS_SC_HEADERS=1)
endif()
target_link_libraries(AudioRobustnessSmoke PRIVATE
    juce::juce_audio_basics juce::juce_core juce::juce_data_structures
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(OTHERWARE_HAS_LIBPD)
    target_link_libraries(AudioRobustnessSmoke PRIVATE otherware_libpd)
endif()

add_executable(CarouselSmoke EXCLUDE_FROM_ALL
    Source/CarouselEditorComponent.cpp
    Source/CarouselEditorComponent.h
    Source/MusicalObjectEditorComponent.cpp
    Source/MusicalObjectEditorComponent.h
    Source/CarouselSmoke.cpp)
target_compile_definitions(CarouselSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(CarouselSmoke PRIVATE
    juce::juce_gui_extra
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

add_executable(ProjectRecoverySmoke EXCLUDE_FROM_ALL Source/ProjectRecoverySmoke.cpp)
target_compile_definitions(ProjectRecoverySmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ProjectRecoverySmoke PRIVATE
    juce::juce_gui_basics juce::juce_data_structures
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

add_executable(PipeWorkspaceSmoke EXCLUDE_FROM_ALL
    Source/EmbeddedScAudioEngine.cpp
    Source/EmbeddedScAudioEngine.h
    Source/MusicalObjectEditorComponent.cpp
    Source/MusicalObjectEditorComponent.h
    Source/PipeWorkspaceComponent.cpp
    Source/PipeWorkspaceComponent.h
    Source/PipeWorkspaceSmoke.cpp)
target_compile_definitions(PipeWorkspaceSmoke PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    GRIDCOLLIDER_ALCHEMY_SC_ROOT="${BLENDINGS_SC_ROOT}")
if (EXISTS "${SUPERCOLLIDER_INCLUDE_ROOT}/server/SC_WorldOptions.h")
    target_include_directories(PipeWorkspaceSmoke SYSTEM PRIVATE
        "${SUPERCOLLIDER_INCLUDE_ROOT}/common"
        "${SUPERCOLLIDER_INCLUDE_ROOT}/plugin_interface"
        "${SUPERCOLLIDER_INCLUDE_ROOT}/server")
    target_compile_definitions(PipeWorkspaceSmoke PRIVATE GRIDCOLLIDER_HAS_SC_HEADERS=1)
endif()
target_link_libraries(PipeWorkspaceSmoke PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

find_program(OTHERWARE_NODE node
    HINTS "$ENV{HOME}/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin")

if(OTHERWARE_NODE)
    add_custom_target(PdEditorRoundTrip
        COMMAND "${OTHERWARE_NODE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/pd_editor_roundtrip_test.js"
            "${CMAKE_CURRENT_SOURCE_DIR}/Source/PdPatchEditor.html"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running Pd editor round-trip checks")
endif()
