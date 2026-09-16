#include "mata/nfa/algorithms.hh"
#include "mata/nfa/colors.hh"
#include "mata/nfa/simulation.hh"

#include <vector>
#include <algorithm>


namespace mata::nfa {

    Nfa colors_new_simulation_reduce(Nfa &nfa, StateRenaming &state_renaming) {

        std::cout << "Hello simulation\n" << nfa.print_to_dot() << std::endl;
        auto simulation = simulation::Simulation(nfa).compute_simulation();
        std::cout << "Computed simulation\n";

        Nfa new_nfa = Nfa();

        // TODO should change this transition handling
        std::vector<State> states_to_get_transitions = {};

        // TODO don't know if can loop through states like this
        for (State state = 0; state < nfa.num_of_states(); state++) {
            // TODO is my own state in equi_st??
            std::vector<State> equi_st = simulation.partition_block(state);

            //! WRONG STRATEGY should chnage:
            // when I first see the given partition I add a new state to reduced Nfa
            // will see if it is first when the minimum state in equi_st is my `state` variable
            State minimum = *std::min_element(equi_st.begin(), equi_st.end());
            if (minimum >= state) {
                State new_state = new_nfa.add_state();

                state_renaming[state] = new_state;

                states_to_get_transitions.push_back(state);
            } else {
                state_renaming[state] = state_renaming.at(minimum);
            }
        }

        //! find better way to do it
        for (State state: states_to_get_transitions) {
            StatePost st_post = nfa.delta[state];
            // loop through its transitions and add these transition to the new_nfa

            for (auto symbol: st_post) {
                for (State target: symbol.targets) {

                    new_nfa.delta.add(state_renaming[state], symbol.symbol, state_renaming[target]);
                }
            }
        }

        return new_nfa;

    }

    ColorsNfa colors_reduce_simulation(const ColorsNfa& aut, StateRenaming &state_renaming) {
        State first_color_state;

        Nfa simulation_nfa = aut.unwind_colors(first_color_state);

        // std::cout << "before simulation\n" << simulation_nfa.print_to_dot() << std::endl;
        Nfa new_nfa = colors_new_simulation_reduce(simulation_nfa, state_renaming);
        // Nfa new_nfa = mata::nfa::algorithms::reduce_simulation(simulation_nfa, state_renaming);

        // std::cout << "After simulation\n" << new_nfa.print_to_dot() << std::endl;

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
        ColorsNfa new_colors = ColorsNfa(nfa);

        for (auto pair: sr) {
            new_colors.add_color_to_current(pair.second, aut.get_color_set(pair.first));
        }

        return new_colors;
    }

    // ColorsNfa better_color_trim(
    //     const ColorsNfa& aut, StateRenaming* state_renaming,
    //     std::optional<std::reference_wrapper<const utils::SparseSet<State>>> initial_states,
    //     std::optional<std::reference_wrapper<const utils::SparseSet<State>>> final_states) {

    //     if (!initial_states) { initial_states = aut.initial; }
    //     if (!final_states) { final_states = aut.final; }

    //     StateRenaming sr;
    //     if (state_renaming == nullptr) {
    //         state_renaming = &sr;
    //     }

    //     // setting callback for tarjan SCC
    //     std::unordered_map<State, State> state_to_scc;

    //     State current_scc = 0;

    //     Nfa::TarjanDiscoverCallback cb;

    //     cb.scc_discover = [&](const std::vector<State>& scc,
    //                         const std::vector<State>& /*stack*/) {
    //         for (State s : scc) {
    //             state_to_scc[s] = current_scc;
    //         }
    //         current_scc++;
    //         return false;
    //     };

    //     // state_to_scc should have mapping between SCCs and states of Nfa
    //     tarjan_scc_discover(cb);

    //     ColorsNfa graph = create_scc_graph(state_to_scc, current_scc);

    //     // TODO probably good idea to connect all initial states
    //     // get initial SCCs
    //     std::set<State> initial_scc = get_initial_sccs(state_to_scc);

    //     // start the algorithm
    //     for (State init_scc: initial_scc) {
    //         ColorSet walk_colors = {};

    //         // take a walk - if found possible solution language is not empty
    //         if (expand(init_scc, walk_colors, graph)) {
    //             return false;
    //         }
    //     }

    //     // all walks failed
    //     return true;

    // }

    //! I hate myself for once again copying code, but need just a small tweek to the logic
    ColorsNfa remove_epsilon_color(const ColorsNfa& aut, Symbol epsilon) {
        // cannot use multimap, because it can contain multiple occurrences of (a -> a), (a -> a)
        std::unordered_map<State, StateSet> epsilon_closure;

        // TODO: grossly inefficient
        // first we compute the epsilon closure
        const size_t num_of_states{aut.num_of_states() };
        for (size_t i{ 0 }; i < num_of_states; ++i) {
            for (const auto& trans : aut.delta[i]) {
                const auto [it, inserted] = epsilon_closure.insert({ i, { i } });
                if (trans.symbol == epsilon) {
                    StateSet& closure = it->second;
                    // TODO: Fix possibly insert to OrdVector. Create list already ordered, then merge (do not need to resize each time);
                    closure.insert(trans.targets);
                }
            }
        }

        bool changed = true;
        while (changed) { // Compute the fixpoint.
            changed = false;
            for (size_t i = 0; i < num_of_states; ++i) {
                const StatePost& post{ aut.delta[i] };
                //TODO: make faster if default epsilon
                if (const auto eps_move_it{ post.find(epsilon) }; eps_move_it != post.end()) {
                    StateSet& src_eps_cl = epsilon_closure[i];
                    for (const State tgt : eps_move_it->targets) {
                        const StateSet& tgt_eps_cl = epsilon_closure[tgt];
                        for (const State st: tgt_eps_cl) {
                            if (src_eps_cl.count(st) == 0) {
                                changed = true;
                                break;
                            }
                        }
                        src_eps_cl.insert(tgt_eps_cl);
                    }
                }
            }
        }

        // Construct the automaton without epsilon transitions.
        Nfa result_nfa{ Delta{}, aut.initial, aut.final, aut.alphabet };
        ColorsNfa result = ColorsNfa(result_nfa, aut.get_accept_formula(), aut.get_color_vector());
        for (const auto& [state, closure_states] : epsilon_closure) {
            for (const State eps_cl_state : closure_states) {
                if (aut.final[eps_cl_state]) result.final.insert(state);
                for (const SymbolPost& move : aut.delta[eps_cl_state]) {
                    if (move.symbol == epsilon) continue;
                    // TODO: this could be done more efficiently if we had a better add method
                    for (const State tgt_state : move.targets) {
                        result.delta.add(state, move.symbol, tgt_state);
                    }
                }
            }
        }
        return result;
        }
}
