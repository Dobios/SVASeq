## Generated with ChatGPT 3
## This script defines classes for NFAs and DFAs, 
## as well as functions for computing epsilon closures and transitions. 
## The nfa_to_dfa function performs the conversion from an NFA to a DFA. 
## The example at the end demonstrates how to create an NFA and 
## convert it to a DFA using the provided functions.

from collections import deque, defaultdict

class NFA:
    def __init__(self, states, alphabet, transitions, start_state, accept_states):
        self.states = states
        self.alphabet = alphabet
        self.transitions = transitions
        self.start_state = start_state
        self.accept_states = accept_states

def epsilon_closure(nfa, states):
    closure = set(states)
    stack = list(states)

    while stack:
        current_state = stack.pop()
        if current_state in nfa.transitions and None in nfa.transitions[current_state]:
            for state in nfa.transitions[current_state][None]:
                if state not in closure:
                    closure.add(state)
                    stack.append(state)

    return frozenset(closure)

def move(nfa, states, symbol):
    result = set()

    for state in states:
        if state in nfa.transitions and symbol in nfa.transitions[state]:
            result.update(nfa.transitions[state][symbol])

    return frozenset(result)

def nfa_to_dfa(nfa):
    dfa_states = set()
    dfa_transitions = defaultdict(dict)
    dfa_start_state = epsilon_closure(nfa, {nfa.start_state})
    dfa_accept_states = set()

    queue = deque([dfa_start_state])
    visited = set()

    while queue:
        current_states = queue.popleft()

        if current_states in visited:
            continue

        visited.add(current_states)
        dfa_states.add(current_states)

        for symbol in nfa.alphabet:
            next_states = epsilon_closure(nfa, move(nfa, current_states, symbol))

            dfa_transitions[current_states][symbol] = next_states

            if next_states not in visited:
                queue.append(next_states)

        if any(state in nfa.accept_states for state in current_states):
            dfa_accept_states.add(current_states)

    return DFA(dfa_states, nfa.alphabet, dfa_transitions, dfa_start_state, dfa_accept_states)

class DFA:
    def __init__(self, states, alphabet, transitions, start_state, accept_states):
        self.states = states
        self.alphabet = alphabet
        self.transitions = transitions
        self.start_state = start_state
        self.accept_states = accept_states

def print_dfa(dfa):
    print("DFA States:", dfa.states)
    print("Alphabet:", dfa.alphabet)
    print("Transitions:")
    for current_state, transitions in dfa.transitions.items():
        for symbol, next_state in transitions.items():
            print(f"{current_state} --({symbol})--> {next_state}")
    print("Start State:", dfa.start_state)
    print("Accept States:", dfa.accept_states)

if __name__ == "__main__":
    # Example usage:

    nfa = NFA(
        states={'q0', 'q1', 'q2'},
        alphabet={'a', 'b'},
        transitions={'q0': {'a': {'q1'}, None: {'q2'}}, 'q1': {'b': {'q2'}}, 'q2': {}},
        start_state='q0',
        accept_states={'q2'}
    )

    dfa = nfa_to_dfa(nfa)

    print_dfa(dfa)
