set(PLUGIN_NAME "audezemaxwell")
set(tgt WinAgentPlugin_audezemaxwell)

add_library(${tgt} SHARED ${PROJECT_SOURCE_DIR}/src/BasePlugin.cpp Plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudezeSampler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudezeMaxwellUi.cpp
)

target_compile_definitions(${tgt} PRIVATE WA_BUILDING_PLUGIN)
target_include_directories(${tgt} PRIVATE ${PROJECT_SOURCE_DIR}/include src)
target_link_directories(${tgt} PRIVATE ${PROJECT_SOURCE_DIR}/lib lib)
target_link_libraries(${tgt} PRIVATE Qt6::Core Qt6::Widgets hidapi)

if (TARGET WinAgent)
    set_target_properties(${tgt} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:WinAgent>/plugins"
            LIBRARY_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:WinAgent>/plugins"
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
