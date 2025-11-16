#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

# Include platform-specific configuration
if(LY_MONOLITHIC_GAME)
    include(Platform/${PAL_PLATFORM_NAME}/platform_${PAL_PLATFORM_NAME_LOWERCASE}.cmake OPTIONAL)
else()
    include(Platform/${PAL_PLATFORM_NAME}/platform_${PAL_PLATFORM_NAME_LOWERCASE}.cmake)
endif()

# Include PAL traits for platform support flags
include(Platform/${PAL_PLATFORM_NAME}/PAL_${PAL_PLATFORM_NAME_LOWERCASE}.cmake)

# Add CesiumNative as a third party
set(LY_PACKAGE_SERVER_URLS "${LY_PACKAGE_SERVER_URLS};file:///${CMAKE_CURRENT_LIST_DIR}/../External/Packages/Install" FORCE)
set(SHA256SUMS_FILE "${CMAKE_CURRENT_LIST_DIR}/../External/Packages/Install/SHA256SUMS")
if(EXISTS ${SHA256SUMS_FILE})
    file(READ ${SHA256SUMS_FILE} CesiumNative_SHA256_PACKAGE)
    string(STRIP ${CesiumNative_SHA256_PACKAGE} CesiumNative_SHA256_PACKAGE)
    ly_associate_package(PACKAGE_NAME CesiumNative TARGETS CesiumNative PACKAGE_HASH ${CesiumNative_SHA256_PACKAGE})
else()
    # SHA256SUMS file doesn't exist yet - this means External package needs to be built first
    # For development builds, we'll use a placeholder hash that will be replaced when the package is built
    # Note: You need to build External/CMakeLists.txt first to generate the SHA256SUMS file
    message(STATUS "CesiumNative SHA256SUMS file not found at ${SHA256SUMS_FILE}.")
    message(STATUS "To generate it, build the External package first:")
    message(STATUS "  cd External")
    message(STATUS "  cmake -B Build -S .")
    message(STATUS "  cmake --build Build --config release --target install")
    message(STATUS "Using placeholder hash for now. This is safe for initial CMake configuration.")
    # Use a placeholder hash - this should be replaced after building External package
    set(CesiumNative_SHA256_PACKAGE "0000000000000000000000000000000000000000000000000000000000000000")
    ly_associate_package(PACKAGE_NAME CesiumNative TARGETS CesiumNative PACKAGE_HASH ${CesiumNative_SHA256_PACKAGE})
endif()

# Add the Cesium.Static target
# Note: We include the common files and the platform specific files which are set in cesium_files.cmake
# and in Platform/${PAL_PLATFORM_NAME}/cesium_${PAL_PLATFORM_NAME_LOWERCASE}_files.cmake
ly_add_target(
    NAME Cesium.Static STATIC
    NAMESPACE Gem
    FILES_CMAKE
        cesium_files.cmake
        Platform/${PAL_PLATFORM_NAME}/cesium_${PAL_PLATFORM_NAME_LOWERCASE}_files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC
            Include
        PRIVATE
            Source
    BUILD_DEPENDENCIES
        PUBLIC
            3rdParty::AWSNativeSDK::Core
            3rdParty::CesiumNative
            3rdParty::mikkelsen
            3rdParty::zlib
            AZ::AzCore
            AZ::AzFramework
            Legacy::CryCommon
            Gem::Atom_RPI.Public
            Gem::LyShine.Static
    COMPILE_DEFINITIONS
        PUBLIC
            SPDLOG_COMPILED_LIB
            LIBASYNC_STATIC
            TIDY_STATIC
)

# Here add Cesium target, it depends on the Cesium.Static
ly_add_target(
    NAME Cesium ${PAL_TRAIT_MONOLITHIC_DRIVEN_MODULE_TYPE}
    NAMESPACE Gem
    FILES_CMAKE
        cesium_shared_files.cmake
        Platform/${PAL_PLATFORM_NAME}/cesium_shared_${PAL_PLATFORM_NAME_LOWERCASE}_files.cmake
    INCLUDE_DIRECTORIES
        PRIVATE
            Source
        PUBLIC
            Include
    BUILD_DEPENDENCIES
        PRIVATE
            Gem::Cesium.Static
)

# By default, we will specify that the above target Cesium would be used by
# Client and Server type targets when this gem is enabled.  If you don't want it
# active in Clients or Servers by default, delete one of both of the following lines:
ly_create_alias(NAME Cesium.Clients NAMESPACE Gem TARGETS Gem::Cesium)
ly_create_alias(NAME Cesium.Servers NAMESPACE Gem TARGETS Gem::Cesium)

# If we are on a host platform, we want to add the host tools targets like the Cesium.Editor target which
# will also depend on Cesium.Static
if(PAL_TRAIT_BUILD_HOST_TOOLS)
    ly_add_target(
        NAME Cesium.Editor.Static STATIC
        NAMESPACE Gem
        AUTOMOC
        AUTORCC
        FILES_CMAKE
            cesium_editor_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Source
            PUBLIC
                Include
        BUILD_DEPENDENCIES
            PUBLIC
                AZ::AzToolsFramework
                Gem::AtomToolsFramework.Static
                Gem::Cesium.Static
    )

    ly_add_target(
        NAME Cesium.Editor GEM_MODULE
        NAMESPACE Gem
        AUTOMOC
        AUTORCC
        OUTPUT_NAME Gem.Cesium.Editor
        FILES_CMAKE
            cesium_editor_shared_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Source
            PUBLIC
                Include
        BUILD_DEPENDENCIES
            PUBLIC
                Gem::Cesium.Editor.Static
    )

    # By default, we will specify that the above target Cesium would be used by
    # Tool and Builder type targets when this gem is enabled.  If you don't want it
    # active in Tools or Builders by default, delete one of both of the following lines:
    ly_create_alias(NAME Cesium.Tools    NAMESPACE Gem TARGETS Gem::Cesium.Editor)
    ly_create_alias(NAME Cesium.Builders NAMESPACE Gem TARGETS Gem::Cesium.Editor)


endif()

################################################################################
# Tests
################################################################################
# See if globally, tests are supported
if(PAL_TRAIT_BUILD_TESTS_SUPPORTED)
    # We globally support tests, see if we support tests on this platform for Cesium.Static
    if(PAL_TRAIT_CESIUM_TEST_SUPPORTED)
        # We support Cesium.Tests on this platform, add Cesium.Tests target which depends on Cesium.Static
        ly_add_target(
            NAME Cesium.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
            NAMESPACE Gem
            FILES_CMAKE
                cesium_files.cmake
                cesium_tests_files.cmake
            INCLUDE_DIRECTORIES
                PRIVATE
                    Tests
                    Source
            BUILD_DEPENDENCIES
                PRIVATE
                    AZ::AzTest
                    AZ::AzFramework
                    Gem::Cesium.Static
        )

        # Add Cesium.Tests to googletest
        ly_add_googletest(
            NAME Gem::Cesium.Tests
        )
    endif()

    # If we are a host platform we want to add tools test like editor tests here
    if(PAL_TRAIT_BUILD_HOST_TOOLS)
        # We are a host platform, see if Editor tests are supported on this platform
        if(PAL_TRAIT_CESIUM_EDITOR_TEST_SUPPORTED)
            # We support Cesium.Editor.Tests on this platform, add Cesium.Editor.Tests target which depends on Cesium.Editor
            ly_add_target(
                NAME Cesium.Editor.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
                NAMESPACE Gem
                FILES_CMAKE
                    cesium_editor_tests_files.cmake
                INCLUDE_DIRECTORIES
                    PRIVATE
                        Tests
                        Source
                BUILD_DEPENDENCIES
                    PRIVATE
                        AZ::AzTest
                        Gem::Cesium.Editor
            )

            # Add Cesium.Editor.Tests to googletest
            ly_add_googletest(
                NAME Gem::Cesium.Editor.Tests
            )
        endif()
    endif()
endif()
