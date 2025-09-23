set(KDDW_3DPARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/3rdparty")

function(kddw_link_to_kdbindings target)
    if(TARGET KDAB::KDBindings)
        target_link_libraries(${target} PRIVATE KDAB::KDBindings)
    else()
        target_include_directories(${target} SYSTEM PRIVATE ${KDDW_3DPARTY_DIR})
    endif()
endfunction()
