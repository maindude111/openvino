# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME conformanceTests)

addIeTargetTest(
        NAME ${TARGET_NAME}
        ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
        ADDITIONAL_SOURCE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        ADD_CPPLINT
        LINK_LIBRARIES
            PUBLIC
                conformanceShared
        LABELS
            OP_CONFORMANCE
)

ie_faster_build(${TARGET_NAME} UNITY)
