set(TARGET_NAME audio_pipeline_demo)

add_executable(${TARGET_NAME} main.c)

target_link_libraries(${TARGET_NAME} 
    PRIVATE 
    eif_dsp
    eif_core
    m
)

install(TARGETS ${TARGET_NAME} DESTINATION bin)
