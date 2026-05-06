#include "mata/nfa/colors.hh"
#include "mata/alphabet.hh"
#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/utils/synchronized-iterator.hh"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace mata::nfa;

using Symbol = unsigned;
using Word = std::vector<Symbol>;

TEST_CASE("ColorFormula") {
    ColorFormula trueFormula = ColorFormula(ColorFormula::OperatorType::True);
    CHECK(trueFormula.get_truth_value({}, {}) == true);
    ColorFormula falseFormula = ColorFormula(ColorFormula::OperatorType::False);
    CHECK(falseFormula.get_truth_value({}, {}) == false);
}

TEST_CASE("ColorsNfa::is_empty_lang() - basic false") {
    int occurColor = 0;
    size_t num_states = 3;
    Word one_char_word = {0};

    ColorFormula acceptF = ColorFormula(ColorFormula::OperatorType::Occurs, occurColor);
    ColorsNfa c_nfa = ColorsNfa(num_states,acceptF, {}, {0});

    c_nfa.add_colors_to_state(0, {1});
    c_nfa.add_colors_to_state(1, {2,0});
    c_nfa.add_colors_to_state(2, {2,1});
    // add transitions
    c_nfa.insert_word(0, one_char_word, 1);
    c_nfa.insert_word(1, one_char_word, 2);

    CHECK(c_nfa.is_lang_empty() == false);
}

TEST_CASE("ColorsNfa::is_empty_lang() - basic true") {
    int occurColor = 3;
    size_t num_states = 3;
    Word one_char_word = {0};

    ColorFormula acceptF = ColorFormula(ColorFormula::OperatorType::Occurs, occurColor);
    ColorsNfa c_nfa = ColorsNfa(num_states,acceptF, {}, {0});

    c_nfa.add_colors_to_state(0, {1});
    c_nfa.add_colors_to_state(1, {2,0});
    c_nfa.add_colors_to_state(2, {2,1});
    // add transitions
    c_nfa.insert_word(0, one_char_word, 1);
    c_nfa.insert_word(1, one_char_word, 2);

    CHECK(c_nfa.is_lang_empty() == true);
}