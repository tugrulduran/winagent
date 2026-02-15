# plugins/CMakeLists.txt
# Auto-discover plugin subdirectories and provide aggregated "plugins" target.

function(wa_register_plugin_target target_name)
    set_property(GLOBAL APPEND PROPERTY WA_PLUGIN_TARGETS ${target_name})
endfunction()

file(GLOB children CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*")

foreach(child ${children})
    if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${child}")
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/CMakeLists.txt")
            add_subdirectory(${child})
        endif()
    endif()
endforeach()

get_property(_pls GLOBAL PROPERTY WA_PLUGIN_TARGETS)
if (NOT _pls)
    set(_pls "")
endif()

add_custom_target(plugins)
if (_pls)
    add_dependencies(plugins ${_pls})
endif()
