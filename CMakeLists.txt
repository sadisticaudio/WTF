cmake_minimum_required(VERSION 3.14)
project(WTF VERSION 1.0.0)

IF (WIN32)
    set(AAX_COPY_LOCATION "C:/Program Files/Common Files/Avid/Audio/Plug-Ins")
    set(JUCE_LOCATION "C:/Users/frye/Documents/JUCE")
    set(VST_COPY_LOCATION "C:/Program Files (x86)/Common Files/VST")
    set(VST3_COPY_LOCATION "C:/Program Files/Common Files/VST3")
    set(VST2_SDK_LOCATION "C:/Users/frye/CODE/SDKs/VST3_SDK/VST2_SDK")
    set(AAX_SDK_LOCATION "C:/Users/frye/CODE/SDKs/AAX_SDK")
    set(AUDIO_PROJECTS_LOCATION "C:/Users/frye/Music/Projects")
ELSE()
    set(AAX_COPY_LOCATION "/Library/Application Support/Avid/Audio/Plug-Ins")
    set(APPLE_CODE_SIGN_IDENTITY "Developer ID Application: Benjamin Kelley (NXW2SG7JJ4)")
    set(APPLE_ID "NXW2SG7JJ4")
    set(JUCE_LOCATION "$ENV{HOME}/Documents/JUCE")
    set(VST_COPY_LOCATION "/Library/Audio/Plug-Ins/VST")
    set(VST3_COPY_LOCATION "/Library/Audio/Plug-Ins/VST3")
    set(VST2_SDK_LOCATION "$ENV{HOME}/CODE/SDKs/VST3_SDK/VST2_SDK")
    set(AAX_SDK_LOCATION "$ENV{HOME}/CODE/SDKs/AAX_SDK")
    set(AUDIO_PROJECTS_LOCATION "$ENV{HOME}/Music/Projects/Reaper")
    set(AU_XCODE_DEBUG_APP "/Applications/Logic Pro X.app")
    set(VST_XCODE_DEBUG_APP "/Applications/Ableton Live 10 Suite.app")
    set(VST3_XCODE_DEBUG_APP "/Applications/REAPER64.app")
    set(DEBUG_ARGS "${AUDIO_PROJECTS_LOCATION}/WTFTestRadiohead/WTFTestRadiohead.RPP")
ENDIF()

# By default we don't want schemes to be made for modules, etc
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_XCODE_ATTRIBUTE_OTHER_LDFLAGS "-Xlinker -headerpad -Xlinker 578")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake.modules/")

# Adds all the module sources so they appear correctly in the IDE
# This needs to be set before JUCE is added as a sub-dir
set_property(GLOBAL PROPERTY USE_FOLDERS YES)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "Targets")

# Create a /Modules directory with all the JUCE Module code
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Show all module sources in IDE projects" ON)

#file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Info.plist" plist_content_to_merge)

add_subdirectory(${JUCE_LOCATION} JUCE)
juce_set_vst2_sdk_path(${VST2_SDK_LOCATION})
juce_set_aax_sdk_path(${AAX_SDK_LOCATION})
juce_add_plugin(WTF
    BUNDLE_ID "com.sadisticaudio.Wtf"
    PLUGIN_MANUFACTURER_CODE sTic
    PLUGIN_CODE wtfK
    COMPANY_NAME "Sadistic Audio"
    PRODUCT_NAME "WTF"
    COMPANY_COPYRIGHT "copyright Sadistic Algorithms, LLC, 2020"
    COMPANY_EMAIL "support@sadisticaudio.com"
    DOCUMENT_EXTENSIONS xml wav
    FORMATS AU VST VST3 AAX
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_COPY_DIR "/Library/Audio/Plug-Ins/Components"
    AAX_COPY_DIR "${AAX_COPY_LOCATION}"
    VST_COPY_DIR "${VST_COPY_LOCATION}"
    VST3_COPY_DIR "${VST3_COPY_LOCATION}"
    HARDENED_RUNTIME_ENABLED TRUE
    IS_SYNTH FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    DESCRIPTION "WTF Dynamic Waveshaper"
    VST2_CATEGORY kPlugCategEffect
    VST3_CATEGORIES Fx Analyzer EQ Filter
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY AAX_ePlugInCategory_EQ)

file(GLOB_RECURSE resources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "Resources/*")
file(GLOB_RECURSE freeResources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "../Deviant/Resources/*")
file(GLOB_RECURSE bundle_resources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "Resources/WTF.xml")

file(GLOB SourceFiles
    Source/*.h
    Source/*.cpp
    ../Source/*.h
    ../Source/*.cpp
    ../Deviant/Source/*.h
    ../Deviant/Source/deviant.cpp)

target_sources(WTF PRIVATE ${SourceFiles})
source_group(TREE ../ PREFIX "" FILES ${SourceFiles})
# Move the INTERFACE auto-created JUCE library stuff into its own folder
source_group("JUCE Library Code" REGULAR_EXPRESSION "juce_")
# This cleans up the folder organization, especially on Xcode.
# It tucks the Plugin varieties into a "Targets" folder and generate an Xcode Scheme manually
# since Xcode scheme generation is turned off to limit noise
# Not all plugin types (au, vst) available on each build type (win, macos, linux)
foreach(target WTF_AAX WTF_AU WTF_VST WTF_VST3 WTF_All)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES 
            FOLDER "Targets"
            XCODE_GENERATE_SCHEME ON
            )
    endif()
endforeach()

#set(AAX_BUNDLE "/Library/Application Support/Avid/Audio/Plug-Ins/WTF.aaxplugin")

foreach(target WTF_AAX)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES 
            OSX_ARCHITECTURES "x86_64"
            XCODE_SCHEME_EXECUTABLE "/Applications/Pro Tools Developer.app")
    endif()
endforeach()

foreach(target WTF_AU WTF_VST WTF_VST3)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES 
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Developer ID Application: Benjamin Kelley (NXW2SG7JJ4)"
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "NXW2SG7JJ4"
            XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
            XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS --timestamp
            OSX_ARCHITECTURES "arm64;x86_64"
            XCODE_SCHEME_EXECUTABLE "/Applications/REAPER64.app"
            XCODE_SCHEME_ARGUMENTS "${AUDIO_PROJECTS_LOCATION}/WTFTestRadiohead/WTFTestRadiohead.RPP")
    endif()
endforeach()

juce_add_binary_data(BinaryData HEADER_NAME SadisticData.h NAMESPACE Data SOURCES ${resources} ${freeResources})

# juce_add_bundle_resources_directory(WTF_AAX Resources)

add_library(ALL_IPP INTERFACE)
IF (WIN32)
    set(IPP_ROOT "C:/Program Files (x86)/Intel/oneAPI/ipp/latest")
    set (IPP_LIB "${IPP_ROOT}/lib/intel64")
ELSE()
    set(IPP_ROOT "/opt/intel/oneapi/ipp/2021.1.1")
    set (IPP_LIB "${IPP_ROOT}/lib")
    add_custom_command(
        TARGET WTF_AAX POST_BUILD COMMAND 
        COMMAND rm -f ~/Library/Preferences/Avid/Pro\ Tools/InstalledAAXPlugIns)
ENDIF()

target_include_directories(ALL_IPP INTERFACE "${IPP_ROOT}/include")

add_library(ippi    STATIC IMPORTED GLOBAL)
add_library(ipps    STATIC IMPORTED GLOBAL)
add_library(ippvm   STATIC IMPORTED GLOBAL)
add_library(ippcore STATIC IMPORTED GLOBAL)

IF (WIN32)
	set_target_properties(ippi    PROPERTIES IMPORTED_LOCATION ${IPP_LIB}/ippimt.lib)
	set_target_properties(ipps    PROPERTIES IMPORTED_LOCATION ${IPP_LIB}/ippsmt.lib)
	set_target_properties(ippvm   PROPERTIES IMPORTED_LOCATION ${IPP_LIB}/ippvmmt.lib)
	set_target_properties(ippcore PROPERTIES IMPORTED_LOCATION ${IPP_LIB}/ippcoremt.lib)
ELSE()
	set_target_properties(ippi    PROPERTIES IMPORTED_LOCATION ${IPP_LIB}/libippi.a    OSX_ARCHITECTURES x86_64)
	set_target_properties(ipps    PROPERTIES IMPORTED_LOCATION ${IPP_LIB}/libipps.a    OSX_ARCHITECTURES x86_64)
	set_target_properties(ippvm   PROPERTIES IMPORTED_LOCATION ${IPP_LIB}/libippvm.a   OSX_ARCHITECTURES x86_64)
	set_target_properties(ippcore PROPERTIES IMPORTED_LOCATION ${IPP_LIB}/libippcore.a OSX_ARCHITECTURES x86_64)
ENDIF()

target_sources(WTF_AAX PRIVATE ${bundle_resources})
set_source_files_properties(${bundle_resources} PROPERTIES
  MACOSX_PACKAGE_LOCATION "Resources"
)
source_group("Resources" FILES ${bundle_resources})

#`target_compile_definitions` adds some preprocessor definitions to our target. In a Projucer
# project, these might be passed in the 'Preprocessor Definitions' field. JUCE modules also make use
# of compile definitions to switch certain features on/off, so if there's a particular feature you
# need that's not on by default, check the module header for the correctvisible code, so for new
# definitions, pick unique names that are unlikely to collide! This is a standard CMake command.
target_compile_features(WTF PRIVATE cxx_std_20)
target_compile_definitions(WTF PUBLIC
    JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
    JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
    JUCE_COPY_PLUGIN_AFTER_BUILD=TRUE
    JUCE_OPENGL3=1
    JUCE_USE_STUDIO_ONE_COMPATIBLE_PARAMETERS=1
    SADISTIC_PRO=1
    )

target_link_libraries(ALL_IPP INTERFACE ippi ipps ippvm ippcore)

target_link_libraries(WTF PRIVATE
    BinaryData
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    juce::juce_product_unlocking
    juce::juce_dsp
    juce::juce_opengl
    ALL_IPP
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
 )
