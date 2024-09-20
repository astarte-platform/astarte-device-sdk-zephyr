# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

function(concat_if_exists FILE_PATH STRING_NAME)
    # Check if the file exists
    get_filename_component(FILE_TO_ADD ${FILE_PATH} ABSOLUTE)
    
    # If the file exists, add it to the list
    if(EXISTS ${FILE_TO_ADD})
        set(${STRING_NAME} "${${STRING_NAME}};${FILE_PATH}" PARENT_SCOPE)
    endif()
endfunction()
