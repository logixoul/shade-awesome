cmake_minimum_required(VERSION 3.28)

project(FluidSims LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Use static MSVC runtime (/MT or /MTd) to match the vcpkg triplet
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# ── Packages ──────────────────────────────────────────────────────────────────
find_package(OpenGL REQUIRED)
find_package(glfw3  CONFIG REQUIRED)
find_package(glm    CONFIG REQUIRED)
# Workaround for vcpkg's kissfft config calling cmake_minimum_required(VERSION 3.3)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
find_package(kissfft CONFIG REQUIRED COMPONENTS SHARED float)

#find_path(VKFFT_INCLUDE_DIRS "VkFFT/vkFFT.h")

# ── Source lists ──────────────────────────────────────────────────────────────
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/third-party/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)

set(THIRD_PARTY_SOURCES
    src/third-party/src/glad.c
    src/third-party/src/stb_image.cpp
)

set(CXX_MODULE_SOURCES
    "src/lxlib/Rect.ixx"
    src/lxlib/Array2D_imageProc.ixx
    src/lxlib/colorspaces.ixx
    src/lxlib/ConfigManager3.ixx
    src/lxlib/GlslProg.ixx
    src/lxlib/gpgpu.ixx
    "src/lxlib/gpuBlur.ixx"
    src/lxlib/IntegratedConsole.ixx
    src/lxlib/shade.ixx
    src/lxlib/SketchBase.ixx
    src/lxlib/SketchScaffold.ixx
    src/lxlib/stuff.ixx
    src/lxlib/TextureCache.ixx
    src/lxlib/TextureRef.ixx
    src/lxlib/Array2D.ixx
    src/lxlib/VaoVbo.ixx
    src/lxlib/KissFFTWrapper.ixx
    src/MultiscaleGrowthSketch/gpuBlurClaude.ixx
    src/MultiscaleGrowthSketch/ThisSketch_ImageProcessingHelpers.ixx
    src/MultiscaleGrowthSketch/MultiscaleGrowthSketch.ixx
    src/ParticleFluidSketch.ixx
    src/GridFluidSketch/GridFluidSketch.ixx
    src/ParticleTraces2DSketch/ParticleTraces2DSketch.ixx
    src/VarianceAddSketch/VarianceAddSketch.ixx
    src/FftRaysSketch/FftRaysSketch.ixx
)

# ── Target ────────────────────────────────────────────────────────────────────
add_executable(App
    src/main.cpp
    src/lxlib/simplexnoise.cpp
    ${IMGUI_SOURCES}
    ${THIRD_PARTY_SOURCES}
)

if(WIN32)
    set_target_properties(App PROPERTIES WIN32_EXECUTABLE ON)
endif()

# C++20 named module units
target_sources(App
    PRIVATE FILE_SET CXX_MODULES FILES
        ${CXX_MODULE_SOURCES}
)

target_include_directories(App PRIVATE
    src
    ${IMGUI_DIR}
#    ${VKFFT_INCLUDE_DIRS}
    src/third-party/include
)

# Precompiled headers interact badly with GCC's current named-modules flow,
# so keep them on MSVC where this setup is known to work.
if(MSVC)
    target_precompile_headers(App PRIVATE src/precompiled.h)
endif()

# Third-party sources don't use our PCH.
set_source_files_properties(
    ${IMGUI_SOURCES}
    src/third-party/src/glad.c
    src/third-party/src/stb_image.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

target_compile_definitions(App PRIVATE
    $<$<PLATFORM_ID:Windows>:WIN32>
    $<$<PLATFORM_ID:Windows>:_WINDOWS>
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:NDEBUG>
)

if(MSVC)
    target_compile_options(App PRIVATE
        /W0
        /Zm200
        $<$<CONFIG:Release>:/O2 /fp:fast /arch:SSE2 /openmp /GS- /Zi>
        $<$<CONFIG:RelWithDebInfo>:/O2 /fp:fast /arch:SSE2 /openmp /GS- /Zi>
        $<$<CONFIG:Debug>:/RTC1 /Od>
    )

    target_link_options(App PRIVATE
        /DEBUG
    )
endif()

target_link_libraries(App PRIVATE
    OpenGL::GL
    glfw
    glm::glm
    kissfft::kissfft
)


# ── Runtime config copies: always checked, only copied when source is newer ───
add_custom_target(AppRuntimeConfigs
    COMMAND ${CMAKE_COMMAND} -E copy_if_newer
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GridFluidSketch/gridFluidConfig.toml
        $<TARGET_FILE_DIR:App>/gridFluidConfig.toml
    COMMAND ${CMAKE_COMMAND} -E copy_if_newer
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MultiscaleGrowthSketch/multiscaleGrowthConfig.toml
        $<TARGET_FILE_DIR:App>/multiscaleGrowthConfig.toml
    COMMAND ${CMAKE_COMMAND} -E copy_if_newer
        ${CMAKE_CURRENT_SOURCE_DIR}/src/FftRaysSketch/FftRaysConfig.toml
        $<TARGET_FILE_DIR:App>/FftRaysConfig.toml
)
add_dependencies(App AppRuntimeConfigs)
