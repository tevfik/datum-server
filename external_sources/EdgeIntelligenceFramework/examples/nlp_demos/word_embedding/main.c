#include <stdio.h>
#include <string.h>
#include "eif_nlp_embedding.h"

int main(void) {
    printf("=== EIF NLP Word Embedding Demo ===\n");
    
    eif_embedding_t emb;
    // Init with capacity 10 words, dim 3
    if (eif_word_embedding_init(&emb, 10, 3) != EIF_STATUS_OK) {
        printf("Failed to init embedding.\n");
        return -1;
    }
    
    // Toy vectors (3D): [Royalty, Gender, Humanity]
    // King:  [0.9,  0.8, 1.0]
    // Queen: [0.9, -0.8, 1.0]
    // Man:   [0.0,  0.9, 1.0]
    // Woman: [0.0, -0.9, 1.0]
    // Apple: [0.0,  0.0, 0.1] (Low on all)
    
    float v_king[]  = {0.9f,  0.8f, 1.0f};
    float v_queen[] = {0.9f, -0.8f, 1.0f};
    float v_man[]   = {0.0f,  0.9f, 1.0f};
    float v_woman[] = {0.0f, -0.9f, 1.0f};
    float v_apple[] = {0.0f,  0.0f, 0.1f};
    
    eif_embedding_add(&emb, "king", v_king);
    eif_embedding_add(&emb, "queen", v_queen);
    eif_embedding_add(&emb, "man", v_man);
    eif_embedding_add(&emb, "woman", v_woman);
    eif_embedding_add(&emb, "apple", v_apple);
    
    printf("Vocabulary loaded: king, queen, man, woman, apple\n");
    
    // Check Similarities
    const char* w1 = "king";
    const char* w2 = "queen";
    float sim = eif_embedding_similarity(&emb, w1, w2);
    printf("Similarity(%s, %s) = %.4f\n", w1, w2, sim);
    
    w1 = "king"; w2 = "man";
    sim = eif_embedding_similarity(&emb, w1, w2);
    printf("Similarity(%s, %s) = %.4f\n", w1, w2, sim);
    
    w1 = "king"; w2 = "woman";
    sim = eif_embedding_similarity(&emb, w1, w2);
    printf("Similarity(%s, %s) = %.4f\n", w1, w2, sim);
    
    w1 = "king"; w2 = "apple";
    sim = eif_embedding_similarity(&emb, w1, w2);
    printf("Similarity(%s, %s) = %.4f\n", w1, w2, sim);
    
    // "King" - "Man" + "Woman" ?= "Queen"
    // Calc expected vector
    float v_calc[3];
    for(int i=0; i<3; i++) v_calc[i] = v_king[i] - v_man[i] + v_woman[i];
    
    // Check similarity of v_calc to "Queen"
    // v_calc should be:
    // [0.9-0+0, 0.8-0.9+(-0.9)= -1.0, 1.0-1.0+1.0=1.0] -> [0.9, -1.0, 1.0]
    // Queen is [0.9, -0.8, 1.0]. Very close!
    
    float sim_analogy = eif_vector_cosine_similarity(v_calc, v_queen, 3);
    printf("Analogy (King - Man + Woman) vs Queen Similarity: %.4f\n", sim_analogy);

    eif_word_embedding_free(&emb);
    return 0;
}
