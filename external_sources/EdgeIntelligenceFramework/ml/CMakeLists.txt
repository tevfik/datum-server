add_library(eif_ml STATIC
    src/eif_ml_trees.c
    src/eif_ml_cluster.c
    src/eif_ml_linear.c
    src/eif_ml_bayes.c
    src/eif_ml_svm.c
    src/eif_ml_knn.c
    src/eif_ml_rf.c
    src/eif_ml_pca.c
    src/eif_ml_kmeans.c
    src/eif_ml_kmeans_fixed.c
    src/eif_ml_knn_fixed.c
    src/eif_ml_svm_fixed.c
    src/eif_ml_rf_fixed.c
    src/eif_ml_nb_fixed.c
    src/eif_ml_logistic_fixed.c
    src/eif_ml_pca_fixed.c
    src/eif_ml_gradient_boost_fixed.c
    src/eif_time_series_fixed.c
    src/eif_adaptive_threshold_fixed.c
)

target_include_directories(eif_ml PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../da/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../hal/include
)

target_link_libraries(eif_ml PUBLIC
    eif_core
    eif_da
    m
)
