/**
 * @file main.c
 * @brief Phoneme-Based Text Processing Demo
 *
 * Demonstrates phoneme capabilities:
 * - Phoneme dictionary lookup
 * - Edit distance (Levenshtein)
 * - G2P conversion
 *
 * Usage:
 *   ./phoneme_cmd_demo --help
 *   ./phoneme_cmd_demo --batch
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/ascii_plot.h"
#include "../../common/demo_cli.h"
#include "eif_memory.h"
#include "eif_nlp_phoneme.h"

static bool json_mode = false;

// Get phoneme name
static const char *get_phoneme_name(eif_phoneme_t ph) {
  return eif_phoneme_to_str(ph);
}

// Print phoneme sequence
static void print_phoneme_seq(const eif_phoneme_seq_t *seq) {
  printf("[");
  for (int i = 0; i < seq->length; i++) {
    printf("%s", get_phoneme_name(seq->phonemes[i]));
    if (i < seq->length - 1)
      printf(" ");
  }
  printf("]");
}

// Demo: Phoneme basics
static void demo_phoneme_basics(void) {
  if (!json_mode) {
    ascii_section("1. CMU ARPABET Phoneme Set");
    printf("  Standard phoneme representation for speech\n\n");
  }

  if (!json_mode) {
    printf("  Vowels (15):\n");
    printf("    AA (odd), AE (at), AH (hut), AO (ought), AW (cow)\n");
    printf("    AY (hide), EH (Ed), ER (hurt), EY (ate), IH (it)\n");
    printf("    IY (eat), OW (oat), OY (toy), UH (hood), UW (two)\n\n");

    printf("  Consonants (24):\n");
    printf("    Stops:     B D G K P T\n");
    printf("    Affricates: CH JH\n");
    printf("    Fricatives: DH F S SH TH V Z ZH\n");
    printf("    Nasals:    M N NG\n");
    printf("    Liquids:   L R\n");
    printf("    Semivowels: W Y HH\n\n");

    printf("  Special: SIL (silence), UNK (unknown)\n");
  }
}

// Demo: Phoneme types
static void demo_phoneme_types(void) {
  if (!json_mode) {
    ascii_section("2. Phoneme Classification");
    printf("  Vowels vs Consonants\n\n");
  }

  eif_phoneme_t test_phonemes[] = {EIF_PH_AA, EIF_PH_B,  EIF_PH_IY,
                                   EIF_PH_S,  EIF_PH_UW, EIF_PH_T};
  const char *names[] = {"AA", "B", "IY", "S", "UW", "T"};
  int num_test = 6;

  if (!json_mode) {
    printf("  Phoneme  Vowel?  Consonant?\n");
    printf("  -------  ------  ----------\n");
    for (int i = 0; i < num_test; i++) {
      bool is_vowel = eif_phoneme_is_vowel(test_phonemes[i]);
      bool is_consonant = eif_phoneme_is_consonant(test_phonemes[i]);
      printf("  %-7s  %-6s  %s\n", names[i], is_vowel ? "YES" : "no",
             is_consonant ? "YES" : "no");
    }
  }
}

// Demo: Edit distance
static void demo_edit_distance(void) {
  if (!json_mode) {
    ascii_section("3. Phoneme Edit Distance");
    printf("  Measure pronunciation similarity\n\n");
  }

  // Create test sequences
  eif_phoneme_seq_t cat = {{EIF_PH_K, EIF_PH_AE, EIF_PH_T}, {0}, 3};
  eif_phoneme_seq_t bat = {{EIF_PH_B, EIF_PH_AE, EIF_PH_T}, {0}, 3};
  eif_phoneme_seq_t dog = {{EIF_PH_D, EIF_PH_AO, EIF_PH_G}, {0}, 3};

  struct {
    const char *name;
    eif_phoneme_seq_t *seq;
  } words[] = {{"CAT", &cat}, {"BAT", &bat}, {"DOG", &dog}};

  if (!json_mode) {
    printf("  Comparing words:\n");
    for (int i = 0; i < 3; i++) {
      printf("    %s = ", words[i].name);
      print_phoneme_seq(words[i].seq);
      printf("\n");
    }
    printf("\n  Edit distances:\n");

    int d_cat_bat = eif_phoneme_distance(&cat, &bat);
    int d_cat_dog = eif_phoneme_distance(&cat, &dog);
    int d_bat_dog = eif_phoneme_distance(&bat, &dog);

    printf("    CAT <-> BAT: %d (only first phoneme differs)\n", d_cat_bat);
    printf("    CAT <-> DOG: %d (all phonemes differ)\n", d_cat_dog);
    printf("    BAT <-> DOG: %d (all phonemes differ)\n", d_bat_dog);
  }
}

// Demo: Use cases
static void demo_use_cases(void) {
  if (!json_mode) {
    ascii_section("4. Phoneme Recognition Use Cases");
    printf("  Applications for embedded speech\n\n");
  }

  if (!json_mode) {
    printf("  1. Keyword Spotting (KWS)\n");
    printf("     - Match spoken word to phoneme patterns\n");
    printf("     - Fuzzy matching tolerates pronunciation variants\n\n");

    printf("  2. Voice Commands\n");
    printf("     - YES [Y EH S]\n");
    printf("     - NO  [N OW]\n");
    printf("     - STOP [S T AA P]\n");
    printf("     - GO   [G OW]\n\n");

    printf("  3. Speech Synthesis (TTS)\n");
    printf("     - Convert text to phoneme sequence\n");
    printf("     - Drive speech synthesizer\n\n");

    printf("  4. Pronunciation Verification\n");
    printf("     - Compare spoken phonemes to expected\n");
    printf("     - Score pronunciation accuracy\n");
  }
}

int main(int argc, char **argv) {
  demo_cli_result_t result = demo_parse_args(
      argc, argv, "phoneme_cmd_demo", "Phoneme-based text processing demo");

  if (result == DEMO_EXIT)
    return 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      json_mode = true;
  }

  if (!json_mode) {
    printf("\n");
    ascii_section("EIF Phoneme Processing Demo");
    printf("  CMU ARPABET phoneme representation\n\n");
  }

  demo_phoneme_basics();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_phoneme_types();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_edit_distance();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_use_cases();

  if (!json_mode) {
    ascii_section("Summary");
    printf("  Phoneme processing provides:\n");
    printf("    - Language-independent speech representation\n");
    printf("    - Compact vocabulary encoding\n");
    printf("    - Fuzzy matching for recognition\n");
    printf("    - Foundation for TTS and ASR\n\n");
  }

  return 0;
}
