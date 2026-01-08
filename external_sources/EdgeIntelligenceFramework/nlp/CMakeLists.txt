add_library(eif_nlp STATIC
    src/eif_nlp_tokenizer.c
    src/eif_nlp_phoneme.c
    src/eif_transformer.c
    src/eif_nlp_embedding.c
    src/eif_nlp_embedding_fixed.c
)

target_include_directories(eif_nlp PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../hal/include
)

target_link_libraries(eif_nlp PUBLIC
    eif_core
    m
)
