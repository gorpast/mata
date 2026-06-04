#pragma once

#include <vector>
#include <set>
#include <optional>
#include <functional>

#include "delta.hh"
#include "nfa.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/utils/synchronized-iterator.hh"

namespace mata::nfa
{

    using ColorNum = int;
    using ColorSet = std::set<ColorNum>;
    // TODO global constant change
    constexpr ColorNum invalidColorNum = -1;

    class ColorFormula {
        public:

            enum class OperatorType {
                Not,
                And,
                Or,
                Last,
                Occurs,
                True,
                False,
                NoOperand
            };

            // stores operation of the ColorFormula
            OperatorType ops;
            // subformalas connected with same ops
            std::vector<ColorFormula> children = {};

            // -1 is invalid value for ColorNum
            ColorNum color_check = invalidColorNum;

            // empty constructor
            ColorFormula(): ops(OperatorType::NoOperand) {} 

            ColorFormula(OperatorType ops, ColorNum color_check = invalidColorNum, std::vector<ColorFormula> children = {}):
                ops(ops), children(children), color_check(color_check) {}

            bool get_truth_value(ColorSet colors, ColorSet last_color) {
                if (ops == OperatorType::Last) { return std::find(last_color.begin(), last_color.end(), color_check) != last_color.end(); }
                else if (ops == OperatorType::Occurs) { return std::find(colors.begin(), colors.end(), color_check) != colors.end(); }
                else if (ops == OperatorType::True) { return true; }
                else if (ops == OperatorType::False) { return false; }
                else if (ops == OperatorType::Not) {
                    assert(children.size() == 1);
                    return !children[0].get_truth_value(colors, last_color);
                } else if (ops == OperatorType::And) {
                    assert(children.size() != 0);
                    for (auto &child: children) {
                        if (! child.get_truth_value(colors, last_color)) return false;
                    }
                    return true;
                } else if (ops == OperatorType::Or) {
                    assert(children.size() != 0);
                    for (auto &child: children) {
                        if (child.get_truth_value(colors, last_color)) return true;
                    }
                    return false;
                }
                // shouldn't be able to get there
                assert(false);
            }

            void add_child(ColorFormula new_child) { children.push_back(new_child); }

            void set_operator(OperatorType new_op, ColorNum new_color_check = invalidColorNum) {
                ops = new_op;
                if (new_op == OperatorType::Last || new_op == OperatorType::Occurs) {
                    color_check = new_color_check;
                }
            }

            std::string print_formula() const {
                if (ops == OperatorType::True) {
                    return "true";
                } else if (ops == OperatorType::False) {
                    return "false";
                } else if (ops == OperatorType::Occurs) {
                    return "(occurs " + std::to_string(color_check) + ")";
                } else if (ops == OperatorType::Or) {
                    std::string s = "(or ";
                    for (auto child : children) {
                        s += child.print_formula();
                    }
                    return s + ")";
                } else if (ops == OperatorType::And) {
                    std::string s = "(and";
                    for (auto child : children) {
                        s += " " + child.print_formula();
                    }
                    return s + ")";
                }
                return "";
            }

            ColorFormula trim_formula(ColorSet active_colors) {
                // if (ops == OperatorType::Last) { return std::find(last_color.begin(), last_color.end(), color_check) != last_color.end(); }
                if (ops == OperatorType::Occurs) {
                    if (std::find(active_colors.begin(), active_colors.end(), color_check) == active_colors.end()) {
                        return ColorFormula(OperatorType::False);
                    }
                    return *this;
                }
                else if (ops == OperatorType::True) { return *this; }
                else if (ops == OperatorType::False) { return *this; }
                else if (ops == OperatorType::Not) {
                    assert(children.size() == 1);
                    if (children[0].ops == OperatorType::False) {
                        return ColorFormula(OperatorType::True);
                    } else if (children[0].ops == OperatorType::True) {
                        return ColorFormula(OperatorType::False);
                    } else {
                        return *this;
                    }
                } else if (ops == OperatorType::And) {
                    if (children.size() == 0) {
                        return ColorFormula(OperatorType::False);
                    }
                    for (int ind = 0; ind < children.size(); ind++) {
                        ColorFormula new_child = children[ind].trim_formula(active_colors);
                        if(new_child.ops == OperatorType::False) return ColorFormula(OperatorType::False);
                        children[ind] = new_child;
                    }
                    return *this;
                } else if (ops == OperatorType::Or) {
                    if (children.size() == 0) {
                        return ColorFormula(OperatorType::False);
                    }
                    for (int ind = 0; ind < children.size(); ind++) {
                        ColorFormula new_child = children[ind].trim_formula(active_colors);
                        if(new_child.ops == OperatorType::True) return ColorFormula(OperatorType::True);
                        children[ind] = new_child;
                    }
                    return *this;
                }
                // shouldn't be able to get there
                // assert(false);
                return *this;
            }


    };

    class ColorsNfa : public Nfa {
        private:

            // stores accepting formula
            ColorFormula accept_formula = ColorFormula();
            // stores colors for each state
            std::vector<ColorSet> color_vector = {};

        public:
            // TODO do something with this 
            explicit ColorsNfa(const size_t num_of_states, ColorFormula accept_formula, std::vector<ColorSet> input_color_vector = {}, utils::SparseSet<State> initial_states = {},
                    utils::SparseSet<State> final_states = {}, Alphabet* alphabet = nullptr)
                : Nfa(num_of_states, initial_states, final_states, alphabet), accept_formula(accept_formula), color_vector(input_color_vector) {
                    color_vector.resize(num_of_states + 1);
                }

            explicit ColorsNfa(Nfa nfa, ColorFormula accept_formula, std::vector<ColorSet> input_color_vector = {})
                : Nfa(nfa), accept_formula(accept_formula), color_vector(input_color_vector) {
                    color_vector.resize(nfa.num_of_states() + 1);
                }

            void add_colors_to_state(State source, ColorSet colors) {

                if (is_state(source)) {
                    color_vector[source] = colors;
                } else {
                    add_state(source);
                    color_vector.resize(num_of_states()+1);
                    color_vector[source] = colors;
                }
            }

            void add_color_to_current(State source, ColorSet new_colors) {

                if (is_state(source)) {
                    color_vector[source].merge(new_colors);
                } else {
                    add_state(source);
                    color_vector.resize(num_of_states()+1);
                    color_vector[source] = new_colors;
                }
            }

            void set_accept_formula(ColorFormula new_formula) {
                accept_formula = new_formula;
            }

            ColorSet get_color_set(State src) const {
                return color_vector[src];
            }

            std::vector<ColorSet> get_color_vector() const {
                return color_vector;
            }

            void resize_color_vector() {
                color_vector.resize(this->num_of_states());
            }
            
            bool has_colors() const {
                for (auto color_set: color_vector) {
                    if (color_set.size() != 0) {
                        return true;
                    }
                }
                return false;
            }

            ColorFormula get_accept_formula() const {
                return accept_formula;
            }

            bool is_lang_empty(void) {
                // setting callback for tarjan SCC
                std::unordered_map<State, State> state_to_scc;
                State current_scc = 0;

                TarjanDiscoverCallback cb;

                cb.scc_discover = [&](const std::vector<State>& scc,
                                    const std::vector<State>& /*stack*/) {
                    for (State s : scc) {
                        state_to_scc[s] = current_scc;
                    }
                    current_scc++;
                    return false;
                };

                // state_to_scc should have mapping between SCCs and states of Nfa
                tarjan_scc_discover(cb);

                ColorsNfa graph = create_scc_graph(state_to_scc, current_scc);

                // TODO probably good idea to connect all initial states
                // get initial SCCs
                std::set<State> initial_scc = get_initial_sccs(state_to_scc);

                // start the algorithm
                for (State init_scc: initial_scc) {
                    ColorSet walk_colors = {};

                    // take a walk - if found possible solution language is not empty
                    if (expand(init_scc, walk_colors, graph)) {
                        return false;
                    }
                }

                // all walks failed
                return true;
            }

            bool expand(State cur_state, ColorSet foundColors, ColorsNfa &scc_graph) {
                // adding colors to the found set
                foundColors.merge(scc_graph.color_vector[cur_state]);

                // leaf node of scc_graph
                if (scc_graph.delta[cur_state].size() == 0) {
                    return accept_formula.get_truth_value(foundColors, scc_graph.color_vector[cur_state]);
                }

                // loop through all successors of the node
                for (const auto& symbol : scc_graph.delta[cur_state]) {
                    StateSet next_states = symbol.targets;
                    for (State next_state : next_states) {
                        // found possible solution language is not empty
                        if (expand(next_state, foundColors, scc_graph)) {
                            return false;
                        }

                    }
                }

                return true;

            }

            std::set<State> get_initial_sccs(std::unordered_map<State, State> state_to_scc) {
                std::set<State> initial_sccs = {};
                for (State s: initial) {
                    initial_sccs.insert(state_to_scc[s]);
                }

                return initial_sccs;
            }

            ColorsNfa create_scc_graph(std::unordered_map<State, State> state_to_scc, State num_scc) {
                // adding states to graph
                ColorsNfa res_graph = ColorsNfa(num_scc, accept_formula);

                // TODO think that can use insert word with one char to add one new transitions
                Word one_char_word = {0};

                // TODO improvement - dont need to iterate over SCC when iterating over transitions
                // adding transitions
                for (State i = 0; i < num_scc; i++) {
                    for (State j = 0; j < num_scc; j++) {
                        if (i == j) continue;

                        // iterator over transitions
                        for (State s = 0; s < delta.num_of_states(); s++) {
                            if (state_to_scc.find(s) != state_to_scc.end()) {
                                continue;
                            }
                            for (const auto& symbol : delta[s]) {
                                StateSet targets = symbol.targets;
                                for (State t : targets) {
                                    if (state_to_scc.find(t) != state_to_scc.end()) {
                                        continue;
                                    }

                                    // check if there is connection between two SCCs
                                    if (state_to_scc[s] == i && state_to_scc[t] == j) {
                                        res_graph.insert_word(i, one_char_word, j);
                                    }

                                }
                            }
                        }

                    }
                }

                // setting colors for all SCCs
                for (State color_aut_node = 0; color_aut_node < num_scc; color_aut_node++) {
                    res_graph.add_color_to_current(state_to_scc[color_aut_node], color_vector[color_aut_node]);
                }

                return res_graph;
            }

            //! okay this is porobably not going to work, beacauseI cant specify that I want to concatenate over epsilon
            //! for now its okay but will change later
            void concatenate(const ColorsNfa& aut) {
                // to concatenate color nfas, just append color vectors
                resize_color_vector();
                color_vector.insert(color_vector.end(), aut.color_vector.begin(), aut.color_vector.end());

                // TODO what to do with formulas
                // now the automat must do both formulas
                ColorFormula cf = ColorFormula(ColorFormula::OperatorType::And);

                cf.add_child(accept_formula);
                cf.add_child(aut.get_accept_formula());

                set_accept_formula(cf);

                // Nfa::concatenate(aut);
                // const size_t n = this->num_of_states();
                // auto upd_fnc = [&](const State st) {
                //     return st + n;
                // };

                // // copy the information about aut to save the case when this is the same object as aut.
                // utils::SparseSet<mata::nfa::State> aut_initial = aut.initial;
                // utils::SparseSet<mata::nfa::State> aut_final = aut.final;
                // const size_t aut_n = aut.num_of_states();

                // this->delta.allocate(n);
                // this->delta.append(aut.delta.renumber_targets(upd_fnc));

                // // set accepting states
                // utils::SparseSet<State> new_fin{};
                // new_fin.reserve(n+aut_n);
                // for(const State& aut_fin : aut_final) {
                //     new_fin.insert(upd_fnc(aut_fin));
                // }

                // // connect both parts
                // for(const State& ini : aut_initial) {
                //     const StatePost& ini_post = this->delta[upd_fnc(ini)];
                //     // is ini state also final?
                //     const bool is_final = aut_final[ini];
                //     for(const State& fin : this->final) {
                //         if(is_final) {
                //             new_fin.insert(fin);
                //         }
                //         for(const SymbolPost& ini_mv : ini_post) {
                //             // TODO: this should be done efficiently in a delta method
                //             // TODO: in fact it is not efficient for now
                //             for(const State& dest : ini_mv.targets) {
                //                 std::cout << dest << " dest there\n";
                //                 this->delta.add(fin, ini_mv.symbol, dest);
                //                 add_color_to_current(fin, aut.get_color_set(ini));
                //             }
                //         }
                //     }
                // }
                // this->final = new_fin;

            }

        //! TODO create better version of trim that uses more data from colors
        ColorsNfa trim() {
            StateRenaming sr;

            Nfa nfa = Nfa::trim(&sr);
            ColorsNfa new_colors = ColorsNfa(nfa, ColorFormula());

            for (auto pair: sr) {
                new_colors.add_color_to_current(pair.second, color_vector[pair.first]);
            }

            return new_colors;
        }

        void inherit_colors(ColorsNfa source) {
            for (int s = 0; s < num_of_states(); s++) {
                add_color_to_current(s, source.get_color_set(s));
            }
        }

        // printing to given format
        std::string print_to_mata() const {
            std::string format_string = Nfa::print_to_mata();
            format_string += "Colors:\n"; // new line between automaton and its colors
            for (int ind = 0; ind < num_of_states(); ind++) {
                std::string color_string = "";
                for (auto col : color_vector[ind]) {
                    color_string += std::to_string(col) + " ";
                }
                format_string += color_string + "\n";
            }

            format_string += "Formula:\n"; // new line between colors and formula

            format_string += accept_formula.print_formula() + "\n";

            return format_string;
        }

        Nfa unwind_colors(State &color_state) const {

            Nfa new_nfa = *this;

            // ! now I create just one state and put different edges to different colors
            color_state = new_nfa.add_state();

            // when I find final state I have gave it a new color so it wont connect with other color states
            // final states have implicit color 0
            for (auto final_state : new_nfa.final) {
                new_nfa.delta.add(final_state, EPSILON - 5, color_state);
            }
            // remove current final state - they already have color
            new_nfa.final.clear();
            new_nfa.final.insert(color_state);

            // for each color in state's color set add transition to new color state
            for (unsigned ind = 0; ind < this->num_of_states(); ind++) {
                for (auto color : color_vector[ind]) {
                    State target = color_state; // well its state of "potential" color 0 + the true value of color
                    new_nfa.delta.add(ind, EPSILON - 5 - color, target); //! dont know which epsilon to use
                }
            }

            return new_nfa;
        }

        std::string print_to_dot() const {
            std::string format_string = Nfa::print_to_dot();
            format_string += "Colors:\n"; // new line between automaton and its colors
            for (int ind = 0; ind < num_of_states(); ind++) {
                std::string color_string = "";
                for (auto col : color_vector[ind]) {
                    color_string += std::to_string(col) + " ";
                }
                format_string += color_string + "\n";
            }

            format_string += "Formula:\n"; // new line between colors and formula

            format_string += accept_formula.print_formula() + "\n";

            return format_string;
        }
    };

    ColorsNfa concatenate(const ColorsNfa& lhs, const ColorsNfa& rhs, const Symbol epsilon = EPSILON);

    ColorsNfa intersection(const ColorsNfa& lhs, const ColorsNfa& rhs, const Symbol first_epsilon = EPSILON);

    ColorsNfa colors_reduce_simulation(const ColorsNfa& aut, StateRenaming &state_renaming);

    ColorsNfa reduce(const ColorsNfa &aut, StateRenaming *state_renaming = nullptr);

    ColorsNfa color_trim(
        const ColorsNfa& aut, StateRenaming* state_renaming = nullptr,
        std::optional<std::reference_wrapper<const utils::SparseSet<State>>> initial_states = std::nullopt,
        std::optional<std::reference_wrapper<const utils::SparseSet<State>>> final_states = std::nullopt);

    ColorsNfa remove_epsilon_color(const ColorsNfa& aut, Symbol epsilon = EPSILON);
}