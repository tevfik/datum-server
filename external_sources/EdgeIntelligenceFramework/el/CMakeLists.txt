# Edge Learning Library
# Contains: Federated Learning, EWC, Online Learning, Few-Shot, RL/DQN

add_library(eif_el STATIC
    src/eif_el_federated.c
    src/eif_el_ewc.c
    src/eif_el_online.c
    src/eif_el_fewshot.c
    src/eif_el_utils.c
    src/eif_el_rl.c
    src/eif_el_dqn.c
)

add_library(EIF::el ALIAS eif_el)

target_include_directories(eif_el 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eif_el
    PUBLIC 
        eif_core
        eif_dl
        m
)

