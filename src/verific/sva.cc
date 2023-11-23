#include <vector>
#include <map>
#include <assert.h>
#include <string>

using namespace std;

// ========================================
// Redefined Yosys Constructs
// ========================================
#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y) CONCAT_IMPL(x, y)

string escape_id(const std::string &input) {
    string result;
    for (char ch : input) {
        if (ch == ' ' || ch == '@') {
            result.push_back('_');
        } else {
            result.push_back(ch);
        }
    }
    return result;
}

// Reimplementation of the Yosys GetSize method without RTLIL (done by chatgpt)
template <typename T>
int GetSize(T const& container) {
    return static_cast<int>(container.size());
}

// Reimplementation of the Yosys NEW_ID without RTLIL (done by chatgpt)
#define NEW_ID \
    ([]() { \
        static int CONCAT(id_, __LINE__) = 0; \
        string id_string = __FILE__ " @ " + std::to_string(__LINE__) + " @ " + __FUNCTION__; \
        string escaped_id = escape_id(id_string); \
        return escaped_id + std::to_string(CONCAT(id_, __LINE__)++); \
    }())
    
struct Wire;
struct SigSpec ;

struct SigBit {
public:
    const bool operator==(SigBit & sb) const;
    const bool operator!=(SigBit & sb) const;
    SigBit & operator=(Wire* w);
    SigBit & operator=(SigSpec a);
    void append(SigBit& a);
};

struct Wire {
public:
    SigBit signal;
};

struct SigSpec {
public:
    SigSpec* ctrl;
    SigBit node;

    int size() const { return 1; }
    void append(SigBit const& a) {}
    void append(SigSpec const& a) {}
    void sort_and_unify() {}
};

const bool SigBit::operator==(SigBit & sb) const { return true; }
const bool SigBit::operator!=(SigBit & sb) const { return true; }
SigBit & SigBit::operator=(Wire* w) { return *this; }
SigBit & SigBit::operator=(SigSpec a) { return *this; }
void SigBit::append(SigBit& a) {}

struct Const {};
struct Module {
    std::vector<Wire*> wires;

    // Placeholder implementation for Or method
    SigBit Or(string ID, const SigBit & a, const SigBit & b) {
        return a;
    }

    // Placeholder implementation for And method
    SigBit And(string ID, const SigBit & a, const SigBit & b) {
        return a;
    }

    // Placeholder implementation for Not method
    SigBit Not(string ID, const SigBit & a) {
        return a;
    }

    // Placeholder implementation for ReduceOr method
    SigBit ReduceOr(string ID, const SigSpec & a) {
        return SigBit();
    }

    void connect(SigBit a, SigBit b) {}
    void connect(Wire* a, SigBit b) {}

    // Placeholder implementation for creating a new wire with a unique ID
    Wire* newWire(string name) {
        Wire* new_wire = new Wire();
        wires.push_back(new_wire);
        // Add logic for generating a unique ID
        return new_wire;
    }

    // Placeholder implementation for creating a new wire with a unique ID
    Wire* addWire(string name) {
        Wire* new_wire = new Wire();
        wires.push_back(new_wire);
        // Add logic for generating a unique ID
        return new_wire;
    }
};

struct VerificClocking {
public:
    Module* module;

    void addDff(string id, SigBit next, Wire* w, SigBit state) {}
};

struct State {
    static SigBit S0;
    static SigBit S1;
    static SigBit Sx;
};

// ========================================
// VerificSVA.cc
// ========================================

// Non-deterministic FSM
struct SvaNFsmNode {
	// Edge: Activate the target node if ctrl signal is true, consumes clock cycle
	// Link: Activate the target node if ctrl signal is true, doesn't consume clock cycle
	vector<pair<int, SigBit>> edges, links;
	bool is_cond_node;
};

// Non-deterministic FSM after resolving links
struct SvaUFsmNode {
	// Edge: Activate the target node if all bits in ctrl signal are true, consumes clock cycle
	// Accept: This node functions as an accept node if all bits in ctrl signal are true
	vector<pair<int, SigSpec>> edges;
	vector<SigSpec> accept, cond;
	bool reachable;
};

// Deterministic FSM
struct SvaDFsmNode {
	// A DFSM state corresponds to a set of NFSM states. We represent DFSM states as sorted vectors
	// of NFSM state node ids. Edge/accept controls are constants matched against the ctrl sigspec.
	SigSpec ctrl;
	vector<pair<vector<int>, Const>> edges;
	vector<Const> accept, reject;

	// additional temp data for getReject()
	Wire *ffoutwire;
	SigBit statesig;
	SigSpec nextstate;

	// additional temp data for getDFsm()
	int outnode;
};

struct SvaFsm {
	Module *module;
	VerificClocking clocking;

	SigBit trigger_sig = State::S1, disable_sig;
	SigBit throughout_sig = State::S1;
	bool in_cond_mode = false;

	vector<SigBit> disable_stack;
	vector<SigBit> throughout_stack;

	int startNode, acceptNode, condNode;
	vector<SvaNFsmNode> nodes;

	vector<SvaUFsmNode> unodes;
	map<vector<int>, SvaDFsmNode> dnodes;
	map<pair<SigSpec, SigSpec>, SigBit> cond_eq_cache;
	bool materialized = false;

	SigBit final_accept_sig = State::Sx;
	SigBit final_reject_sig = State::Sx;

    SvaFsm(const VerificClocking &clking, SigBit trig = State::S1) {
		module = clking.module;
		clocking = clking;
		trigger_sig = trig;

		startNode = createNode();
		acceptNode = createNode();

		in_cond_mode = true;
		condNode = createNode();
		in_cond_mode = false;
	}

    void pushDisable(SigBit sig) {
		assert(!materialized);

		disable_stack.push_back(disable_sig);

		if (disable_sig == State::S0)
			disable_sig = sig;
		else
			disable_sig = module->Or(NEW_ID, disable_sig, sig);
	}

	void popDisable() {
		assert(!materialized);
		assert(!disable_stack.empty());

		disable_sig = disable_stack.back();
		disable_stack.pop_back();
	}

	void pushThroughout(SigBit sig) {
		assert(!materialized);

		throughout_stack.push_back(throughout_sig);

		if (throughout_sig == State::S1)
			throughout_sig = sig;
		else
			throughout_sig = module->And(NEW_ID, throughout_sig, sig);
	}

	void popThroughout() {
		assert(!materialized);
		assert(!throughout_stack.empty());

		throughout_sig = throughout_stack.back();
		throughout_stack.pop_back();
	}

	int createNode(int link_node = -1){
		assert(!materialized);

		int idx = GetSize(nodes);
		nodes.push_back(SvaNFsmNode());
		nodes.back().is_cond_node = in_cond_mode;
		if (link_node >= 0)
			createLink(link_node, idx);
		return idx;
	}

    int createStartNode() {
		return createNode(startNode);
	}

	void createEdge(int from_node, int to_node, SigBit ctrl = State::S1) {
		assert(!materialized);
		assert(0 <= from_node && from_node < GetSize(nodes));
		assert(0 <= to_node && to_node < GetSize(nodes));
		assert(from_node != acceptNode);
		assert(to_node != acceptNode);
		assert(from_node != condNode);
		assert(to_node != condNode);
		assert(to_node != startNode);

		if (from_node != startNode)
			assert(nodes.at(from_node).is_cond_node == nodes.at(to_node).is_cond_node);

		if (throughout_sig != State::S1) {
			if (ctrl != State::S1)
				ctrl = module->And(NEW_ID, throughout_sig, ctrl);
			else
				ctrl = throughout_sig;
		}

		nodes[from_node].edges.push_back(make_pair(to_node, ctrl));
	}

	void createLink(int from_node, int to_node, SigBit ctrl = State::S1) {
		assert(!materialized);
		assert(0 <= from_node && from_node < GetSize(nodes));
		assert(0 <= to_node && to_node < GetSize(nodes));
		assert(from_node != acceptNode);
		assert(from_node != condNode);
		assert(to_node != startNode);

		if (from_node != startNode)
			assert(nodes.at(from_node).is_cond_node == nodes.at(to_node).is_cond_node);

		if (throughout_sig != State::S1) {
			if (ctrl != State::S1)
				ctrl = module->And(NEW_ID, throughout_sig, ctrl);
			else
				ctrl = throughout_sig;
		}

		nodes[from_node].links.push_back(make_pair(to_node, ctrl));
	}

	void make_link_order(vector<int> &order, int node, int min) {
		order[node] = std::max(order[node], min);
		for (auto &it : nodes[node].links)
			make_link_order(order, it.first, order[node]+1);
	}

    // ----------------------------------------------------
	// Generating NFSM circuit to acquire accept signal

	SigBit getAccept()
	{
		assert(!materialized);
		materialized = true;

		vector<Wire*> state_wire(GetSize(nodes));
		vector<SigBit> state_sig(GetSize(nodes));
		vector<SigBit> next_state_sig(GetSize(nodes));

		// Create state signals

		{
			SigBit not_disable = State::S1;

			if (disable_sig != State::S0)
				not_disable = module->Not(NEW_ID, disable_sig);

			for (int i = 0; i < GetSize(nodes); i++)
			{
				Wire *w = module->addWire(NEW_ID);
				state_wire[i] = w;
				state_sig[i] = w;

				if (i == startNode)
					state_sig[i] = module->Or(NEW_ID, state_sig[i], trigger_sig);

				if (disable_sig != State::S0)
					state_sig[i] = module->And(NEW_ID, state_sig[i], not_disable);
			}
		}

		// Follow Links

		{
			vector<int> node_order(GetSize(nodes));
			vector<vector<int>> order_to_nodes;

			for (int i = 0; i < GetSize(nodes); i++)
				make_link_order(node_order, i, 0);

			for (int i = 0; i < GetSize(nodes); i++) {
				if (node_order[i] >= GetSize(order_to_nodes))
					order_to_nodes.resize(node_order[i]+1);
				order_to_nodes[node_order[i]].push_back(i);
			}

			for (int order = 0; order < GetSize(order_to_nodes); order++)
			for (int node : order_to_nodes[order])
			{
				for (auto &it : nodes[node].links)
				{
					int target = it.first;
					SigBit ctrl = state_sig[node];

					if (it.second != State::S1)
						ctrl = module->And(NEW_ID, ctrl, it.second);

					state_sig[target] = module->Or(NEW_ID, state_sig[target], ctrl);
				}
			}
		}

		// Construct activations

		{
			vector<SigSpec> activate_sig(GetSize(nodes));
			vector<SigBit> activate_bit(GetSize(nodes));

			for (int i = 0; i < GetSize(nodes); i++) {
				for (auto &it : nodes[i].edges)
					activate_sig[it.first].append(module->And(NEW_ID, state_sig[i], it.second));
			}

			for (int i = 0; i < GetSize(nodes); i++) {
				if (GetSize(activate_sig[i]) == 0)
					next_state_sig[i] = State::S0;
				else if (GetSize(activate_sig[i]) == 1)
					next_state_sig[i] = activate_sig[i];
				else
					next_state_sig[i] = module->ReduceOr(NEW_ID, activate_sig[i]);
			}
		}

		// Create state FFs

		for (int i = 0; i < GetSize(nodes); i++)
		{
			if (next_state_sig[i] != State::S0) {
				clocking.addDff(NEW_ID, next_state_sig[i], state_wire[i], State::S0);
			} else {
				module->connect(state_wire[i], State::S0);
			}
		}

		final_accept_sig = state_sig[acceptNode];
		return final_accept_sig;
	}

	// ----------------------------------------------------
	// Generating quantifier-based NFSM circuit to acquire reject signal

	SigBit getAnyAllRejectWorker(bool allMode) {
		// FIXME
		assert(allMode);
	}

	SigBit getAnyReject() {
		return getAnyAllRejectWorker(false);
	}

	SigBit getAllReject() {
		return getAnyAllRejectWorker(true);
	}

    // ----------------------------------------------------
	// Generating DFSM circuit to acquire reject signal

	void node_to_unode(int node, int unode, SigSpec ctrl) {
		if (node == acceptNode)
			unodes[unode].accept.push_back(ctrl);

		if (node == condNode)
			unodes[unode].cond.push_back(ctrl);

		for (auto &it : nodes[node].edges) {
			if (it.second != State::S1) {
				SigSpec s = {&ctrl, it.second};
				s.sort_and_unify();
				unodes[unode].edges.push_back(make_pair(it.first, s));
			} else {
				unodes[unode].edges.push_back(make_pair(it.first, ctrl));
			}
		}

		for (auto &it : nodes[node].links) {
			if (it.second != State::S1) {
				SigSpec s = {&ctrl, it.second};
				s.sort_and_unify();
				node_to_unode(it.first, unode, s);
			} else {
				node_to_unode(it.first, unode, ctrl);
			}
		}
	}
};

