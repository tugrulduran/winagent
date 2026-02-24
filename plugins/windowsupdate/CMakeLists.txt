set(PLUGIN_NAME "windowsupdate")
set(tgt windowsupdate)

add_library(${tgt} SHARED ${PROJECT_SOURCE_DIR}/src/BasePlugin.cpp Plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/WindowsUpdateSampler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/WindowsUpdateUi.cpp
)

target_compile_definitions(${tgt} PRIVATE WA_BUILDING_PLUGIN)
target_include_directories(${tgt} PRIVATE ${PROJECT_SOURCE_DIR}/include src)

# Windows Update Agent (WUA) COM API
# Link against Wuguid.lib (Windows SDK). wuapi.h provides the COM interfaces.
target_link_libraries(${tgt} PRIVATE Qt6::Core Qt6::Widgets Wuguid ole32 oleaut32)

if (TARGET WinAgent)
    set_target_properties(${tgt} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:WinAgent>/plugins/${PLUGIN_NAME}"
            LIBRARY_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:WinAgent>/plugins/${PLUGIN_NAME}"
    )
    add_custom_command(TARGET ${tgt} POST_BUILD
            # Runtime convention: <exe_dir>/plugins/<id>/config.json
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:WinAgent>/plugins/${PLUGIN_NAME}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
            "$<TARGET_FILE_DIR:WinAgent>/plugins/${PLUGIN_NAME}/config.json"
    )
endif()

wa_register_plugin_target(${tgt})

install(TARGETS ${tgt}
        RUNTIME DESTINATION bin/plugins
        LIBRARY DESTINATION bin/plugins
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
        DESTINATION "bin/plugins/${PLUGIN_NAME}"
        RENAME "config.json"
)
