#
#   Copyright 2013 Pixar
#
#   Licensed under the Apache License, Version 2.0 (the "Apache License")
#   with the following modification; you may not use this file except in
#   compliance with the Apache License and the following modification to it:
#   Section 6. Trademarks. is deleted and replaced with:
#
#   6. Trademarks. This License does not grant permission to use the trade
#      names, trademarks, service marks, or product names of the Licensor
#      and its affiliates, except as required to comply with Section 4(c) of
#      the License and to reproduce the content of the NOTICE file.
#
#   You may obtain a copy of the Apache License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the Apache License with the above modification is
#   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#   KIND, either express or implied. See the Apache License for the specific
#   language governing permissions and limitations under the Apache License.
#

set(TUTORIALS
    tutorial_0
    tutorial_1
    tutorial_2
    tutorial_3
    tutorial_4
    tutorial_5
    tutorial_6
    tutorial_7
)

foreach(tutorial ${TUTORIALS})

    add_subdirectory("${tutorial}")

    list(APPEND TUTORIAL_TARGETS "far_${tutorial}")

    add_test(far_${tutorial} ${EXECUTABLE_OUTPUT_PATH}/far_${tutorial})

endforeach()

add_custom_target(far_tutorials DEPENDS ${TUTORIAL_TARGETS})
