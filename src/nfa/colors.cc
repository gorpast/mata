#include "mata/nfa/algorithms.hh"
#include "mata/nfa/colors.hh"


namespace mata::nfa {

    ColorsNfa colors_reduce_simulation(const ColorsNfa& aut, StateRenaming &state_renaming) {
        State first_color_state;

        Nfa simulation_nfa = aut.unwind_colors(first_color_state);

        Nfa new_nfa = mata::nfa::algorithms::reduce_simulation(simulation_nfa, state_renaming);

        ColorsNfa new_colors_nfa = ColorsNfa(new_nfa, aut.get_accept_formula());

        std::vector<State> renaming = {};
        BoolVector is_staying = {};
        long unsigned counter = 0;

        for (State state = 0; state < new_colors_nfa.num_of_states(); state++) {
            // TODO will get all the color states get mapped to bigger number?
            if (state >= state_renaming[first_color_state]) {
                renaming.push_back(counter++);
                is_staying.push_back(0);
                new_colors_nfa.final.erase(state);
                continue;
            }
            renaming.push_back(counter++);
            is_staying.push_back(1);

        }
        
        // passing colors back to the state
        for (auto rename_pair : state_renaming) {
            // want to ignore color states that were added to preserve structure
            if (rename_pair.second >= state_renaming[first_color_state]) {
                continue;
            }
            new_colors_nfa.add_color_to_current(rename_pair.second, aut.get_color_set(rename_pair.first));

            if (std::find(aut.final.begin(), aut.final.end(), rename_pair.first) != aut.final.end()) {
                new_colors_nfa.final.insert(rename_pair.second);
            }
        }

        new_colors_nfa.final.truncate();

        new_colors_nfa.delta = new_colors_nfa.delta.defragment(is_staying, renaming);

        new_colors_nfa.resize_color_vector();

        return new_colors_nfa;
    }

    ColorsNfa reduce(const ColorsNfa &aut, StateRenaming *state_renaming) {

        StateRenaming sr;
        if (state_renaming == nullptr) {
            state_renaming = &sr;
        }

        ColorsNfa new_color_aut = colors_reduce_simulation(aut, *state_renaming);
            
        return new_color_aut;
    }

    ColorsNfa color_trim(
        const ColorsNfa& aut, StateRenaming* state_renaming,
        std::optional<std::reference_wrapper<const utils::SparseSet<State>>> initial_states,
        std::optional<std::reference_wrapper<const utils::SparseSet<State>>> final_states) {
        if (!initial_states) { initial_states = aut.initial; }
        if (!final_states) { final_states = aut.final; }

        StateRenaming sr;
        if (state_renaming == nullptr) {
            state_renaming = &sr;
        }

        Nfa nfa = trim(aut, state_renaming, initial_states, final_states);
        ColorsNfa new_colors = ColorsNfa(nfa, ColorFormula());

        for (auto pair: sr) {
            new_colors.add_color_to_current(pair.second, aut.get_color_set(pair.first));
        }

        return new_colors;
    }
}
