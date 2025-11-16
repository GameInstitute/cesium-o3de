#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

o3de_read_json_key(GEM_TYPE ${CMAKE_CURRENT_SOURCE_DIR}/gem.json "type")
if (GEM_TYPE STREQUAL "Asset")
    if(LY_MONOLITHIC_GAME)
        include(Platform/${PAL_PLATFORM_NAME}/platform_${PAL_PLATFORM_NAME_LOWERCASE}.cmake OPTIONAL)                    
    else()
        include(Platform/${PAL_PLATFORM_NAME}/platform_${PAL_PLATFORM_NAME_LOWERCASE}.cmake)
    endif()
else()
    add_subdirectory(Code)
endif()
