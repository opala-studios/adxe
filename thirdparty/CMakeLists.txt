
project(thirdparty)

if(NOT APPLE)
    set(OPT_ALSOFT_DEFAULT ON)
else()
    set(OPT_ALSOFT_DEFAULT OFF) 
endif()

option(OPT_BOX2D_OPTIMIZED "" OFF)
option(BUILD_DEP_BOX2D "Build with internal Box2D support" ON)
option(BUILD_DEP_CHIPMUNK "Build with internal chipmunk support" ON)
option(BUILD_DEP_FREETYPE "Build with internal freetype support" ON)
option(BUILD_DEP_RECAST "Build with internal recast support" ON)
option(BUILD_DEP_BULLET "Build with internal bullet support" ON)
option(BUILD_DEP_JPEG "Build with internal jpeg support" ON)
option(BUILD_DEP_OPENSSL "Build with internal openssl support" ON)
option(BUILD_DEP_WEBP "Build with internal webp support" ON)
option(BUILD_DEP_PUGIXML "Build with internal pugixml support" ON)
option(BUILD_DEP_CLIPPER "Build with internal clipper support" ON)
option(BUILD_DEP_CONVERTUTF "Build with internal ConvertUTF support" ON)
option(BUILD_DEP_POLY2TRI "Build with internal poly2tri support" ON)
option(BUILD_DEP_ZLIB "Build with internal zlib support" ON)
option(BUILD_DEP_CURL "Build with internal curl support" ON)
option(BUILD_DEP_UNZIP "Build with internal unzip support" ON)
option(BUILD_DEP_ASTC "Build with internal ASTC support" ON)
option(BUILD_DEP_ALSOFT "Build with internal openal-soft support" ${OPT_ALSOFT_DEFAULT})
option(BUILD_DEP_CARES "Build with internal c-ares support" OFF)
option(BUILD_DEP_LLHTTP "Build with lhttp support" ON)
option(BUILD_DEP_YAML_CPP "Build with yaml-cpp support" OFF)
option(BUILD_DEP_KCP "Build with internal kcp support" OFF)

# by default, enable ios,macOS openal-soft framework for legal license LGPL-2.1
option(ALSOFT_OSX_FRAMEWORK "" ON)

set(ANDROID_SHARED_LOADS "" CACHE INTERNAL "The android shared libraries load source code" )
set(ANDROID_SHARED_LOAD_FILE_NAME "SharedLoader.java" CACHE INTERNAL "The android shared load java file name" )
set(ANDROID_SHARED_LOAD_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${ANDROID_SHARED_LOAD_FILE_NAME}.in" CACHE INTERNAL "The android shared libraries load config code file" )

# cmake_policy(SET CMP0079 NEW)

function(configure_target_outdir target)
    set_target_properties(${target} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "ThirdParty"
    )
endfunction()

if(NOT DEFINED LUA_ENGINE)
    set(LUA_ENGINE "plainlua" CACHE INTERNAL "The lua core lib, luajit or plainlua")
endif()

if(LUA_ENGINE STREQUAL "plainlua")
    if(NOT DEFINED LUA_VERSION)
        set(LUA_VERSION "5.3.6" CACHE INTERNAL "The plainlua version")
    endif()
    message(STATUS "Using plainlua: ${LUA_VERSION}")
elseif(LUA_ENGINE STREQUAL "luajit")
    message(STATUS "Using luajit")
else()
    message(FATAL_ERROR "Invalid lua engine: ${LUA_ENGINE}, must be plainlua or luajit")
endif()

add_library(thirdparty STATIC empty.cpp)

if(MACOSX OR ANDROID OR WINDOWS)
    add_subdirectory(zlib)
    target_link_libraries(thirdparty 
        ext_zlib
    )
endif()

if(BUILD_DEP_BOX2D)
    set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "Build the Box2D unit tests" FORCE)
    set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "Build the Box2D testbed" FORCE)
    if(OPT_BOX2D_OPTIMIZED)
        add_subdirectory(box2d-optimized)
        configure_target_outdir(box2d-optimized)
        target_link_libraries(thirdparty box2d-optimized)
    else()
        add_subdirectory(box2d)
        configure_target_outdir(box2d)
        target_link_libraries(thirdparty box2d)
    endif()
endif(BUILD_DEP_BOX2D)

if(BUILD_DEP_CHIPMUNK)
    set(CP_BUILD_SHARED OFF CACHE BOOL "Build chipmunk as shared library" FORCE)
    set(CP_BUILD_STATIC ON CACHE BOOL "Build chipmunk as static library" FORCE)
    set(CP_BUILD_DEMOS OFF CACHE BOOL "Build chipmunk demos" FORCE)
    set(CP_INSTALL_STATIC OFF CACHE BOOL "Install chipmunk static" FORCE)
    add_subdirectory(chipmunk)
    set_target_properties(chipmunk PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/chipmunk/include"
    )
    
    # !important adxe not use double precision
    target_compile_definitions(chipmunk PUBLIC CP_USE_CGTYPES=0)
    target_compile_definitions(chipmunk PUBLIC CP_USE_DOUBLES=0)

    #~ include_directories("${CMAKE_CURRENT_SOURCE_DIR}/chipmunk/include")
    target_link_libraries(thirdparty chipmunk)
    configure_target_outdir(chipmunk)
endif(BUILD_DEP_CHIPMUNK)
if(BUILD_DEP_FREETYPE)
    set(FT_WITH_ZLIB ON CACHE BOOL "Use system zlib instead of internal library." FORCE)
    set(DISABLE_FORCE_DEBUG_POSTFIX ON CACHE BOOL "" FORCE)
    add_subdirectory(freetype)
    target_include_directories(thirdparty PUBLIC "freetype/include")
    target_link_libraries(thirdparty freetype)
    configure_target_outdir(freetype)
endif(BUILD_DEP_FREETYPE)
if(BUILD_DEP_RECAST)
    add_subdirectory(recast)
    target_link_libraries(thirdparty recast)
    configure_target_outdir(recast)
endif(BUILD_DEP_RECAST)
if(BUILD_DEP_BULLET)
    add_subdirectory(bullet)
    target_link_libraries(thirdparty bullet)
    configure_target_outdir(bullet)
endif(BUILD_DEP_BULLET)
if(BUILD_DEP_JPEG)
    # set(WITH_SIMD ${CMAKE_ASM_NASM_COMPILER_LOADED} CACHE BOOL "Build TurboJPEG with simd speed up" FORCE)
    # set(WITH_JPEG8 TRUE CACHE BOOL "Build libjpeg-turbo with JEPG8 API" FORCE)
    # set(ENABLE_SHARED FALSE CACHE BOOL "Build shared libraries" FORCE)
    # set(WITH_TURBOJPEG FALSE CACHE BOOL "Include the TurboJPEG API library and associated test programs" FORCE)
    # set(JTURBO_ENABLE_INSTALL FALSE CACHE BOOL "Enable libjpeg-turbo install" FORCE)
    # set(JTURBO_BUILD_TESTS FALSE CACHE BOOL "Build TurboJPEG tests and examples" FORCE)
    # set(JTURBO_BUILD_BINARIES FALSE CACHE BOOL "Build TurboJPEG binaries" FORCE)
    # set(WITH_CRT_DLL TRUE CACHE BOOL "Build TurboJPEG with crt dll" FORCE)
    add_subdirectory(jpeg)
    # configure_target_outdir(jpeg)
    # configure_target_outdir(simd)
    # if(TARGET simd-objs)
    #     configure_target_outdir(simd-objs)
    # endif()
    # if(TARGET jsimdcfg)
    #     configure_target_outdir(jsimdcfg)
    # endif()
    target_link_libraries(thirdparty ext_jpeg)
    # target_include_directories(thirdparty INTERFACE 
    #   "${CMAKE_CURRENT_LIST_DIR}/jpeg" 
    #   "${JPEG_CONFIG_OUT_DIR}")
endif(BUILD_DEP_JPEG)
if(BUILD_DEP_OPENSSL)
    add_subdirectory(openssl)
    if(ANDROID OR LINUX)
        target_link_libraries(OpenSSL::SSL INTERFACE OpenSSL::Crypto)
        target_link_libraries(thirdparty OpenSSL::SSL)
    else()
        target_link_libraries(thirdparty OpenSSL::Crypto OpenSSL::SSL)
    endif()
endif(BUILD_DEP_OPENSSL)
if(BUILD_DEP_WEBP)
    add_subdirectory(webp)
    target_link_libraries(thirdparty webp)
    configure_target_outdir(webp)
endif(BUILD_DEP_WEBP)
if(BUILD_DEP_PUGIXML)
    add_subdirectory(pugixml)
    target_link_libraries(thirdparty pugixml)
    configure_target_outdir(pugixml)
endif(BUILD_DEP_PUGIXML)

add_subdirectory(xxhash)
target_link_libraries(thirdparty xxhash)
configure_target_outdir(xxhash)

add_subdirectory(lz4)
target_link_libraries(thirdparty lz4)
configure_target_outdir(lz4)

if(BUILD_DEP_CLIPPER)
    add_subdirectory(clipper)
    target_link_libraries(thirdparty clipper)
    configure_target_outdir(clipper)
endif(BUILD_DEP_CLIPPER)
if(BUILD_DEP_CONVERTUTF)
    add_subdirectory(ConvertUTF)
    target_link_libraries(thirdparty ConvertUTF)
    configure_target_outdir(ConvertUTF)
endif(BUILD_DEP_CONVERTUTF)
if(BUILD_DEP_POLY2TRI)
    add_subdirectory(poly2tri)
    target_link_libraries(thirdparty poly2tri)
    configure_target_outdir(poly2tri)
endif(BUILD_DEP_POLY2TRI)

if(BUILD_DEP_ASTC)
    add_subdirectory(astc)
    target_link_libraries(thirdparty astc)
    configure_target_outdir(astc)
endif(BUILD_DEP_ASTC)

# use lua/js specific libs by property to prevent conflict
if(BUILD_LUA_LIBS)
    add_subdirectory(lua/${LUA_ENGINE})
    add_subdirectory(lua/tolua)
    add_subdirectory(lua/lua-cjson)
    set_property(TARGET thirdparty APPEND PROPERTY 
        CC_LUA_DEPEND ${LUA_ENGINE} tolua lua-cjson
    )
endif()

if(NOT LINUX)
    if(BUILD_DEP_CURL)
        add_subdirectory(curl)
        if(ANDROID OR LINUX)
            target_link_libraries(libcurl INTERFACE OpenSSL::SSL)
        endif()
        target_link_libraries(thirdparty libcurl)
    endif(BUILD_DEP_CURL)
endif(NOT LINUX)

if(ANDROID)
    add_subdirectory(android-specific/cpufeatures)
    # target_link_libraries(thirdparty 
    #     cpufeatures 
    # )
endif()

# The openal-soft(LGPL 2.1)
if(BUILD_DEP_ALSOFT)
    set(ALSOFT_DLOPEN OFF CACHE BOOL "Check for the dlopen API for loading optional libs" FORCE)
    set(ALSOFT_UTILS OFF CACHE BOOL "Build utility program" FORCE)
    set(ALSOFT_EXAMPLES OFF CACHE BOOL "Build example programs" FORCE)
    set(ALSOFT_INSTALL OFF CACHE BOOL "Install main library" FORCE)
    add_subdirectory(openal EXCLUDE_FROM_ALL)
    configure_target_outdir(common)
    configure_target_outdir(OpenAL)
    target_link_libraries(thirdparty
        OpenAL
    )
    target_compile_definitions(thirdparty PUBLIC CC_USE_ALSOFT=1)
    
    if(ANDROID)
        set(ANDROID_SHARED_LOADS "${ANDROID_SHARED_LOADS}System.loadLibrary(\"openal\");" CACHE INTERNAL "Android Shared Loads" )
    endif()
endif()

# The ogg decoder
add_subdirectory(ogg)
target_link_libraries(thirdparty ogg)
configure_target_outdir(ogg)

if(WINDOWS OR LINUX)
    add_subdirectory(glad)
    target_link_libraries(thirdparty glad)
    configure_target_outdir(glad)
endif()

if(WINDOWS OR MACOSX OR LINUX)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "Build the GLFW documentation" FORCE)
    add_subdirectory(glfw)
    configure_target_outdir(glfw)
    set_target_properties(glfw PROPERTIES
                      OUTPUT_NAME glfw)
    target_link_libraries(thirdparty glfw)
    if(NOT WINDOWS)
        target_link_libraries(thirdparty pthread)
    endif()
    target_include_directories(thirdparty PUBLIC "glfw/include/GLFW")
endif()

if(NOT LINUX)
    add_subdirectory(png)
    target_include_directories(png PRIVATE "zlib/include")
    if(ANDROID)
        target_include_directories(png PRIVATE ${cpufeatures_include_dir})
        target_link_libraries(png INTERFACE cpufeatures)
    endif()
    target_link_libraries(thirdparty png)
    configure_target_outdir(png)
endif()

# unzip depend on zlib
if(BUILD_DEP_UNZIP)
    add_subdirectory(unzip)
    target_link_libraries(thirdparty unzip)
    configure_target_outdir(unzip)
endif(BUILD_DEP_UNZIP)

if(BUILD_DEP_CARES)
    set(CARES_STATIC ON CACHE BOOL "Build c-ares as static library" FORCE)
    set(CARES_SHARED OFF CACHE BOOL "Build c-ares as shared library" FORCE)
    set(CARES_BUILD_TOOLS OFF CACHE BOOL "Build c-ares tools" FORCE)
    set(CARES_INSTALL OFF CACHE BOOL "Create installation targets (chain builders may want to disable this)" FORCE)
    add_subdirectory(c-ares)
    target_link_libraries(thirdparty c-ares)
    include_directories($<TARGET_FILE_DIR:c-ares>)
    include_directories("c-ares/include")
    configure_target_outdir(c-ares)
    target_compile_definitions(thirdparty PUBLIC YASIO_HAVE_CARES=1)
endif(BUILD_DEP_CARES)

if(BUILD_DEP_LLHTTP)
    add_subdirectory(llhttp)
    configure_target_outdir(llhttp)
    target_link_libraries(thirdparty llhttp)
endif()

if(BUILD_DEP_YAML_CPP)
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_CLANG_FORMAT_EXE OFF CACHE BOOL "" FORCE)
    # set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    add_subdirectory(yaml-cpp)
    configure_target_outdir(yaml-cpp)
    # target_link_libraries(thirdparty yaml-cpp)
endif()

if(BUILD_DEP_KCP)
    add_subdirectory(kcp)
    configure_target_outdir(kcp)
    target_link_libraries(thirdparty kcp)
    target_compile_definitions(thirdparty PUBLIC YASIO_HAVE_KCP=1)
endif()

add_subdirectory(fmt)
target_link_libraries(thirdparty fmt-header-only)
configure_target_outdir(fmt)

# put "thirdparty" into thirdparty folder, too
configure_target_outdir(thirdparty)

if(MACOSX OR IOS)
    add_subdirectory(glsl-optimizer)
    target_link_libraries(thirdparty 
    ext_glcpp_library
    ext_glsl_optimizer
    ext_libmesa
    )
endif()

if (ANDROID)
    function(config_android_shared_libs package_name target_folder)
        string(REPLACE "." "/" package_path ${package_name})
        set(ANDROID_PACKAGE_NAME ${package_name})
        configure_file(${ANDROID_SHARED_LOAD_FILE_PATH} ${target_folder}/${package_path}/${ANDROID_SHARED_LOAD_FILE_NAME} @ONLY)
    endfunction()
endif()
