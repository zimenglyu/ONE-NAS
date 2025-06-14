#include <algorithm>
using std::sort;
using std::upper_bound;

#include <cmath>
#include <fstream>
using std::ifstream;
using std::istream;
using std::ofstream;
using std::ostream;

#include <iomanip>
using std::setfill;
using std::setw;

#include <ios>
using std::hex;
using std::ios;

#include <iostream>
using std::cout;
using std::endl;

#include <random>
using std::minstd_rand0;
using std::uniform_real_distribution;

#include <thread>
using std::thread;

#include <random>
using std::minstd_rand0;
using std::uniform_int_distribution;
using std::uniform_real_distribution;

#include <sstream>
using std::istringstream;
using std::ostringstream;

#include <string>
using std::string;
using std::to_string;

#include <vector>
using std::vector;

#include <unordered_map>
using std::unordered_map;

#include <map>
using std::map;

#include "common/color_table.hxx"
#include "common/log.hxx"
#include "common/random.hxx"
#include "delta_node.hxx"
#include "dnas_node.hxx"
#include "enarc_node.hxx"
#include "enas_dag_node.hxx"
#include "generate_nn.hxx"
#include "gru_node.hxx"
#include "lstm_node.hxx"
#include "mgu_node.hxx"
#include "random_dag_node.hxx"
#include "rnn.hxx"
#include "rnn_genome.hxx"
#include "rnn_node.hxx"
#include "time_series/time_series.hxx"
#include "ugrnn_node.hxx"

vector<int32_t> dnas_node_types = {SIMPLE_NODE, UGRNN_NODE, MGU_NODE, GRU_NODE, DELTA_NODE, LSTM_NODE};

string parse_fitness(double fitness) {
    if (fitness == EXAMM_MAX_DOUBLE) {
        return "UNEVAL";
    } else {
        return to_string(fitness);
    }
}

void RNN_Genome::sort_nodes_by_depth() {
    sort(nodes.begin(), nodes.end(), sort_RNN_Nodes_by_depth());
}

void RNN_Genome::sort_edges_by_depth() {
    sort(edges.begin(), edges.end(), sort_RNN_Edges_by_depth());
}

void RNN_Genome::sort_recurrent_edges_by_depth() {
    sort(recurrent_edges.begin(), recurrent_edges.end(), sort_RNN_Recurrent_Edges_by_depth());
}

RNN_Genome::RNN_Genome(
    vector<RNN_Node_Interface*>& _nodes, vector<RNN_Edge*>& _edges, vector<RNN_Recurrent_Edge*>& _recurrent_edges
) {
    generation_id = -1;
    group_id = -1;

    best_validation_mse = EXAMM_MAX_DOUBLE;
    best_validation_mae = EXAMM_MAX_DOUBLE;

    nodes = _nodes;
    edges = _edges;
    recurrent_edges = _recurrent_edges;
    // weight_rules = _weight_rules->copy();

    sort_nodes_by_depth();
    sort_edges_by_depth();

    // set default values
    // bp_iterations = 20000;
    // learning_rate = 0.001;
    // adapt_learning_rate = false;
    // use_nesterov_momentum = false;
    // use_reset_weights = false;

    // use_high_norm = true;
    // high_threshold = 1.0;
    // use_low_norm = true;
    // low_threshold = 0.05;

    use_dropout = false;
    dropout_probability = 0.5;

    log_filename = "";

    int16_t seed = std::chrono::system_clock::now().time_since_epoch().count();
    generator = minstd_rand0(seed);
    rng = uniform_real_distribution<double>(-0.5, 0.5);
    rng_0_1 = uniform_real_distribution<double>(0.0, 1.0);
    rng_1_1 = uniform_real_distribution<double>(-1.0, 1.0);

    assign_reachability();
}

RNN_Genome::RNN_Genome(
    vector<RNN_Node_Interface*>& _nodes, vector<RNN_Edge*>& _edges, vector<RNN_Recurrent_Edge*>& _recurrent_edges,
    int16_t seed
)
    : RNN_Genome(_nodes, _edges, _recurrent_edges) {
    generator = minstd_rand0(seed);
}

void RNN_Genome::set_parameter_names(
    const vector<string>& _input_parameter_names, const vector<string>& _output_parameter_names
) {
    input_parameter_names = _input_parameter_names;
    output_parameter_names = _output_parameter_names;
}

RNN_Genome* RNN_Genome::copy() {
    // Log::info("Copying genome, generation_id: %d\n", generation_id);
    vector<RNN_Node_Interface*> node_copies;
    vector<RNN_Edge*> edge_copies;
    vector<RNN_Recurrent_Edge*> recurrent_edge_copies;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        node_copies.push_back(nodes[i]->copy());
    }

    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        edge_copies.push_back(edges[i]->copy(node_copies));
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        recurrent_edge_copies.push_back(recurrent_edges[i]->copy(node_copies));
    }

    RNN_Genome* other = new RNN_Genome(node_copies, edge_copies, recurrent_edge_copies);

    other->group_id = group_id;
    other->bp_iterations = bp_iterations;
    other->generation_id = generation_id;
    other->genome_type = genome_type;

    other->use_dropout = use_dropout;
    other->dropout_probability = dropout_probability;

    other->log_filename = log_filename;



    other->initial_parameters = initial_parameters;

    other->best_validation_mse = best_validation_mse;
    other->best_validation_mae = best_validation_mae;
    other->best_parameters = best_parameters;

    other->input_parameter_names = input_parameter_names;
    other->output_parameter_names = output_parameter_names;

    other->normalize_type = normalize_type;
    other->normalize_mins = normalize_mins;
    other->normalize_maxs = normalize_maxs;
    other->normalize_avgs = normalize_avgs;
    other->normalize_std_devs = normalize_std_devs;

    // reachability is assigned in the constructor
    // other->assign_reachability();

    return other;
}

RNN_Genome::~RNN_Genome() {
    RNN_Node_Interface* node;

    while (nodes.size() > 0) {
        node = nodes.back();
        nodes.pop_back();
        delete node;
    }

    RNN_Edge* edge;

    while (edges.size() > 0) {
        edge = edges.back();
        edges.pop_back();
        delete edge;
    }

    RNN_Recurrent_Edge* recurrent_edge;

    while (recurrent_edges.size() > 0) {
        recurrent_edge = recurrent_edges.back();
        recurrent_edges.pop_back();
        delete recurrent_edge;
    }

    // // Add this line to properly delete the weight_rules pointer
    // delete weight_rules;
}

string RNN_Genome::print_statistics_header() {
    ostringstream oss;

    oss << std::left << setw(12) << "MSE" << setw(12) << "MAE" << setw(12) << "Edges" << setw(12) << "Rec Edges"
        << setw(12) << "Simple" << setw(12) << "Jordan" << setw(12) << "Elman" << setw(12) << "UGRNN" << setw(12)
        << "MGU" << setw(12) << "GRU" << setw(12) << "Delta" << setw(12) << "LSTM" << setw(12) << "ENARC" << setw(12)
        << "ENAS_DAG" << setw(12) << "RANDOM_DAG" << setw(12) << "Total"
        << "Generated";

    return oss.str();
}

string RNN_Genome::print_statistics() {
    ostringstream oss;
    oss << std::left << setw(12) << parse_fitness(best_validation_mse) << setw(12) << parse_fitness(best_validation_mae)
        << setw(12) << get_edge_count_str(false) << setw(12) << get_edge_count_str(true) << setw(12)
        << get_node_count_str(SIMPLE_NODE) << setw(12) << get_node_count_str(JORDAN_NODE) << setw(12)
        << get_node_count_str(ELMAN_NODE) << setw(12) << get_node_count_str(UGRNN_NODE) << setw(12)
        << get_node_count_str(MGU_NODE) << setw(12) << get_node_count_str(GRU_NODE) << setw(12)
        << get_node_count_str(DELTA_NODE) << setw(12) << get_node_count_str(LSTM_NODE) << setw(12)
        << get_node_count_str(ENARC_NODE) << setw(12) << get_node_count_str(ENAS_DAG_NODE) << setw(12)
        << get_node_count_str(RANDOM_DAG_NODE) << setw(12) << get_node_count_str(-1);  //-1 does all nodes
    return oss.str();
}

double RNN_Genome::get_avg_recurrent_depth() const {
    int32_t count = 0;
    double average = 0.0;
    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->is_reachable()) {
            average += recurrent_edges[i]->get_recurrent_depth();
            count++;
        }
    }

    // in case there are no recurrent edges
    if (count == 0) {
        return 0;
    }

    return average / count;
}

string RNN_Genome::get_edge_count_str(bool recurrent) {
    ostringstream oss;
    if (recurrent) {
        oss << get_enabled_recurrent_edge_count() << " (" << recurrent_edges.size() << ")";
    } else {
        oss << get_enabled_edge_count() << " (" << edges.size() << ")";
    }
    return oss.str();
}

string RNN_Genome::get_node_count_str(int32_t node_type) {
    ostringstream oss;
    if (node_type < 0) {
        oss << get_enabled_node_count() << " (" << get_node_count() << ")";
    } else {
        int32_t enabled_nodes = get_enabled_node_count(node_type);
        int32_t total_nodes = get_node_count(node_type);

        if (total_nodes > 0) {
            oss << enabled_nodes << " (" << total_nodes << ")";
        }
    }
    return oss.str();
}

int32_t RNN_Genome::get_enabled_node_count() {
    int32_t count = 0;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->enabled) {
            count++;
        }
    }

    return count;
}

int32_t RNN_Genome::get_enabled_node_count(int32_t node_type) {
    int32_t count = 0;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->enabled && nodes[i]->layer_type == HIDDEN_LAYER && nodes[i]->node_type == node_type) {
            count++;
        }
    }

    return count;
}

int32_t RNN_Genome::get_node_count() {
    return (int32_t) nodes.size();
}

int32_t RNN_Genome::get_node_count(int32_t node_type) {
    int32_t count = 0;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->node_type == node_type) {
            count++;
        }
    }

    return count;
}




vector<string> RNN_Genome::get_input_parameter_names() const {
    return input_parameter_names;
}

vector<string> RNN_Genome::get_output_parameter_names() const {
    return output_parameter_names;
}

void RNN_Genome::set_normalize_bounds(
    string _normalize_type, const map<string, double>& _normalize_mins, const map<string, double>& _normalize_maxs,
    const map<string, double>& _normalize_avgs, const map<string, double>& _normalize_std_devs
) {
    normalize_type = _normalize_type;
    normalize_mins = _normalize_mins;
    normalize_maxs = _normalize_maxs;
    normalize_avgs = _normalize_avgs;
    normalize_std_devs = _normalize_std_devs;
}

string RNN_Genome::get_normalize_type() const {
    return normalize_type;
}

map<string, double> RNN_Genome::get_normalize_mins() const {
    return normalize_mins;
}

map<string, double> RNN_Genome::get_normalize_maxs() const {
    return normalize_maxs;
}

map<string, double> RNN_Genome::get_normalize_avgs() const {
    return normalize_avgs;
}

map<string, double> RNN_Genome::get_normalize_std_devs() const {
    return normalize_std_devs;
}

int32_t RNN_Genome::get_group_id() const {
    return group_id;
}

void RNN_Genome::set_group_id(int32_t _group_id) {
    group_id = _group_id;
}

int32_t RNN_Genome::get_enabled_edge_count() {
    int32_t count = 0;

    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->enabled) {
            count++;
        }
    }

    return count;
}

int32_t RNN_Genome::get_enabled_recurrent_edge_count() {
    int32_t count = 0;

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->enabled) {
            count++;
        }
    }

    return count;
}

void RNN_Genome::set_bp_iterations(int32_t _bp_iterations) {
    // if (epochs_acc_freq > 0) {
    //     if (generation_id < epochs_acc_freq) bp_iterations = 0;
    //     else {
    //         int32_t n = floor(generation_id/epochs_acc_freq) - 1;
    //         bp_iterations = (int32_t)pow(2, n);
    //     }

    //     Log::info("Setting bp interation %d to genome %d \n", bp_iterations, generation_id);
    // } else {
    bp_iterations = _bp_iterations;
    // }
}

int32_t RNN_Genome::get_bp_iterations() {
    return bp_iterations;
}

// void RNN_Genome::set_learning_rate(double _learning_rate) {
//     learning_rate = _learning_rate;
// }

// void RNN_Genome::disable_high_threshold() {
//     use_high_norm = false;
// }

// void RNN_Genome::enable_high_threshold(double _high_threshold) {
//     use_high_norm = true;
//     high_threshold = _high_threshold;
// }

// void RNN_Genome::disable_low_threshold() {
//     use_low_norm = false;
// }

// void RNN_Genome::enable_low_threshold(double _low_threshold) {
//     use_low_norm = true;
//     low_threshold = _low_threshold;
// }

void RNN_Genome::disable_dropout() {
    use_dropout = false;
    dropout_probability = 0;
}

void RNN_Genome::enable_dropout(double _dropout_probability) {
    dropout_probability = _dropout_probability;
}

void RNN_Genome::set_log_filename(string _log_filename) {
    log_filename = _log_filename;
}

void RNN_Genome::get_weights(vector<double>& parameters) {
    parameters.resize(get_number_weights());

    int32_t current = 0;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        nodes[i]->get_weights(current, parameters);
        // if (nodes[i]->is_reachable()) nodes[i]->get_weights(current, parameters);
    }

    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        parameters[current++] = edges[i]->weight;
        // if (edges[i]->is_reachable()) parameters[current++] = edges[i]->weight;
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        parameters[current++] = recurrent_edges[i]->weight;
        // if (recurrent_edges[i]->is_reachable()) parameters[current++] = recurrent_edges[i]->weight;
    }
}

void RNN_Genome::set_weights(const vector<double>& parameters) {
    if ((int32_t) parameters.size() != get_number_weights()) {
        Log::fatal(
            "ERROR! Trying to set weights where the RNN has %d weights, and the parameters vector has %d weights!\n",
            get_number_weights(), parameters.size()
        );
        exit(1);
    }

    int32_t current = 0;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        nodes[i]->set_weights(current, parameters);
        // if (nodes[i]->is_reachable()) nodes[i]->set_weights(current, parameters);
    }

    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        edges[i]->weight = bound(parameters[current++]);
        // if (edges[i]->is_reachable()) edges[i]->weight = parameters[current++];
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        recurrent_edges[i]->weight = bound(parameters[current++]);
        // if (recurrent_edges[i]->is_reachable()) recurrent_edges[i]->weight = parameters[current++];
    }
}

int32_t RNN_Genome::get_number_inputs() {
    int32_t number_inputs = 0;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->get_layer_type() == INPUT_LAYER) {
            number_inputs++;
        }
    }

    return number_inputs;
}

int32_t RNN_Genome::get_number_outputs() {
    int32_t number_outputs = 0;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->get_layer_type() == OUTPUT_LAYER) {
            number_outputs++;
        }
    }

    return number_outputs;
}

int32_t RNN_Genome::get_number_weights() {
    int32_t number_weights = 0;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        number_weights += nodes[i]->get_number_weights();
        // if (nodes[i]->is_reachable()) number_weights += nodes[i]->get_number_weights();
    }

    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        number_weights++;
        // if (edges[i]->is_reachable()) number_weights++;
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        number_weights++;
        // if (recurrent_edges[i]->is_reachable()) number_weights++;
    }

    return number_weights;
}

double RNN_Genome::get_avg_edge_weight() {
    double avg_weight;
    double weights = 0;
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->enabled) {
            if (edges[i]->weight > 10) {
                Log::error("ERROR: edge %d has weight %f \n", i, edges[i]->weight);
            }
            weights += edges[i]->weight;
        }
    }
    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->enabled) {
            if (recurrent_edges[i]->weight > 10) {
                Log::error("ERROR: recurrent edge %d has weight %f \n", i, recurrent_edges[i]->weight);
            }
            weights += recurrent_edges[i]->weight;
        }
    }
    int32_t N = edges.size() + recurrent_edges.size();
    avg_weight = weights / N;
    return avg_weight;
}

// WeightRules* RNN_Genome::get_weight_rules() const {
//     return weight_rules;
// }

void RNN_Genome::initialize_randomly(WeightRules* weight_rules) {
    Log::trace("initializing genome %d of group %d randomly!\n", generation_id, group_id);
    int32_t number_of_weights = get_number_weights();
    initial_parameters.assign(number_of_weights, 0.0);
    WeightType weight_initialize = weight_rules->get_weight_initialize_method();

    if (weight_initialize == WeightType::RANDOM) {
        for (int32_t i = 0; i < (int32_t) initial_parameters.size(); i++) {
            initial_parameters[i] = rng(generator);
        }
        this->set_weights(initial_parameters);
    } else if (weight_initialize == WeightType::XAVIER) {
        for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
            initialize_xavier(nodes[i]);
        }
        get_weights(initial_parameters);
    } else if (weight_initialize == WeightType::KAIMING) {
        for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
            initialize_kaiming(nodes[i]);
        }
        get_weights(initial_parameters);
    } else if (weight_initialize == WeightType::GP) {
        for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
            initialize_gp(nodes[i]);
        }
        get_weights(initial_parameters);
    } else {
        Log::fatal(
            "ERROR: trying to initialize a genome randomly with unknown weight initalization strategy: '%d'\n",
            weight_initialize
        );
        exit(1);
    }

    this->set_best_parameters(initial_parameters);
    this->set_weights(initial_parameters);
}

void RNN_Genome::initialize_xavier(RNN_Node_Interface* n) {
    vector<RNN_Edge*> input_edges;
    vector<RNN_Recurrent_Edge*> input_recurrent_edges;
    get_input_edges(n->innovation_number, input_edges, input_recurrent_edges);
    int32_t fan_in = (int32_t) (input_edges.size() + input_recurrent_edges.size());
    int32_t fan_out = get_fan_out(n->innovation_number);
    int32_t sum = fan_in + fan_out;
    if (sum <= 0) {
        sum = 1;
    }
    double range = sqrt(6) / sqrt(sum);
    for (int32_t j = 0; j < (int32_t) input_edges.size(); j++) {
        double edge_weight = range * rng_1_1(generator);
        input_edges[j]->weight = edge_weight;
    }
    for (int32_t j = 0; j < (int32_t) input_recurrent_edges.size(); j++) {
        double edge_weight = range * rng_1_1(generator);
        input_recurrent_edges[j]->weight = edge_weight;
    }
    n->initialize_xavier(generator, rng_1_1, range);
}

void RNN_Genome::initialize_kaiming(RNN_Node_Interface* n) {
    vector<RNN_Edge*> input_edges;
    vector<RNN_Recurrent_Edge*> input_recurrent_edges;
    get_input_edges(n->innovation_number, input_edges, input_recurrent_edges);
    int32_t fan_in = (int32_t) (input_edges.size() + input_recurrent_edges.size());

    if (fan_in <= 0) {
        fan_in = 1;
    }
    double range = sqrt(2) / sqrt(fan_in);
    for (int32_t j = 0; j < (int32_t) input_edges.size(); j++) {
        double edge_weight = range * normal_distribution.random(generator, 0, 1);
        input_edges[j]->weight = edge_weight;
    }
    for (int32_t j = 0; j < (int32_t) input_recurrent_edges.size(); j++) {
        double edge_weight = range * normal_distribution.random(generator, 0, 1);
        input_recurrent_edges[j]->weight = edge_weight;
    }
    n->initialize_kaiming(generator, normal_distribution, range);
}

void RNN_Genome::initialize_gp(RNN_Node_Interface* n) {
    vector<RNN_Edge*> input_edges;
    vector<RNN_Recurrent_Edge*> input_recurrent_edges;
    get_input_edges(n->innovation_number, input_edges, input_recurrent_edges);

    // this assumes a single output
    if (n->get_layer_type() == OUTPUT_LAYER) {
        for (int32_t j = 0; j < (int32_t) input_edges.size(); j++) {
            if (input_edges[j]->input_node->get_layer_type() == INPUT_LAYER) {
                string output_parameter_name = n->parameter_name;
                string input_parameter_name = input_edges[j]->input_node->parameter_name;
                if (output_parameter_name.compare(input_parameter_name) == 0) {
                    input_edges[j]->weight = 1.0;
                } else {
                    input_edges[j]->weight = 0.0;
                }
            }
        }
        for (int32_t j = 0; j < (int32_t) input_recurrent_edges.size(); j++) {
            input_recurrent_edges[j]->weight = 0.0;
        }
    } else {
        for (int32_t j = 0; j < (int32_t) input_edges.size(); j++) {
            input_edges[j]->weight = 1.0;
        }
        for (int32_t j = 0; j < (int32_t) input_recurrent_edges.size(); j++) {
            input_recurrent_edges[j]->weight = 1.0;
        }
    }
}

double RNN_Genome::get_xavier_weight(RNN_Node_Interface* output_node) {
    int32_t sum = get_fan_in(output_node->innovation_number) + get_fan_out(output_node->innovation_number);
    if (sum <= 0) {
        sum = 1;
    }
    double range = sqrt(6) / sqrt(sum);
    // double range = sqrt(6)/sqrt(output_node->fan_in + output_node->fan_out);
    return range * (rng_1_1(generator));
}

double RNN_Genome::get_kaiming_weight(RNN_Node_Interface* output_node) {
    int32_t fan_in = get_fan_in(output_node->innovation_number);
    if (fan_in <= 0) {
        fan_in = 1;
    }
    double range = sqrt(2) / sqrt(fan_in);
    // double range = sqrt(6)/sqrt(output_node->fan_in + output_node->fan_out);
    return range * normal_distribution.random(generator, 0, 1);
}

double RNN_Genome::get_random_weight() {
    return rng(generator);
}

void RNN_Genome::initialize_node_randomly(RNN_Node_Interface* n, WeightRules* weight_rules) {
    WeightType mutated_component_weight = weight_rules->get_mutated_components_weight_method();
    WeightType weight_initialize = weight_rules->get_weight_initialize_method();
    if (mutated_component_weight == weight_initialize) {
        if (weight_initialize == WeightType::XAVIER) {
            int32_t sum = get_fan_in(n->innovation_number) + get_fan_out(n->innovation_number);
            if (sum <= 0) {
                sum = 1;
            }
            double range = sqrt(6) / sqrt(sum);
            // double range = sqrt(6)/sqrt(n->fan_in + n->fan_out);
            n->initialize_xavier(generator, rng_1_1, range);
        } else if (weight_initialize == WeightType::KAIMING) {
            int32_t fan_in = get_fan_in(n->innovation_number);
            if (fan_in <= 0) {
                fan_in = 1;
            }
            double range = sqrt(2) / sqrt(fan_in);
            n->initialize_kaiming(generator, normal_distribution, range);
        } else if (weight_initialize == WeightType::RANDOM) {
            // random weight
            n->initialize_uniform_random(generator, rng);
        } else if (weight_initialize == WeightType::GP) {
            if (n->node_type == MULTIPLY_NODE_GP || n->node_type == SUM_NODE_GP) {
                int32_t sum = get_fan_in(n->innovation_number) + get_fan_out(n->innovation_number);
                if (sum <= 0) {
                    sum = 1;
                }
                double range = sqrt(6) / sqrt(sum);
                // double range = sqrt(6)/sqrt(n->fan_in + n->fan_out);
                n->initialize_xavier(generator, rng_1_1, range);
            }
        } else {
            Log::fatal("weight initialize method %d is not set correctly \n", weight_initialize);
            exit(1);
        }
    }
}

void RNN_Genome::get_input_edges(
    int32_t node_innovation, vector<RNN_Edge*>& input_edges, vector<RNN_Recurrent_Edge*>& input_recurrent_edges
) {
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->enabled) {
            if (edges[i]->output_node->innovation_number == node_innovation) {
                input_edges.push_back(edges[i]);
            }
        }
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->enabled) {
            if (recurrent_edges[i]->output_node->innovation_number == node_innovation) {
                input_recurrent_edges.push_back(recurrent_edges[i]);
            }
        }
    }
}

int32_t RNN_Genome::get_fan_in(int32_t node_innovation) {
    int32_t fan_in = 0;
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->enabled) {
            if (edges[i]->output_node->innovation_number == node_innovation) {
                fan_in++;
            }
        }
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->enabled) {
            if (recurrent_edges[i]->output_node->innovation_number == node_innovation) {
                fan_in++;
            }
        }
    }
    return fan_in;
}

int32_t RNN_Genome::get_fan_out(int32_t node_innovation) {
    int32_t fan_out = 0;
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->enabled) {
            if (edges[i]->input_node->innovation_number == node_innovation) {
                fan_out++;
            }
        }
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->enabled) {
            if (recurrent_edges[i]->input_node->innovation_number == node_innovation) {
                fan_out++;
            }
        }
    }
    return fan_out;
}

RNN* RNN_Genome::get_rnn() {
    vector<RNN_Node_Interface*> node_copies;
    vector<RNN_Edge*> edge_copies;
    vector<RNN_Recurrent_Edge*> recurrent_edge_copies;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        node_copies.push_back(nodes[i]->copy());
        // if (nodes[i]->layer_type == INPUT_LAYER || nodes[i]->layer_type == OUTPUT_LAYER || nodes[i]->is_reachable())
        // node_copies.push_back( nodes[i]->copy() );
    }

    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        edge_copies.push_back(edges[i]->copy(node_copies));
        // if (edges[i]->is_reachable()) edge_copies.push_back( edges[i]->copy(node_copies) );
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        recurrent_edge_copies.push_back(recurrent_edges[i]->copy(node_copies));
        // if (recurrent_edges[i]->is_reachable()) recurrent_edge_copies.push_back(
        // recurrent_edges[i]->copy(node_copies) );
    }

    return new RNN(node_copies, edge_copies, recurrent_edge_copies, input_parameter_names, output_parameter_names);
}

vector<double> RNN_Genome::get_best_parameters() const {
    return best_parameters;
}

// INFO: ADDED BY ABDELRAHMAN TO USE FOR TRANSFER LEARNING
void RNN_Genome::set_best_parameters(vector<double> parameters) {
    best_parameters = parameters;
}

vector<double> RNN_Genome::get_initial_parameters() const {
    return initial_parameters;
}

// INFO: ADDED BY ABDELRAHMAN TO USE FOR TRANSFER LEARNING
void RNN_Genome::set_initial_parameters(vector<double> parameters) {
    initial_parameters = parameters;
}

int32_t RNN_Genome::get_generation_id() const {
    return generation_id;
}

void RNN_Genome::set_generation_id(int32_t _generation_id) {
    generation_id = _generation_id;
}

double RNN_Genome::get_fitness() const {
    return best_validation_mse;
    // return best_validation_mae;
}

double RNN_Genome::get_best_validation_mse() const {
    return best_validation_mse;
}

double RNN_Genome::get_best_validation_mae() const {
    return best_validation_mae;
}



bool RNN_Genome::sanity_check() {
    return true;
}

void forward_pass_thread_regression(
    RNN* rnn, const vector<double>& parameters, const vector<vector<double> >& inputs,
    const vector<vector<double> >& outputs, int32_t i, double* mses, bool use_dropout, bool training,
    double dropout_probability
) {
    rnn->set_weights(parameters);
    rnn->forward_pass(inputs, use_dropout, training, dropout_probability);
    mses[i] = rnn->calculate_error_mse(outputs);
    // mses[i] = rnn->calculate_error_mae(outputs);

    Log::trace("mse[%d]: %lf\n", i, mses[i]);
}

void forward_pass_thread_classification(
    RNN* rnn, const vector<double>& parameters, const vector<vector<double> >& inputs,
    const vector<vector<double> >& outputs, int32_t i, double* mses, bool use_dropout, bool training,
    double dropout_probability
) {
    rnn->set_weights(parameters);
    rnn->forward_pass(inputs, use_dropout, training, dropout_probability);
    mses[i] = rnn->calculate_error_softmax(outputs);
    // mses[i] = rnn->calculate_error_mae(outputs);

    Log::trace("mse[%d]: %lf\n", i, mses[i]);
}

void RNN_Genome::get_analytic_gradient(
    vector<RNN*>& rnns, const vector<double>& parameters, const vector<vector<vector<double> > >& inputs,
    const vector<vector<vector<double> > >& outputs, double& mse, vector<double>& analytic_gradient, bool training
) {
    double* mses = new double[rnns.size()];
    double mse_sum = 0.0;
    vector<thread> threads;
    for (int32_t i = 0; i < (int32_t) rnns.size(); i++) {
        threads.push_back(thread(
            forward_pass_thread_regression, rnns[i], parameters, inputs[i], outputs[i], i, mses, use_dropout, training,
            dropout_probability
        ));
    }

    for (int32_t i = 0; i < (int32_t) rnns.size(); i++) {
        threads[i].join();
        mse_sum += mses[i];
    }
    delete[] mses;

    for (int32_t i = 0; i < (int32_t) rnns.size(); i++) {
        double d_mse = 0.0;
        d_mse = mse_sum * (1.0 / outputs[i][0].size()) * 2.0;
        rnns[i]->backward_pass(d_mse, use_dropout, training, dropout_probability);
    }

    mse = mse_sum;

    vector<double> current_gradients;
    analytic_gradient.assign(parameters.size(), 0.0);
    for (int32_t k = 0; k < (int32_t) rnns.size(); k++) {
        int32_t current = 0;
        for (int32_t i = 0; i < rnns[k]->get_number_nodes(); i++) {
            rnns[k]->get_node(i)->get_gradients(current_gradients);

            for (int32_t j = 0; j < (int32_t) current_gradients.size(); j++) {
                analytic_gradient[current] += current_gradients[j];
                current++;
            }
        }

        for (int32_t i = 0; i < rnns[k]->get_number_edges(); i++) {
            analytic_gradient[current] += rnns[k]->get_edge(i)->get_gradient();
            current++;
        }
    }
}

void RNN_Genome::backpropagate(
    const vector<vector<vector<double> > >& inputs, const vector<vector<vector<double> > >& outputs,
    const vector<vector<vector<double> > >& validation_inputs,
    const vector<vector<vector<double> > >& validation_outputs, WeightUpdate* weight_update_method
) {
    // double learning_rate = weight_update_method->get_learning_rate() / inputs.size();
    // double low_threshold = sqrt(weight_update_method->get_low_threshold() * inputs.size());
    // double high_threshold = sqrt(weight_update_method->get_high_threshold() * inputs.size());

    int32_t n_series = (int32_t) inputs.size();
    vector<RNN*> rnns;
    for (int32_t i = 0; i < n_series; i++) {
        rnns.push_back(this->get_rnn());
    }
    int32_t n_parameters = this->get_number_weights();
    vector<double> parameters = initial_parameters;
    vector<double> velocity(n_parameters, 0.0);
    vector<double> prev_velocity(n_parameters, 0.0);
    vector<double> analytic_gradient;
    vector<double> prev_gradient(n_parameters, 0.0);

    double mse;
    double norm = 0.0;

    // initialize the initial previous values
    get_analytic_gradient(rnns, parameters, inputs, outputs, mse, analytic_gradient, true);
    double validation_mse = get_mse(parameters, validation_inputs, validation_outputs);
    best_validation_mse = validation_mse;
    best_validation_mae = get_mae(parameters, validation_inputs, validation_outputs);
    best_parameters = parameters;

    norm = weight_update_method->get_norm(analytic_gradient);

    ofstream* output_log = create_log_file();

    for (int32_t iteration = 0; iteration < bp_iterations; iteration++) {
        prev_gradient = analytic_gradient;
        get_analytic_gradient(rnns, parameters, inputs, outputs, mse, analytic_gradient, true);
        this->set_weights(parameters);
        validation_mse = get_mse(parameters, validation_inputs, validation_outputs);
        if (validation_mse < best_validation_mse) {
            best_validation_mse = validation_mse;
            best_validation_mae = get_mae(parameters, validation_inputs, validation_outputs);
            best_parameters = parameters;
        }
        norm = weight_update_method->get_norm(analytic_gradient);
        if (output_log != NULL) {
            (*output_log) << iteration << " " << mse << " " << validation_mse << " " << best_validation_mse << endl;
        }
        weight_update_method->norm_gradients(analytic_gradient, norm);
        weight_update_method->update_weights(parameters, velocity, prev_velocity, analytic_gradient, iteration);
        Log::info(
            "iteration %10d, mse: %10lf, v_mse: %10lf, bv_mse: %10lf, norm: %lf", iteration, mse, validation_mse,
            best_validation_mse, norm
        );
        Log::info_no_header("\n");
    }

    RNN* g;
    while (rnns.size() > 0) {
        g = rnns.back();
        rnns.pop_back();
        delete g;
    }
    this->set_weights(best_parameters);
}

void RNN_Genome::backpropagate_stochastic(
    const vector<vector<vector<double> > >& inputs, const vector<vector<vector<double> > >& outputs,
    const vector<vector<vector<double> > >& validation_inputs,
    const vector<vector<vector<double> > >& validation_outputs, WeightUpdate* weight_update_method
) {
    int32_t n_parameters = this->get_number_weights();
    int32_t n_series = (int32_t) inputs.size();

    vector<double> parameters = initial_parameters;
    vector<double> velocity(n_parameters, 0.0);
    vector<double> prev_velocity(n_parameters, 0.0);
    vector<double> analytic_gradient;
    vector<double> prev_gradient(n_parameters, 0.0);

    double mse;
    double norm = 0.0;
    RNN* rnn = get_rnn();
    rnn->set_weights(parameters);

    std::chrono::time_point<std::chrono::system_clock> startClock = std::chrono::system_clock::now();

    // initialize the initial previous values
    for (int32_t i = 0; i < n_series; i++) {
        Log::trace(
            "getting analytic gradient for input/output: %d, n_series: %d, parameters.size: %d, inputs.size(): %d, "
            "outputs.size(): %d, log filename: '%s'\n",
            i, n_series, parameters.size(), inputs.size(), outputs.size(), log_filename.c_str()
        );
        rnn->get_analytic_gradient(
            parameters, inputs[i], outputs[i], mse, analytic_gradient, use_dropout, true, dropout_probability
        );
        Log::trace("got analytic gradient.\n");
        norm = weight_update_method->get_norm(analytic_gradient);
    }
    Log::trace("initialized previous values.\n");

    // TODO: need to get validation mse on the RNN not the genome
    double validation_mse = get_mse(parameters, validation_inputs, validation_outputs);
    best_validation_mse = validation_mse;
    best_validation_mae = get_mae(parameters, validation_inputs, validation_outputs);
    best_parameters = parameters;

    Log::trace("got initial mses.\n");
    Log::info("initial validation_mse: %lf, best validation mse: %lf\n", validation_mse, best_validation_mse);

    for (int32_t i = 0; i < (int32_t) parameters.size(); i++) {
        Log::trace("parameters[%d]: %lf\n", i, parameters[i]);
    }

    ofstream* output_log = create_log_file();

    for (int32_t iteration = 0; iteration < bp_iterations; iteration++) {
        vector<int32_t> shuffle_order;
        for (int32_t i = 0; i < n_series; i++) {
            shuffle_order.push_back(i);
        }
        fisher_yates_shuffle(generator, shuffle_order);
        double avg_norm = 0.0;
        for (int32_t k = 0; k < (int32_t) shuffle_order.size(); k++) {
            int32_t random_selection = shuffle_order[k];
            prev_gradient = analytic_gradient;
            rnn->get_analytic_gradient(
                parameters, inputs[random_selection], outputs[random_selection], mse, analytic_gradient, use_dropout,
                true, dropout_probability
            );

            norm = weight_update_method->get_norm(analytic_gradient);

            if (isnan(norm) || isinf(norm)) {
                // This genome is getting NANs for gradients so it is a
                // genetic dead end, delete it.
                // TODO: figure out why and maybe use clipping or another
                // method to handle it.
                delete rnn;
                best_parameters = parameters;
                this->best_validation_mse = NAN;
                this->best_validation_mae = NAN;
                return;
            }

            avg_norm += norm;
            weight_update_method->norm_gradients(analytic_gradient, norm);
            weight_update_method->update_weights(parameters, velocity, prev_velocity, analytic_gradient, iteration);
        }
        this->set_weights(parameters);
        double training_mse = get_mse(parameters, inputs, outputs);
        validation_mse = get_mse(parameters, validation_inputs, validation_outputs);

        if (validation_mse < best_validation_mse) {
            best_validation_mse = validation_mse;
            best_validation_mae = get_mae(parameters, validation_inputs, validation_outputs);
            best_parameters = parameters;
        }
        if (output_log != NULL) {
            std::chrono::time_point<std::chrono::system_clock> currentClock = std::chrono::system_clock::now();
            long milliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(currentClock - startClock).count();
            update_log_file(output_log, iteration, milliseconds, training_mse, validation_mse, avg_norm);
        }
        Log::info(
            "iteration %4d, mse: %5.10lf, v_mse: %5.10lf, bv_mse: %5.10lf, avg_norm: %5.10lf\n", iteration,
            training_mse, validation_mse, best_validation_mse, avg_norm
        );
    }
    delete rnn;
    this->set_weights(best_parameters);
    Log::info("backpropagation completed, getting mu/sigma\n");
    double _mu, _sigma;
    get_mu_sigma(best_parameters, _mu, _sigma);
}

ofstream* RNN_Genome::create_log_file() {
    ofstream* output_log = NULL;
    if (log_filename != "") {
        Log::trace("creating new log stream for '%s'\n", log_filename.c_str());
        output_log = new ofstream(log_filename);
        Log::trace("testing to see if log file is valid.\n");

        if (!output_log->is_open()) {
            Log::fatal("ERROR, could not open output log: '%s'\n", log_filename.c_str());
            exit(1);
        }
        Log::trace("opened log file '%s'\n", log_filename.c_str());

        (*output_log) << "Total BP Epochs, Time, Train MSE, Val. MSE, BEST Val. MSE, BEST Val. MAE, norm";
        (*output_log) << endl;
    }
    return output_log;
}

void RNN_Genome::update_log_file(
    ofstream* output_log, int32_t iteration, long milliseconds, double training_mse, double validation_mse,
    double avg_norm
) {
    // make sure the output log is good
    if (!output_log->good()) {
        output_log->close();
        delete output_log;
        output_log = new ofstream(log_filename, std::ios_base::app);
        Log::trace("testing to see if log file valid for '%s'\n", log_filename.c_str());
        if (!output_log->is_open()) {
            Log::fatal("ERROR, could not open output log: '%s'\n", log_filename.c_str());
            exit(1);
        }
    }
    (*output_log) << iteration << "," << milliseconds << "," << training_mse << "," << validation_mse << ","
                  << best_validation_mse << "," << best_validation_mae << "," << avg_norm << endl;
}

double RNN_Genome::get_softmax(
    const vector<double>& parameters, const vector<vector<vector<double> > >& inputs,
    const vector<vector<vector<double> > >& outputs
) {
    RNN* rnn = get_rnn();
    rnn->set_weights(parameters);

    double softmax = 0.0;
    double avg_softmax = 0.0;

    for (int32_t i = 0; i < (int32_t) inputs.size(); i++) {
        softmax = rnn->prediction_softmax(inputs[i], outputs[i], use_dropout, false, dropout_probability);

        avg_softmax += softmax;

        Log::trace("series[%5d]: Softmax: %5.10lf\n", i, softmax);
    }

    delete rnn;

    avg_softmax /= inputs.size();
    Log::trace("average Softmax: %5.10lf\n", avg_softmax);
    return avg_softmax;
}

double RNN_Genome::get_mse(
    const vector<double>& parameters, const vector<vector<vector<double> > >& inputs,
    const vector<vector<vector<double> > >& outputs
) {
    RNN* rnn = get_rnn();
    rnn->set_weights(parameters);

    double mse = 0.0;
    double avg_mse = 0.0;

    for (int32_t i = 0; i < (int32_t) inputs.size(); i++) {
        mse = rnn->prediction_mse(inputs[i], outputs[i], use_dropout, false, dropout_probability);

        avg_mse += mse;

        Log::trace("series[%5d]: MSE: %5.10lf\n", i, mse);
    }

    delete rnn;

    avg_mse /= inputs.size();
    Log::trace("average MSE: %5.10lf\n", avg_mse);
    return avg_mse;
}

double RNN_Genome::get_mae(
    const vector<double>& parameters, const vector<vector<vector<double> > >& inputs,
    const vector<vector<vector<double> > >& outputs
) {
    RNN* rnn = get_rnn();
    rnn->set_weights(parameters);

    double mae;
    double avg_mae = 0.0;

    for (int32_t i = 0; i < (int32_t) inputs.size(); i++) {
        mae = rnn->prediction_mae(inputs[i], outputs[i], use_dropout, false, dropout_probability);

        avg_mae += mae;

        Log::debug("series[%5d] MAE: %5.10lf\n", i, mae);
    }

    delete rnn;

    avg_mae /= inputs.size();
    Log::debug("average MAE: %5.10lf\n", avg_mae);
    return avg_mae;
}

// vector<vector<double> > RNN_Genome::get_predictions(
//     const vector<double>& parameters, const vector<vector<vector<double> > >& inputs,
//     const vector<vector<vector<double> > >& outputs
// ) {
//     RNN* rnn = get_rnn();
//     rnn->set_weights(parameters);

//     vector<vector<double> > all_results;

//     // one input vector per testing file
//     for (int32_t i = 0; i < (int32_t) inputs.size(); i++) {ß
//         all_results.push_back(rnn->get_predictions(inputs[i], outputs[i], use_dropout, dropout_probability));
//     }

//     delete rnn;

//     return all_results;
// }

vector< vector< vector<double> > > RNN_Genome::get_predictions(const vector<double> &parameters, const vector< vector< vector<double> > > &inputs, const vector< vector< vector<double> > > &outputs) {
    RNN *rnn = get_rnn();
    if (parameters.size() == 0) rnn->set_weights(initial_parameters);
    else rnn->set_weights(parameters);

    vector< vector< vector<double> > > all_results;

    //one input vector per testing file
    for (int32_t i = 0; i < (int32_t)inputs.size(); i++) {
        all_results.push_back(rnn->get_predictions(inputs[i], outputs[i], use_dropout, dropout_probability));
    }

    delete rnn;

    return all_results;
}

void RNN_Genome::write_predictions(
    string output_directory, const vector<string>& input_filenames, const vector<double>& parameters,
    const vector<vector<vector<double> > >& inputs, const vector<vector<vector<double> > >& outputs,
    TimeSeriesSets* time_series_sets
) {
    RNN* rnn = get_rnn();
    rnn->set_weights(parameters);

    for (int32_t i = 0; i < (int32_t) inputs.size(); i++) {
        string filename = input_filenames[i];
        Log::info("input filename[%5d]: '%s'\n", i, filename.c_str());

        int32_t last_dot_pos = filename.find_last_of(".");
        string extension = filename.substr(last_dot_pos);
        string prefix = filename.substr(0, last_dot_pos);

        string output_filename = prefix + "_predictions" + extension;
        output_filename = output_directory + "/" + output_filename.substr(output_filename.find_last_of("/") + 1);

        Log::info("output filename: '%s'\n", output_filename.c_str());

        rnn->write_predictions(
            output_filename, input_parameter_names, output_parameter_names, inputs[i], outputs[i], time_series_sets,
            use_dropout, dropout_probability
        );
    }

    delete rnn;
}

// void RNN_Genome::write_predictions(string output_directory, const vector<string> &input_filenames, const
// vector<double> &parameters, const vector< vector< vector<double> > > &inputs, const vector< vector< vector<double> >
// > &outputs, Corpus *word_series_sets) {
//     RNN *rnn = get_rnn();
//     rnn->set_weights(parameters);

//     vector< vector<double> > all_results;

//     //one input vector per testing file
//     for (int32_t i = 0; i < inputs.size(); i++) {
//         string filename = input_filenames[i];
//         Log::info("input filename[%5d]: '%s'\n", i, filename.c_str());

//         int32_t last_dot_pos = filename.find_last_of(".");
//         string extension = filename.substr(last_dot_pos);
//         string prefix = filename.substr(0, last_dot_pos);

//         string output_filename = prefix + "_predictions" + extension;
//         output_filename = output_directory + "/" + output_filename.substr(output_filename.find_last_of("/") + 1);

//         Log::info("output filename: '%s'\n", output_filename.c_str());

//         rnn->write_predictions(output_filename, input_parameter_names, output_parameter_names, inputs[i], outputs[i],
//         word_series_sets, use_dropout, dropout_probability);
//     }

//     delete rnn;
// }

void RNN_Genome::evaluate_online(const vector< vector< vector<double> > > &inputs, const vector< vector< vector<double> > > &output) {

    if (best_parameters.size() > 0) {
        best_validation_mse = get_mse(best_parameters, inputs, output);
    } else {
        // Log::error("initial parameter size %d\n", initial_parameters.size());
        best_validation_mse = get_mse(initial_parameters, inputs, output);
    }   
}

void RNN_Genome::set_genome_type(int32_t type) {
    genome_type = type;
}

int32_t RNN_Genome::get_genome_type() {
    return genome_type;
}

bool RNN_Genome::has_node_with_innovation(int32_t innovation_number) const {
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->get_innovation_number() == innovation_number) {
            return true;
        }
    }

    return false;
}

bool RNN_Genome::equals(RNN_Genome* other) {
    if (nodes.size() != other->nodes.size()) {
        return false;
    }
    if (edges.size() != other->edges.size()) {
        return false;
    }
    if (recurrent_edges.size() != other->recurrent_edges.size()) {
        return false;
    }

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (!nodes[i]->equals(other->nodes[i])) {
            return false;
        }
    }

    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (!edges[i]->equals(other->edges[i])) {
            return false;
        }
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (!recurrent_edges[i]->equals(other->recurrent_edges[i])) {
            return false;
        }
    }

    return true;
}

void RNN_Genome::assign_reachability() {
    Log::trace("assigning reachability!\n");
    Log::trace("%6d nodes, %6d edges, %6d recurrent edges\n", nodes.size(), edges.size(), recurrent_edges.size());

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        nodes[i]->forward_reachable = false;
        nodes[i]->backward_reachable = false;
        nodes[i]->total_inputs = 0;
        nodes[i]->total_outputs = 0;

        // set enabled input nodes as reachable
        if (nodes[i]->layer_type == INPUT_LAYER && nodes[i]->enabled) {
            nodes[i]->forward_reachable = true;
            nodes[i]->total_inputs = 1;

            Log::trace("\tsetting input node[%5d] reachable\n", i);
        }

        if (nodes[i]->layer_type == OUTPUT_LAYER) {
            nodes[i]->backward_reachable = true;
            nodes[i]->total_outputs = 1;
        }
    }

    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        edges[i]->forward_reachable = false;
        edges[i]->backward_reachable = false;
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        recurrent_edges[i]->forward_reachable = false;
        recurrent_edges[i]->backward_reachable = false;
    }

    // do forward reachability
    vector<RNN_Node_Interface*> nodes_to_visit;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->layer_type == INPUT_LAYER && nodes[i]->enabled) {
            nodes_to_visit.push_back(nodes[i]);
        }
    }

    while (nodes_to_visit.size() > 0) {
        RNN_Node_Interface* current = nodes_to_visit.back();
        nodes_to_visit.pop_back();

        // if the node is not enabled, we don't need to do anything
        if (!current->enabled) {
            continue;
        }

        for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
            if (edges[i]->input_innovation_number == current->innovation_number && edges[i]->enabled) {
                // this is an edge coming out of this node

                if (edges[i]->output_node->enabled) {
                    edges[i]->forward_reachable = true;

                    if (edges[i]->output_node->forward_reachable == false) {
                        if (edges[i]->output_node->innovation_number == edges[i]->input_node->innovation_number) {
                            Log::fatal("ERROR, forward edge was circular -- this should never happen");
                            exit(1);
                        }
                        edges[i]->output_node->forward_reachable = true;
                        nodes_to_visit.push_back(edges[i]->output_node);
                    }
                }
            }
        }

        for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
            if (recurrent_edges[i]->forward_reachable) {
                continue;
            }

            if (recurrent_edges[i]->input_innovation_number == current->innovation_number
                && recurrent_edges[i]->enabled) {
                // this is an recurrent_edge coming out of this node

                if (recurrent_edges[i]->output_node->enabled) {
                    recurrent_edges[i]->forward_reachable = true;

                    if (recurrent_edges[i]->output_node->forward_reachable == false) {
                        recurrent_edges[i]->output_node->forward_reachable = true;

                        // handle the edge case when a recurrent edge loops back on itself
                        nodes_to_visit.push_back(recurrent_edges[i]->output_node);
                    }
                }
            }
        }
    }

    // do backward reachability
    nodes_to_visit.clear();
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->layer_type == OUTPUT_LAYER && nodes[i]->enabled) {
            nodes_to_visit.push_back(nodes[i]);
        }
    }

    while (nodes_to_visit.size() > 0) {
        RNN_Node_Interface* current = nodes_to_visit.back();
        nodes_to_visit.pop_back();

        // if the node is not enabled, we don't need to do anything
        if (!current->enabled) {
            continue;
        }

        for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
            if (edges[i]->output_innovation_number == current->innovation_number && edges[i]->enabled) {
                // this is an edge coming out of this node

                if (edges[i]->input_node->enabled) {
                    edges[i]->backward_reachable = true;
                    if (edges[i]->input_node->backward_reachable == false) {
                        edges[i]->input_node->backward_reachable = true;
                        nodes_to_visit.push_back(edges[i]->input_node);
                    }
                }
            }
        }

        for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
            if (recurrent_edges[i]->output_innovation_number == current->innovation_number
                && recurrent_edges[i]->enabled) {
                // this is an recurrent_edge coming out of this node

                if (recurrent_edges[i]->input_node->enabled) {
                    recurrent_edges[i]->backward_reachable = true;
                    if (recurrent_edges[i]->input_node->backward_reachable == false) {
                        recurrent_edges[i]->input_node->backward_reachable = true;
                        nodes_to_visit.push_back(recurrent_edges[i]->input_node);
                    }
                }
            }
        }
    }

    // set inputs/outputs
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->is_reachable()) {
            edges[i]->input_node->total_outputs++;
            edges[i]->output_node->total_inputs++;
        }
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->is_reachable()) {
            recurrent_edges[i]->input_node->total_outputs++;
            recurrent_edges[i]->output_node->total_inputs++;
        }
    }

    if (Log::at_level(Log::TRACE)) {
        Log::trace("node reachabiltity:\n");
        for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
            RNN_Node_Interface* n = nodes[i];
            Log::trace(
                "node %5d, e: %d, fr: %d, br: %d, ti: %5d, to: %5d\n", n->innovation_number, n->enabled,
                n->forward_reachable, n->backward_reachable, n->total_inputs, n->total_outputs
            );
        }

        Log::trace("edge reachabiltity:\n");
        for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
            RNN_Edge* e = edges[i];
            Log::trace(
                "edge %5d, e: %d, fr: %d, br: %d\n", e->innovation_number, e->enabled, e->forward_reachable,
                e->backward_reachable
            );
        }

        Log::trace("recurrent edge reachabiltity:\n");
        for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
            RNN_Recurrent_Edge* e = recurrent_edges[i];
            Log::trace(
                "recurrent edge %5d, e: %d, fr: %d, br: %d\n", e->innovation_number, e->enabled, e->forward_reachable,
                e->backward_reachable
            );
        }
    }

    // calculate structural hash
    long node_hash = 0;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->is_reachable() && nodes[i]->is_enabled()) {
            node_hash += nodes[i]->get_innovation_number();
        }
    }

    long edge_hash = 0;
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->is_reachable() && edges[i]->is_enabled()) {
            edge_hash += edges[i]->get_innovation_number();
        }
    }

    long recurrent_edge_hash = 0;
    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->is_reachable() && recurrent_edges[i]->is_enabled()) {
            recurrent_edge_hash += recurrent_edges[i]->get_innovation_number();
        }
    }

    structural_hash = to_string(node_hash) + "_" + to_string(edge_hash) + "_" + to_string(recurrent_edge_hash);
    // Log::info("genome had structural hash: '%s'\n", structural_hash.c_str());
}

bool RNN_Genome::outputs_unreachable() {
    assign_reachability();

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->layer_type == OUTPUT_LAYER && !nodes[i]->is_reachable()) {
            return true;
        }
    }

    return false;
}

void RNN_Genome::get_mu_sigma(const vector<double>& p, double& mu, double& sigma) {
    if (p.size() == 0) {
        mu = 0.0;
        sigma = 0.25;
        Log::debug("\tmu: %lf, sigma: %lf, parameters.size() == 0\n", mu, sigma);
        return;
    }

    mu = 0.0;
    sigma = 0.0;

    for (int32_t i = 0; i < (int32_t) p.size(); i++) {
        /*
        if (p[i] < -10 || p[i] > 10) {
            Log::fatal("ERROR in get_mu_sigma, parameter[%d] was out of bounds: %lf\n", i, p[i]);
            Log::fatal("all parameters:\n");
            for (int32_t i = 0; i < (int32_t)p.size(); i++) {
                Log::fatal("\t%lf\n", p[i]);
            }
            exit(1);
        }
        */

        if (p[i] < -10) {
            mu += -10.0;
        } else if (p[i] > 10) {
            mu += 10.0;
        } else {
            mu += p[i];
        }
    }
    mu /= p.size();

    double temp;
    for (int32_t i = 0; i < (int32_t) p.size(); i++) {
        temp = (mu - p[i]) * (mu - p[i]);
        sigma += temp;
    }

    sigma /= (p.size() - 1);
    sigma = sqrt(sigma);
    Log::debug("\tmu: %lf, sigma: %lf, parameters.size(): %d\n", mu, sigma, p.size());
    if (std::isnan(mu) || std::isinf(mu) || std::isnan(sigma) || std::isinf(sigma)) {
        Log::fatal("mu or sigma was not a number, all parameters:\n");
        for (int32_t i = 0; i < (int32_t) p.size(); i++) {
            Log::fatal("\t%lf\n", p[i]);
        }

        Log::fatal("initial parameters:\n");
        for (int32_t i = 0; i < (int32_t) initial_parameters.size(); i++) {
            Log::fatal("\t%lf\n", initial_parameters[i]);
        }

        exit(1);
    }

    if (mu < -11.0 || mu > 11.0 || sigma < -30.0 || sigma > 30.0) {
        Log::fatal("mu or sigma exceeded possible bounds (11 or 30), all parameters:\n");
        for (int32_t i = 0; i < (int32_t) p.size(); i++) {
            Log::fatal("\t%lf\n", p[i]);
        }
        exit(1);
    }
}

RNN_Node_Interface* RNN_Genome::create_node(
    double mu, double sigma, int32_t node_type, int32_t& node_innovation_count, double depth, WeightRules* weight_rules
) {
    RNN_Node_Interface* n = NULL;
    WeightType mutated_component_weight = weight_rules->get_mutated_components_weight_method();
    WeightType weight_initialize = weight_rules->get_weight_initialize_method();

    Log::trace("CREATING NODE, type: '%s'\n", NODE_TYPES[node_type].c_str());
    if (node_type != DNAS_NODE) {
        n = create_hidden_node(node_type, node_innovation_count, depth);
    } else {
        n = create_dnas_node(node_innovation_count, depth, dnas_node_types);
    }

    if (mutated_component_weight == WeightType::LAMARCKIAN) {
        Log::debug("new component weight is lamarckian, setting new node weight to lamarckian \n");
        n->initialize_lamarckian(generator, normal_distribution, mu, sigma);
    } else if (mutated_component_weight == weight_initialize) {
        Log::debug(
            "new component weight is %s, setting new node's weight randomly with %s method \n",
            WEIGHT_TYPES_STRING[mutated_component_weight].c_str(), WEIGHT_TYPES_STRING[mutated_component_weight].c_str()
        );
        initialize_node_randomly(n, weight_rules);
    } else {
        Log::fatal(
            "new component weight is not set correctly, weight initialize: %s, new component weight: %s. \n",
            WEIGHT_TYPES_STRING[weight_initialize].c_str(), WEIGHT_TYPES_STRING[mutated_component_weight].c_str()
        );
        exit(1);
    }

    return n;
}

bool RNN_Genome::attempt_edge_insert(
    RNN_Node_Interface* n1, RNN_Node_Interface* n2, double mu, double sigma, int32_t& edge_innovation_count, WeightRules* weight_rules
) {
    Log::trace("\tadding edge between nodes %d and %d\n", n1->innovation_number, n2->innovation_number);
    WeightType mutated_component_weight = weight_rules->get_mutated_components_weight_method();
    WeightType weight_initialize = weight_rules->get_weight_initialize_method();

    if (n1->depth == n2->depth) {
        Log::trace("\tcannot add edge between nodes as their depths are the same: %lf and %lf\n", n1->depth, n2->depth);
        return false;
    }

    if (n2->depth < n1->depth) {
        // swap the nodes so that the lower one is first
        RNN_Node_Interface* temp = n2;
        n2 = n1;
        n1 = temp;
        Log::trace("\tswaping nodes, because n2->depth < n1->depth\n");
    }

    // check to see if an edge between the two nodes already exists
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->input_innovation_number == n1->innovation_number
            && edges[i]->output_innovation_number == n2->innovation_number) {
            if (!edges[i]->enabled) {
                // edge was disabled so we can enable it
                Log::trace("\tedge already exists but was disabled, enabling it.\n");
                edges[i]->enabled = true;
                // edges[i]->input_node->fan_out++;
                // edges[i]->output_node->fan_in++;
                return true;
            } else {
                Log::trace("\tedge already exists, not adding.\n");
                // edge was already enabled, so there will not be a change
                return false;
            }
        }
    }

    RNN_Edge* e = new RNN_Edge(++edge_innovation_count, n1, n2);
    if (mutated_component_weight == weight_initialize) {
        Log::debug("setting new edge weight with %s method \n", WEIGHT_TYPES_STRING[mutated_component_weight].c_str());
        if (weight_initialize == WeightType::XAVIER) {
            Log::debug("setting new edge weight to Xavier \n");
            e->weight = get_xavier_weight(n2);
        } else if (weight_initialize == WeightType::KAIMING) {
            Log::debug("setting new edge weight to Kaiming \n");
            e->weight = get_kaiming_weight(n2);
        } else if (weight_initialize == WeightType::RANDOM) {
            Log::debug("setting new edge weight to Random \n");
            e->weight = get_random_weight();
        } else if (weight_initialize == WeightType::GP) {
            e->weight = 1.0;
        } else {
            Log::fatal("weight initialization method %d is not set correctly \n", weight_initialize);
        }
    } else if (mutated_component_weight == WeightType::LAMARCKIAN) {
        Log::debug("setting new edge weight with Lamarckian method \n");
        e->weight = bound(normal_distribution.random(generator, mu, sigma));
    } else {
        Log::fatal(
            "new component weight method is not set correctly, weight initialize: %s, new component weight: %s\n",
            WEIGHT_TYPES_STRING[weight_initialize].c_str(), WEIGHT_TYPES_STRING[mutated_component_weight].c_str()
        );
    }

    Log::trace(
        "\tadding edge between nodes %d and %d, new edge weight: %lf\n", e->input_innovation_number,
        e->output_innovation_number, e->weight
    );
    edges.insert(upper_bound(edges.begin(), edges.end(), e, sort_RNN_Edges_by_depth()), e);

    return true;
}

bool RNN_Genome::attempt_recurrent_edge_insert(
    RNN_Node_Interface* n1, RNN_Node_Interface* n2, double mu, double sigma, uniform_int_distribution<int32_t> dist,
    int32_t& edge_innovation_count, WeightRules* weight_rules
) {
    Log::trace("\tadding recurrent edge between nodes %d and %d\n", n1->innovation_number, n2->innovation_number);
    WeightType mutated_component_weight = weight_rules->get_mutated_components_weight_method();
    WeightType weight_initialize = weight_rules->get_weight_initialize_method();
    // int32_t recurrent_depth = 1 + (rng_0_1(generator) * (max_recurrent_depth - 1));
    int32_t recurrent_depth = dist(generator);

    // check to see if an edge between the two nodes already exists
    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->input_innovation_number == n1->innovation_number
            && recurrent_edges[i]->output_innovation_number == n2->innovation_number
            && recurrent_edges[i]->recurrent_depth == recurrent_depth) {
            if (!recurrent_edges[i]->enabled) {
                // edge was disabled so we can enable it
                Log::trace("\trecurrent edge already exists but was disabled, enabling it.\n");
                recurrent_edges[i]->enabled = true;
                // recurrent_edges[i]->input_node->fan_out++;
                // recurrent_edges[i]->output_node->fan_in++;
                return true;
            } else {
                Log::trace(
                    "\tenabled recurrent edge already existed between selected nodes %d and %d at recurrent depth: "
                    "%d\n",
                    n1->innovation_number, n2->innovation_number, recurrent_depth
                );
                // edge was already enabled, so there will not be a change
                return false;
            }
        }
    }

    RNN_Recurrent_Edge* e = new RNN_Recurrent_Edge(++edge_innovation_count, recurrent_depth, n1, n2);
    if (mutated_component_weight == weight_initialize) {
        Log::debug(
            "setting new recurrent edge weight with %s method \n", WEIGHT_TYPES_STRING[mutated_component_weight].c_str()
        );
        if (weight_initialize == WeightType::XAVIER) {
            Log::debug("setting new recurrent edge weight to Xavier \n");
            e->weight = get_xavier_weight(n2);
        } else if (weight_initialize == WeightType::KAIMING) {
            Log::debug("setting new recurrent edge weight to Kaiming \n");
            e->weight = get_kaiming_weight(n2);
        } else if (weight_initialize == WeightType::RANDOM) {
            Log::debug("setting new recurrent edge weight to Random \n");
            e->weight = get_random_weight();
        } else if (weight_initialize == WeightType::GP) {
            e->weight = 1.0;
        } else {
            Log::fatal("Weight initialization method %d is not set correctly \n", weight_initialize);
        }
    } else if (mutated_component_weight == WeightType::LAMARCKIAN) {
        Log::debug("setting new recurrent edge weight with Lamarckian method \n");
        e->weight = bound(normal_distribution.random(generator, mu, sigma));
    } else {
        Log::fatal(
            "new component weight method is not set correctly, weight initialize: %s, new component weight: %s\n",
            WEIGHT_TYPES_STRING[weight_initialize].c_str(), WEIGHT_TYPES_STRING[mutated_component_weight].c_str()
        );
    }

    Log::trace(
        "\tadding recurrent edge with innovation number %d between nodes %d and %d, new edge weight: %d\n",
        e->innovation_number, e->input_innovation_number, e->output_innovation_number, e->weight
    );

    recurrent_edges.insert(
        upper_bound(recurrent_edges.begin(), recurrent_edges.end(), e, sort_RNN_Recurrent_Edges_by_depth()), e
    );
    return true;
}

void RNN_Genome::generate_recurrent_edges(
    RNN_Node_Interface* node, double mu, double sigma, uniform_int_distribution<int32_t> dist,
    int32_t& edge_innovation_count, WeightRules* weight_rules
) {
    if (node->node_type == JORDAN_NODE) {
        for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
            if (edges[i]->input_innovation_number == node->innovation_number && edges[i]->enabled) {
                attempt_recurrent_edge_insert(edges[i]->output_node, node, mu, sigma, dist, edge_innovation_count, weight_rules);
            }
        }
    } else if (node->node_type == ELMAN_NODE) {
        // elman nodes have a circular reference to themselves
        attempt_recurrent_edge_insert(node, node, mu, sigma, dist, edge_innovation_count, weight_rules);
    }
}

bool RNN_Genome::add_edge(double mu, double sigma, int32_t& edge_innovation_count, WeightRules* weight_rules) {
    Log::info("\tattempting to add edge!\n");
    vector<RNN_Node_Interface*> reachable_nodes;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->is_reachable()) {
            reachable_nodes.push_back(nodes[i]);
        }
    }

    /*
        if (reachable_nodes.size() == 0) {
            return false;
        }
    */
    Log::info("\treachable_nodes.size(): %d\n", reachable_nodes.size());

    int32_t position = rng_0_1(generator) * reachable_nodes.size();
    RNN_Node_Interface* n1 = reachable_nodes[position];
    Log::info("\tselected first node %d with depth %d\n", n1->innovation_number, n1->depth);
    // printf("pos: %d, size: %d\n", position, reachable_nodes.size());

    for (int32_t i = 0; i < (int32_t) reachable_nodes.size();) {
        auto it = reachable_nodes[i];
        if (it->depth == n1->depth) {
            reachable_nodes.erase(reachable_nodes.begin() + i);
        } else {
            i++;
        }
    }

    // for (auto i = reachable_nodes.begin(); i < reachable_nodes.end();) {
    //     if ((*i)->depth == n1->depth) {
    //         Log::info("\t\terasing node %d with depth %d\n", (*i)->innovation_number, (*i)->depth);
    //         reachable_nodes.erase(i);
    //     } else {
    //         Log::info("\t\tkeeping node %d with depth %d\n", (*i)->innovation_number, (*i)->depth);
    //         i++;
    //     }
    // }

    Log::trace("\treachable_nodes.size(): %d\n", reachable_nodes.size());

    position = rng_0_1(generator) * reachable_nodes.size();
    RNN_Node_Interface* n2 = reachable_nodes[position];
    Log::trace("\tselected second node %d with depth %d\n", n2->innovation_number, n2->depth);

    return attempt_edge_insert(n1, n2, mu, sigma, edge_innovation_count, weight_rules);
}

bool RNN_Genome::add_recurrent_edge(
    double mu, double sigma, uniform_int_distribution<int32_t> dist, int32_t& edge_innovation_count, WeightRules* weight_rules
) {
    Log::trace("\tattempting to add recurrent edge!\n");

    vector<RNN_Node_Interface*> possible_input_nodes;
    vector<RNN_Node_Interface*> possible_output_nodes;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->is_reachable()) {
            possible_input_nodes.push_back(nodes[i]);

            if (nodes[i]->layer_type != INPUT_LAYER) {
                possible_output_nodes.push_back(nodes[i]);
            }
        }
    }

    Log::trace("\tpossible_input_nodes.size(): %d\n", possible_input_nodes.size());
    Log::trace("\tpossible_output_nodes.size(): %d\n", possible_output_nodes.size());

    if (possible_input_nodes.size() == 0) {
        return false;
    }
    if (possible_output_nodes.size() == 0) {
        return false;
    }

    int32_t p1 = rng_0_1(generator) * possible_input_nodes.size();
    int32_t p2 = rng_0_1(generator) * possible_output_nodes.size();
    // no need to swap the nodes as recurrent connections can go backwards

    RNN_Node_Interface* n1 = possible_input_nodes[p1];
    Log::trace("\tselected first node %d with depth %d\n", n1->innovation_number, n1->depth);

    RNN_Node_Interface* n2 = possible_output_nodes[p2];
    Log::trace("\tselected second node %d with depth %d\n", n2->innovation_number, n2->depth);

    return attempt_recurrent_edge_insert(n1, n2, mu, sigma, dist, edge_innovation_count, weight_rules);
}

// TODO: should probably change these to enable/disable path
bool RNN_Genome::disable_edge() {
    // TODO: edge should be reachable
    vector<RNN_Edge*> enabled_edges;
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->enabled) {
            enabled_edges.push_back(edges[i]);
        }
    }

    vector<RNN_Recurrent_Edge*> enabled_recurrent_edges;
    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->enabled) {
            enabled_recurrent_edges.push_back(recurrent_edges[i]);
        }
    }

    if ((enabled_edges.size() + enabled_recurrent_edges.size()) == 0) {
        return false;
    }

    int32_t position = (enabled_edges.size() + enabled_recurrent_edges.size()) * rng_0_1(generator);

    if (position < (int32_t) enabled_edges.size()) {
        enabled_edges[position]->enabled = false;
        // innovation_list.erase(std::remove(innovation_list.begin(), innovation_list.end(),
        // enabled_edges[position]->get_innovation_number()), innovation_list.end());
        return true;
    } else {
        position -= enabled_edges.size();
        enabled_recurrent_edges[position]->enabled = false;
        // innovation_list.erase(std::remove(innovation_list.begin(), innovation_list.end(),
        // enabled_edges[position]->get_innovation_number()), innovation_list.end());
        return true;
    }
}

bool RNN_Genome::enable_edge(WeightRules* weight_rules) {
    // TODO: edge should be reachable
    vector<RNN_Edge*> disabled_edges;
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (!edges[i]->enabled) {
            disabled_edges.push_back(edges[i]);
        }
    }

    vector<RNN_Recurrent_Edge*> disabled_recurrent_edges;
    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (!recurrent_edges[i]->enabled) {
            disabled_recurrent_edges.push_back(recurrent_edges[i]);
        }
    }

    if ((disabled_edges.size() + disabled_recurrent_edges.size()) == 0) {
        return false;
    }

    int32_t position = (disabled_edges.size() + disabled_recurrent_edges.size()) * rng_0_1(generator);
    WeightType weight_initialize = weight_rules->get_weight_initialize_method();
    if (position < (int32_t) disabled_edges.size()) {
        disabled_edges[position]->enabled = true;
        if (weight_initialize == WeightType::GP) {
            disabled_edges[position]->weight = 1.0;
        }
        // innovation_list.push_back(disabled_edges[position]->get_innovation_number);
        return true;
    } else {
        position -= disabled_edges.size();
        disabled_recurrent_edges[position]->enabled = true;
        if (weight_initialize == WeightType::GP) {
            disabled_recurrent_edges[position]->weight = 1.0;
        }
        // innovation_list.push_back(disabled_recurrent_edges[position]->get_innovation_number);
        return true;
    }
}

bool RNN_Genome::split_edge(
    double mu, double sigma, int32_t node_type, uniform_int_distribution<int32_t> dist, int32_t& edge_innovation_count,
    int32_t& node_innovation_count, WeightRules* weight_rules
) {
    Log::trace("\tattempting to split an edge!\n");
    vector<RNN_Edge*> enabled_edges;
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->enabled) {
            enabled_edges.push_back(edges[i]);
        }
    }

    vector<RNN_Recurrent_Edge*> enabled_recurrent_edges;
    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->enabled) {
            enabled_recurrent_edges.push_back(recurrent_edges[i]);
        }
    }

    int32_t position = rng_0_1(generator) * (enabled_edges.size() + enabled_recurrent_edges.size());

    bool was_forward_edge = false;
    RNN_Node_Interface* n1 = NULL;
    RNN_Node_Interface* n2 = NULL;
    if (position < (int32_t) enabled_edges.size()) {
        RNN_Edge* edge = enabled_edges[position];
        n1 = edge->input_node;
        n2 = edge->output_node;
        edge->enabled = false;
        was_forward_edge = true;
    } else {
        position -= enabled_edges.size();
        RNN_Recurrent_Edge* recurrent_edge = enabled_recurrent_edges[position];
        n1 = recurrent_edge->input_node;
        n2 = recurrent_edge->output_node;
        recurrent_edge->enabled = false;
    }

    double new_depth = (n1->get_depth() + n2->get_depth()) / 2.0;
    RNN_Node_Interface* new_node = create_node(mu, sigma, node_type, node_innovation_count, new_depth, weight_rules);

    nodes.insert(upper_bound(nodes.begin(), nodes.end(), new_node, sort_RNN_Nodes_by_depth()), new_node);

    if (was_forward_edge) {
        attempt_edge_insert(n1, new_node, mu, sigma, edge_innovation_count, weight_rules);
        attempt_edge_insert(new_node, n2, mu, sigma, edge_innovation_count, weight_rules);
    } else {
        attempt_recurrent_edge_insert(n1, new_node, mu, sigma, dist, edge_innovation_count, weight_rules);
        attempt_recurrent_edge_insert(new_node, n2, mu, sigma, dist, edge_innovation_count, weight_rules);
    }

    if (node_type == JORDAN_NODE || node_type == ELMAN_NODE) {
        generate_recurrent_edges(new_node, mu, sigma, dist, edge_innovation_count, weight_rules);
    }

    // node_initialize(new_node);

    return true;
}

bool RNN_Genome::connect_new_input_node(
    double mu, double sigma, RNN_Node_Interface* new_node, uniform_int_distribution<int32_t> dist,
    int32_t& edge_innovation_count, bool not_all_hidden, WeightRules* weight_rules
) {
    Log::trace("\tattempting to connect a new input node (%d) for transfer learning!\n", new_node->innovation_number);

    vector<RNN_Node_Interface*> possible_outputs;

    /*
    int32_t enabled_count = 0;
    double avg_outputs = 0.0;

    for (int32_t i = 0; i < (int32_t)nodes.size(); i++) {
        //can connect to output or hidden nodes
        if (nodes[i]->get_layer_type() == OUTPUT_LAYER || (nodes[i]->get_layer_type() == HIDDEN_LAYER &&
    nodes[i]->is_reachable())) { Log::info("\tpotential connection node[%d], depth: %lf, total_inputs: %d,
    total_outputs: %d\n", nodes[i]->get_innovation_number(), nodes[i]->get_depth(), nodes[i]->get_total_inputs(),
    nodes[i]->get_total_outputs()); possible_outputs.push_back(nodes[i]);
        }

        if (nodes[i]->enabled) {
            enabled_count++;
            avg_outputs += nodes[i]->total_outputs;
        }
    }

    avg_outputs /= enabled_count;

    double output_sigma = 0.0;
    double temp;
    for (int32_t i = 0; i < (int32_t)nodes.size(); i++) {
        if (nodes[i]->enabled) {
            temp = (avg_outputs - nodes[i]->total_outputs);
            temp = temp * temp;
            output_sigma += temp;
        }
    }

    output_sigma /= (enabled_count - 1);
    output_sigma = sqrt(output_sigma);


    int32_t max_outputs = fmax(1, 2.0 + normal_distribution.random(generator, avg_outputs, output_sigma));
    while (possible_outputs.size() > max_outputs) {
        int32_t position = rng_0_1(generator) * possible_outputs.size();
        possible_outputs.erase(possible_outputs.begin() + position);
    }
    Log::info("\tadd new input node, max_outputs: %d\n", max_outputs);
    */

    possible_outputs = pick_possible_nodes(INPUT_LAYER, not_all_hidden, "input");

    int32_t enabled_edges = get_enabled_edge_count();
    int32_t enabled_recurrent_edges = get_enabled_recurrent_edge_count();

    double recurrent_probability =
        (double) enabled_recurrent_edges / (double) (enabled_recurrent_edges + enabled_edges);
    // recurrent_probability = fmax(0.2, recurrent_probability);

    Log::trace("\tadd new node for transfer recurrent probability: %lf\n", recurrent_probability);

    for (int32_t i = 0; i < (int32_t) possible_outputs.size(); i++) {
        // TODO: remove after running tests without recurrent edges
        // recurrent_probability = 0;

        if (rng_0_1(generator) < recurrent_probability) {
            attempt_recurrent_edge_insert(new_node, possible_outputs[i], mu, sigma, dist, edge_innovation_count, weight_rules);
        } else {
            attempt_edge_insert(new_node, possible_outputs[i], mu, sigma, edge_innovation_count, weight_rules);
        }
    }

    return true;
}

//------------------------------------------------

vector<RNN_Node_Interface*> RNN_Genome::pick_possible_nodes(int32_t layer_type, bool not_all_hidden, string node_type) {
    int32_t enabled_count = 0;
    double avg_nodes = 0.0;

    vector<RNN_Node_Interface*> possible_nodes;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        // can connect to node or hidden nodes
        if (nodes[i]->get_layer_type() == layer_type
            || (nodes[i]->get_layer_type() == HIDDEN_LAYER && nodes[i]->is_reachable())) {
            possible_nodes.push_back(nodes[i]);
            Log::trace(
                "\tpotential connection node[%d], depth: %lf, total_inputs: %d, total_outputs: %d\n",
                nodes[i]->get_innovation_number(), nodes[i]->get_depth(), nodes[i]->get_total_inputs(),
                nodes[i]->get_total_outputs()
            );
        }

        if (nodes[i]->enabled) {
            enabled_count++;
            avg_nodes += nodes[i]->total_inputs;
        }
    }

    if (not_all_hidden) {
        avg_nodes /= enabled_count;

        double _sigma = 0.0;
        double temp;
        for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
            if (nodes[i]->enabled) {
                temp = (avg_nodes - nodes[i]->total_inputs);
                temp = temp * temp;
                _sigma += temp;
            }
        }

        _sigma /= (enabled_count - 1);
        _sigma = sqrt(_sigma);

        int32_t max_nodes = fmax(1, 2.0 + normal_distribution.random(generator, avg_nodes, _sigma));

        while ((int32_t) possible_nodes.size() > max_nodes) {
            int32_t position = rng_0_1(generator) * possible_nodes.size();
            possible_nodes.erase(possible_nodes.begin() + position);
        }
        Log::trace("\tadd new %s node, max_inputs: %d\n", node_type.c_str(), max_nodes);
    }

    return possible_nodes;
}

//------------------------------------------------

bool RNN_Genome::connect_new_output_node(
    double mu, double sigma, RNN_Node_Interface* new_node, uniform_int_distribution<int32_t> dist,
    int32_t& edge_innovation_count, bool not_all_hidden, WeightRules* weight_rules
) {
    Log::trace("\tattempting to connect a new output node for transfer learning!\n");

    vector<RNN_Node_Interface*> possible_inputs;

    /*
    int32_t enabled_count = 0;
    double avg_inputs = 0.0;

    for (int32_t i = 0; i < (int32_t)nodes.size(); i++) {
        //can connect to input or hidden nodes
        if (nodes[i]->get_layer_type() == INPUT_LAYER || (nodes[i]->get_layer_type() == HIDDEN_LAYER &&
    nodes[i]->is_reachable())) { possible_inputs.push_back(nodes[i]); Log::info("\tpotential connection node[%d], depth:
    %lf, total_inputs: %d, total_outputs: %d\n", nodes[i]->get_innovation_number(), nodes[i]->get_depth(),
    nodes[i]->get_total_inputs(), nodes[i]->get_total_outputs());
        }

        if (nodes[i]->enabled) {
            enabled_count++;
            avg_inputs += nodes[i]->total_inputs;
        }
    }

    avg_inputs /= enabled_count;

    double input_sigma = 0.0;
    double temp;
    for (int32_t i = 0; i < (int32_t)nodes.size(); i++) {
        if (nodes[i]->enabled) {
            temp = (avg_inputs - nodes[i]->total_inputs);
            temp = temp * temp;
            input_sigma += temp;
        }
    }

    input_sigma /= (enabled_count - 1);
    input_sigma = sqrt(input_sigma);

    int32_t max_inputs = fmax(1, 2.0 + normal_distribution.random(generator, avg_inputs, input_sigma));
    while (possible_inputs.size() > max_inputs) {
        int32_t position = rng_0_1(generator) * possible_inputs.size();
        possible_inputs.erase(possible_inputs.begin() + position);
    }
    Log::info("\tadd new output node, max_inputs: %d\n", max_inputs);
    */

    possible_inputs = pick_possible_nodes(INPUT_LAYER, not_all_hidden, "output");

    int32_t enabled_edges = get_enabled_edge_count();
    int32_t enabled_recurrent_edges = get_enabled_recurrent_edge_count();

    double recurrent_probability =
        (double) enabled_recurrent_edges / (double) (enabled_recurrent_edges + enabled_edges);
    // recurrent_probability = fmax(0.2, recurrent_probability);

    Log::trace("\tadd new node for transfer recurrent probability: %lf\n", recurrent_probability);

    for (int32_t i = 0; i < (int32_t) possible_inputs.size(); i++) {
        // TODO: remove after running tests without recurrent edges
        // recurrent_probability = 0;

        if (rng_0_1(generator) < recurrent_probability) {
            attempt_recurrent_edge_insert(possible_inputs[i], new_node, mu, sigma, dist, edge_innovation_count, weight_rules);
        } else {
            attempt_edge_insert(possible_inputs[i], new_node, mu, sigma, edge_innovation_count, weight_rules);
        }
    }

    return true;
}

// INFO: ADDED BY ABDELRAHMAN TO USE FOR TRANSFER LEARNING
bool RNN_Genome::connect_node_to_hid_nodes(
    double mu, double sig, RNN_Node_Interface* new_node, uniform_int_distribution<int32_t> dist,
    int32_t& edge_innovation_count, bool from_input, WeightRules* weight_rules
) {
    vector<RNN_Node_Interface*> candidate_nodes;

    int32_t enabled_count = 0;
    double avg_candidates = 0.0;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->get_layer_type() == HIDDEN_LAYER && nodes[i]->is_reachable()) {
            if (nodes[i]->enabled) {
                candidate_nodes.push_back(nodes[i]);
                enabled_count++;
                if (from_input) {
                    avg_candidates += nodes[i]->total_inputs;
                } else {
                    avg_candidates += nodes[i]->total_outputs;
                }
            }
        }
    }

    avg_candidates /= enabled_count;

    double sigma = 0.0;
    double temp;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->enabled && nodes[i]->get_layer_type() == HIDDEN_LAYER) {
            if (from_input) {
                temp = (avg_candidates - nodes[i]->total_inputs);
            } else {
                temp = (avg_candidates - nodes[i]->total_outputs);
            }
            temp = temp * temp;
            sigma += temp;
        }
    }
    if (enabled_count != 1) {
        sigma /= (enabled_count - 1);
    } else {
        sigma /= 1;
    }
    sigma = sqrt(sigma);

    int32_t max_candidates = fmax(1, 2.0 + normal_distribution.random(generator, avg_candidates, sigma));

    int32_t enabled_edges = get_enabled_edge_count();
    int32_t enabled_recurrent_edges = get_enabled_recurrent_edge_count();

    double recurrent_probability =
        (double) enabled_recurrent_edges / (double) (enabled_recurrent_edges + enabled_edges);

    while ((int32_t) candidate_nodes.size() > max_candidates) {
        int32_t position = rng_0_1(generator) * candidate_nodes.size();
        candidate_nodes.erase(candidate_nodes.begin() + position);
    }

    for (auto node : candidate_nodes) {
        if (rng_0_1(generator) < recurrent_probability) {
            int32_t recurrent_depth = dist(generator);
            RNN_Recurrent_Edge* e;
            if (from_input) {
                e = new RNN_Recurrent_Edge(++edge_innovation_count, recurrent_depth, new_node, node);
                // innovation_list.push_back(edge_innovation_count);
            }

            else {
                e = new RNN_Recurrent_Edge(++edge_innovation_count, recurrent_depth, node, new_node);
                // innovation_list.push_back(edge_innovation_count);
            }

            e->weight = bound(normal_distribution.random(generator, mu, sigma));
            Log::debug(
                "\tadding recurrent edge between nodes %d and %d, new edge weight: %d\n", e->input_innovation_number,
                e->output_innovation_number, e->weight
            );
            recurrent_edges.insert(
                upper_bound(recurrent_edges.begin(), recurrent_edges.end(), e, sort_RNN_Recurrent_Edges_by_depth()), e
            );

            initial_parameters.push_back(e->weight);
            best_parameters.push_back(e->weight);

            // attempt_recurrent_edge_insert(new_node, node, mu, sigma, dist, edge_innovation_count);
        } else {
            RNN_Edge* e;
            if (from_input) {
                e = new RNN_Edge(++edge_innovation_count, new_node, node);
                // innovation_list.push_back(edge_innovation_count);
            } else {
                e = new RNN_Edge(++edge_innovation_count, node, new_node);
                // innovation_list.push_back(edge_innovation_count);
            }
            e->weight = bound(normal_distribution.random(generator, mu, sigma));
            Log::trace(
                "\tadding edge between nodes %d and %d, new edge weight: %lf\n", e->input_innovation_number,
                e->output_innovation_number, e->weight
            );
            edges.insert(upper_bound(edges.begin(), edges.end(), e, sort_RNN_Edges_by_depth()), e);

            initial_parameters.push_back(e->weight);
            best_parameters.push_back(e->weight);

            // attempt_edge_insert(node, new_node, mu, sig, edge_innovation_count);
        }
    }
    return true;
}

/*   ################# ################# ################# */

bool RNN_Genome::add_node(
    double mu, double sigma, int32_t node_type, uniform_int_distribution<int32_t> dist, int32_t& edge_innovation_count,
    int32_t& node_innovation_count, WeightRules* weight_rules
) {
    Log::trace("\tattempting to add a node!\n");
    double split_depth = rng_0_1(generator);

    vector<RNN_Node_Interface*> possible_inputs;
    vector<RNN_Node_Interface*> possible_outputs;

    int32_t enabled_count = 0;
    double avg_inputs = 0.0;
    double avg_outputs = 0.0;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->depth < split_depth && nodes[i]->is_reachable()) {
            possible_inputs.push_back(nodes[i]);
        } else if (nodes[i]->is_reachable()) {
            possible_outputs.push_back(nodes[i]);
        }

        if (nodes[i]->enabled) {
            enabled_count++;
            avg_inputs += nodes[i]->total_inputs;
            avg_outputs += nodes[i]->total_outputs;
        }
    }

    avg_inputs /= enabled_count;
    avg_outputs /= enabled_count;

    double input_sigma = 0.0;
    double output_sigma = 0.0;
    double temp;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->enabled) {
            temp = (avg_inputs - nodes[i]->total_inputs);
            temp = temp * temp;
            input_sigma += temp;

            temp = (avg_outputs - nodes[i]->total_outputs);
            temp = temp * temp;
            output_sigma += temp;
        }
    }

    input_sigma /= (enabled_count - 1);
    input_sigma = sqrt(input_sigma);

    output_sigma /= (enabled_count - 1);
    output_sigma = sqrt(output_sigma);

    int32_t max_inputs = fmax(1, 2.0 + normal_distribution.random(generator, avg_inputs, input_sigma));
    int32_t max_outputs = fmax(1, 2.0 + normal_distribution.random(generator, avg_outputs, output_sigma));
    Log::trace("\tadd node, split depth: %lf, max_inputs: %d, max_outputs: %d\n", split_depth, max_inputs, max_outputs);

    int32_t enabled_edges = get_enabled_edge_count();
    int32_t enabled_recurrent_edges = get_enabled_recurrent_edge_count();

    double recurrent_probability =
        (double) enabled_recurrent_edges / (double) (enabled_recurrent_edges + enabled_edges);
    // recurrent_probability = fmax(0.2, recurrent_probability);

    Log::trace("\tadd node recurrent probability: %lf\n", recurrent_probability);

    while ((int32_t) possible_inputs.size() > max_inputs) {
        int32_t position = rng_0_1(generator) * possible_inputs.size();
        possible_inputs.erase(possible_inputs.begin() + position);
    }

    while ((int32_t) possible_outputs.size() > max_outputs) {
        int32_t position = rng_0_1(generator) * possible_outputs.size();
        possible_outputs.erase(possible_outputs.begin() + position);
    }

    RNN_Node_Interface* new_node = create_node(mu, sigma, node_type, node_innovation_count, split_depth, weight_rules);
    nodes.insert(upper_bound(nodes.begin(), nodes.end(), new_node, sort_RNN_Nodes_by_depth()), new_node);

    for (int32_t i = 0; i < (int32_t) possible_inputs.size(); i++) {
        // TODO: remove after running tests without recurrent edges
        // recurrent_probability = 0;

        if (rng_0_1(generator) < recurrent_probability) {
            attempt_recurrent_edge_insert(possible_inputs[i], new_node, mu, sigma, dist, edge_innovation_count, weight_rules);
        } else {
            attempt_edge_insert(possible_inputs[i], new_node, mu, sigma, edge_innovation_count, weight_rules);
        }
    }

    for (int32_t i = 0; i < (int32_t) possible_outputs.size(); i++) {
        // TODO: remove after running tests without recurrent edges
        // recurrent_probability = 0;

        if (rng_0_1(generator) < recurrent_probability) {
            attempt_recurrent_edge_insert(new_node, possible_outputs[i], mu, sigma, dist, edge_innovation_count, weight_rules);
        } else {
            attempt_edge_insert(new_node, possible_outputs[i], mu, sigma, edge_innovation_count, weight_rules);
        }
    }

    if (node_type == JORDAN_NODE || node_type == ELMAN_NODE) {
        generate_recurrent_edges(new_node, mu, sigma, dist, edge_innovation_count, weight_rules);
    }

    // node_initialize(new_node);

    return true;
}

bool RNN_Genome::enable_node(WeightRules* weight_rules) {
    Log::trace("\tattempting to enable a node!\n");
    vector<RNN_Node_Interface*> possible_nodes;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (!nodes[i]->enabled) {
            possible_nodes.push_back(nodes[i]);
        }
    }

    if (possible_nodes.size() == 0) {
        return false;
    }

    int32_t position = rng_0_1(generator) * possible_nodes.size();
    possible_nodes[position]->enabled = true;
    Log::trace(
        "\tenabling node %d at depth %lf\n", possible_nodes[position]->innovation_number,
        possible_nodes[position]->depth
    );

    return true;
}

bool RNN_Genome::disable_node() {
    Log::trace("\tattempting to disable a node!\n");
    vector<RNN_Node_Interface*> possible_nodes;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->layer_type != OUTPUT_LAYER && nodes[i]->enabled) {
            possible_nodes.push_back(nodes[i]);
        }
    }

    if (possible_nodes.size() == 0) {
        return false;
    }

    int32_t position = rng_0_1(generator) * possible_nodes.size();
    possible_nodes[position]->enabled = false;
    Log::trace(
        "\tdisabling node %d at depth %lf\n", possible_nodes[position]->innovation_number,
        possible_nodes[position]->depth
    );

    return true;
}

bool RNN_Genome::split_node(
    double mu, double sigma, int32_t node_type, uniform_int_distribution<int32_t> dist, int32_t& edge_innovation_count,
    int32_t& node_innovation_count, WeightRules* weight_rules
) {
    Log::trace("\tattempting to split a node!\n");
    vector<RNN_Node_Interface*> possible_nodes;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->layer_type != INPUT_LAYER && nodes[i]->layer_type != OUTPUT_LAYER && nodes[i]->is_reachable()) {
            possible_nodes.push_back(nodes[i]);
        }
    }

    if (possible_nodes.size() == 0) {
        return false;
    }

    int32_t position = rng_0_1(generator) * possible_nodes.size();
    RNN_Node_Interface* selected_node = possible_nodes[position];
    Log::trace("\tselected node: %d at depth %lf\n", selected_node->innovation_number, selected_node->depth);

    vector<RNN_Edge*> input_edges;
    vector<RNN_Edge*> output_edges;
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->output_innovation_number == selected_node->innovation_number) {
            input_edges.push_back(edges[i]);
        }

        if (edges[i]->input_innovation_number == selected_node->innovation_number) {
            output_edges.push_back(edges[i]);
        }
    }

    vector<RNN_Recurrent_Edge*> recurrent_edges_1;
    vector<RNN_Recurrent_Edge*> recurrent_edges_2;

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->output_innovation_number == selected_node->innovation_number
            || recurrent_edges[i]->input_innovation_number == selected_node->innovation_number) {
            if (rng_0_1(generator) < 0.5) {
                recurrent_edges_1.push_back(recurrent_edges[i]);
            }
            if (rng_0_1(generator) < 0.5) {
                recurrent_edges_2.push_back(recurrent_edges[i]);
            }
        }
    }
    Log::trace(
        "\t\trecurrent_edges_1.size(): %d, recurrent_edges_2.size(): %d, input_edges.size(): %d, output_edges.size(): "
        "%d\n",
        recurrent_edges_1.size(), recurrent_edges_2.size(), input_edges.size(), output_edges.size()
    );

    if (input_edges.size() == 0 || output_edges.size() == 0) {
        Log::warning("\tthe input or output edges size was 0 for the selected node, we cannot split it\n");
        // write_graphviz("error_genome.gv");
        // exit(1);
        return false;
    }

    vector<RNN_Edge*> input_edges_1;
    vector<RNN_Edge*> input_edges_2;

    for (int32_t i = 0; i < (int32_t) input_edges.size(); i++) {
        if (rng_0_1(generator) < 0.5) {
            input_edges_1.push_back(input_edges[i]);
        }
        if (rng_0_1(generator) < 0.5) {
            input_edges_2.push_back(input_edges[i]);
        }
    }

    // make sure there is at least one input edge
    if (input_edges_1.size() == 0 && input_edges.size() > 0) {
        int32_t position = rng_0_1(generator) * input_edges.size();
        input_edges_1.push_back(input_edges[position]);
    }

    if (input_edges_2.size() == 0 && input_edges.size() > 0) {
        int32_t position = rng_0_1(generator) * input_edges.size();
        input_edges_2.push_back(input_edges[position]);
    }

    vector<RNN_Edge*> output_edges_1;
    vector<RNN_Edge*> output_edges_2;

    for (int32_t i = 0; i < (int32_t) output_edges.size(); i++) {
        if (rng_0_1(generator) < 0.5) {
            output_edges_1.push_back(output_edges[i]);
        }
        if (rng_0_1(generator) < 0.5) {
            output_edges_2.push_back(output_edges[i]);
        }
    }

    // make sure there is at least one output edge
    if (output_edges_1.size() == 0 && output_edges.size() > 0) {
        int32_t position = rng_0_1(generator) * output_edges.size();
        output_edges_1.push_back(output_edges[position]);
    }

    if (output_edges_2.size() == 0 && output_edges.size() > 0) {
        int32_t position = rng_0_1(generator) * output_edges.size();
        output_edges_2.push_back(output_edges[position]);
    }

    // create the two new nodes
    double n1_avg_input = 0.0, n1_avg_output = 0.0;
    double n2_avg_input = 0.0, n2_avg_output = 0.0;

    for (int32_t i = 0; i < (int32_t) input_edges_1.size(); i++) {
        n1_avg_input += input_edges_1[i]->input_node->depth;
    }
    n1_avg_input /= input_edges_1.size();

    for (int32_t i = 0; i < (int32_t) output_edges_1.size(); i++) {
        n1_avg_output += output_edges_1[i]->output_node->depth;
    }
    n1_avg_output /= output_edges_1.size();

    for (int32_t i = 0; i < (int32_t) input_edges_2.size(); i++) {
        n2_avg_input += input_edges_2[i]->input_node->depth;
    }
    n2_avg_input /= input_edges_2.size();

    for (int32_t i = 0; i < (int32_t) output_edges_2.size(); i++) {
        n2_avg_output += output_edges_2[i]->output_node->depth;
    }
    n2_avg_output /= output_edges_2.size();

    double new_depth_1 = (n1_avg_input + n1_avg_output) / 2.0;
    double new_depth_2 = (n2_avg_input + n2_avg_output) / 2.0;

    RNN_Node_Interface* new_node_1 = create_node(mu, sigma, node_type, node_innovation_count, new_depth_1, weight_rules);
    RNN_Node_Interface* new_node_2 = create_node(mu, sigma, node_type, node_innovation_count, new_depth_2, weight_rules);

    // create the new edges
    for (int32_t i = 0; i < (int32_t) input_edges_1.size(); i++) {
        attempt_edge_insert(input_edges_1[i]->input_node, new_node_1, mu, sigma, edge_innovation_count, weight_rules);
    }

    for (int32_t i = 0; i < (int32_t) output_edges_1.size(); i++) {
        attempt_edge_insert(new_node_1, output_edges_1[i]->output_node, mu, sigma, edge_innovation_count, weight_rules);
    }

    for (int32_t i = 0; i < (int32_t) input_edges_2.size(); i++) {
        attempt_edge_insert(input_edges_2[i]->input_node, new_node_2, mu, sigma, edge_innovation_count, weight_rules);
    }

    for (int32_t i = 0; i < (int32_t) output_edges_2.size(); i++) {
        attempt_edge_insert(new_node_2, output_edges_2[i]->output_node, mu, sigma, edge_innovation_count, weight_rules);
    }

    Log::debug("\tattempting recurrent edge inserts for split node\n");

    for (int32_t i = 0; i < (int32_t) recurrent_edges_1.size(); i++) {
        if (recurrent_edges_1[i]->input_innovation_number == selected_node->innovation_number) {
            attempt_recurrent_edge_insert(
                new_node_1, recurrent_edges_1[i]->output_node, mu, sigma, dist, edge_innovation_count,
                weight_rules
            );
        } else if (recurrent_edges_1[i]->output_innovation_number == selected_node->innovation_number) {
            attempt_recurrent_edge_insert(
                recurrent_edges_1[i]->input_node, new_node_1, mu, sigma, dist, edge_innovation_count, weight_rules
            );
        } else {
            Log::fatal(
                "\trecurrent edge list for split had an edge which was not connected to the selected node! This should "
                "never happen.\n"
            );
            exit(1);
        }
        // disable the old recurrent edges
        recurrent_edges_1[i]->enabled = false;
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges_2.size(); i++) {
        if (recurrent_edges_2[i]->input_innovation_number == selected_node->innovation_number) {
            attempt_recurrent_edge_insert(
                new_node_2, recurrent_edges_2[i]->output_node, mu, sigma, dist, edge_innovation_count, weight_rules
            );
        } else if (recurrent_edges_2[i]->output_innovation_number == selected_node->innovation_number) {
            attempt_recurrent_edge_insert(
                recurrent_edges_2[i]->input_node, new_node_2, mu, sigma, dist, edge_innovation_count, weight_rules
            );
        } else {
            Log::fatal(
                "\trecurrent edge list for split had an edge which was not connected to the selected node! This should "
                "never happen.\n"
            );
            exit(1);
        }
        // disable the old recurrent edges
        recurrent_edges_2[i]->enabled = false;
    }

    nodes.insert(upper_bound(nodes.begin(), nodes.end(), new_node_1, sort_RNN_Nodes_by_depth()), new_node_1);
    nodes.insert(upper_bound(nodes.begin(), nodes.end(), new_node_2, sort_RNN_Nodes_by_depth()), new_node_2);

    // disable the selected node and it's edges
    for (int32_t i = 0; i < (int32_t) input_edges.size(); i++) {
        input_edges[i]->enabled = false;
    }

    for (int32_t i = 0; i < (int32_t) output_edges.size(); i++) {
        output_edges[i]->enabled = false;
    }

    selected_node->enabled = false;

    if (node_type == JORDAN_NODE || node_type == ELMAN_NODE) {
        generate_recurrent_edges(new_node_1, mu, sigma, dist, edge_innovation_count, weight_rules);
    }

    if (node_type == JORDAN_NODE || node_type == ELMAN_NODE) {
        generate_recurrent_edges(new_node_2, mu, sigma, dist, edge_innovation_count, weight_rules);
    }

    // node_initialize(new_node_1);
    // node_initialize(new_node_2);

    return true;
}

bool RNN_Genome::merge_node(
    double mu, double sigma, int32_t node_type, uniform_int_distribution<int32_t> dist, int32_t& edge_innovation_count,
    int32_t& node_innovation_count, WeightRules* weight_rules
) {
    Log::trace("\tattempting to merge a node!\n");
    vector<RNN_Node_Interface*> possible_nodes;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->layer_type != INPUT_LAYER && nodes[i]->layer_type != OUTPUT_LAYER) {
            possible_nodes.push_back(nodes[i]);
        }
    }

    if (possible_nodes.size() < 2) {
        return false;
    }

    while (possible_nodes.size() > 2) {
        int32_t position = rng_0_1(generator) * possible_nodes.size();
        possible_nodes.erase(possible_nodes.begin() + position);
    }

    RNN_Node_Interface* n1 = possible_nodes[0];
    RNN_Node_Interface* n2 = possible_nodes[1];
    n1->enabled = false;
    n2->enabled = false;

    double new_depth = (n1->depth + n2->depth) / 2.0;

    RNN_Node_Interface* new_node = create_node(mu, sigma, node_type, node_innovation_count, new_depth, weight_rules);
    nodes.insert(upper_bound(nodes.begin(), nodes.end(), new_node, sort_RNN_Nodes_by_depth()), new_node);

    vector<RNN_Edge*> merged_edges;
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        RNN_Edge* e = edges[i];

        if (e->input_innovation_number == n1->innovation_number || e->input_innovation_number == n2->innovation_number
            || e->output_innovation_number == n1->innovation_number
            || e->output_innovation_number == n2->innovation_number) {
            // if the edge is between the two merged nodes just disasble it
            if ((e->input_innovation_number == n1->innovation_number
                 && e->output_innovation_number == n2->innovation_number)
                || (e->input_innovation_number == n2->innovation_number
                    && e->output_innovation_number == n1->innovation_number)) {
                e->enabled = false;
            }

            if (e->enabled) {
                e->enabled = false;
                merged_edges.push_back(e);
            }
        }
    }

    for (int32_t i = 0; i < (int32_t) merged_edges.size(); i++) {
        RNN_Edge* e = merged_edges[i];

        RNN_Node_Interface* input_node = NULL;
        RNN_Node_Interface* output_node = NULL;

        if (e->input_innovation_number == n1->innovation_number
            || e->input_innovation_number == n2->innovation_number) {
            input_node = new_node;
        } else {
            input_node = e->input_node;
        }

        if (e->output_innovation_number == n1->innovation_number
            || e->output_innovation_number == n2->innovation_number) {
            output_node = new_node;
        } else {
            output_node = e->output_node;
        }

        if (input_node->depth == output_node->depth) {
            Log::trace("\tskipping merged edge because the input and output nodes are the same depth\n");
            continue;
        }

        // swap the edges becasue the input node is deeper than the output node
        if (input_node->depth > output_node->depth) {
            RNN_Node_Interface* tmp = input_node;
            input_node = output_node;
            output_node = tmp;
        }

        attempt_edge_insert(input_node, output_node, mu, sigma, edge_innovation_count, weight_rules);
    }

    vector<RNN_Recurrent_Edge*> merged_recurrent_edges;
    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        RNN_Recurrent_Edge* e = recurrent_edges[i];

        if (e->input_innovation_number == n1->innovation_number || e->input_innovation_number == n2->innovation_number
            || e->output_innovation_number == n1->innovation_number
            || e->output_innovation_number == n2->innovation_number) {
            if (e->enabled) {
                e->enabled = false;
                merged_recurrent_edges.push_back(e);
            }
        }
    }

    // add recurrent edges to merged node
    for (int32_t i = 0; i < (int32_t) merged_recurrent_edges.size(); i++) {
        RNN_Recurrent_Edge* e = merged_recurrent_edges[i];

        RNN_Node_Interface* input_node = NULL;
        RNN_Node_Interface* output_node = NULL;

        if (e->input_innovation_number == n1->innovation_number
            || e->input_innovation_number == n2->innovation_number) {
            input_node = new_node;
        } else {
            input_node = e->input_node;
        }

        if (e->output_innovation_number == n1->innovation_number
            || e->output_innovation_number == n2->innovation_number) {
            output_node = new_node;
        } else {
            output_node = e->output_node;
        }

        attempt_recurrent_edge_insert(input_node, output_node, mu, sigma, dist, edge_innovation_count, weight_rules);
    }

    if (node_type == JORDAN_NODE || node_type == ELMAN_NODE) {
        generate_recurrent_edges(new_node, mu, sigma, dist, edge_innovation_count, weight_rules);
    }

    // node_initialize(new_node);

    return true;
}

string RNN_Genome::get_color(double weight, bool is_recurrent) {
    double max = 0.0;
    double min = 0.0;

    ostringstream oss;

    if (!is_recurrent) {
        for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
            if (edges[i]->weight > max) {
                max = edges[i]->weight;
            } else if (edges[i]->weight < min) {
                min = edges[i]->weight;
            }
        }

    } else {
        for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
            if (recurrent_edges[i]->weight > max) {
                max = recurrent_edges[i]->weight;
            } else if (recurrent_edges[i]->weight < min) {
                min = recurrent_edges[i]->weight;
            }
        }
    }

    double value;
    if (weight <= 0) {
        value = -((weight / min) / 2.0) + 0.5;
    } else {
        value = ((weight / max) / 2.0) + 0.5;
    }
    Color color = get_colormap(value);

    Log::debug("weight: %lf, converted to value: %lf\n", weight, value);

    oss << hex << setw(2) << setfill('0') << color.red << hex << setw(2) << setfill('0') << color.green << hex
        << setw(2) << setfill('0') << color.blue;

    return oss.str();
}

void RNN_Genome::write_graphviz(string filename) {
    ofstream outfile(filename);

    outfile << "digraph RNN {" << endl;
    outfile << "labelloc=\"t\";" << endl;
    outfile << "label=\"Genome Fitness: " << best_validation_mae * 100.0 << "% MAE\";" << endl;
    outfile << endl;

    outfile << "\tgraph [pad=\"0.01\", nodesep=\"0.05\", ranksep=\"0.9\"];" << endl;

    int32_t input_name_index = 0;
    outfile << "\t{" << endl;
    outfile << "\t\trank = source;" << endl;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->layer_type != INPUT_LAYER) {
            continue;
        }
        input_name_index++;
        if (nodes[i]->total_outputs == 0) {
            continue;
        }
        outfile << "\t\tnode" << nodes[i]->innovation_number << " [shape=box,color=green,label=\"input "
                << nodes[i]->innovation_number << "\\ndepth " << nodes[i]->depth;

        if (input_parameter_names.size() != 0) {
            outfile << "\\n" << input_parameter_names[input_name_index - 1];
        }

        outfile << "\"];" << endl;
    }
    outfile << "\t}" << endl;
    outfile << endl;

    int32_t output_count = 0;
    int32_t input_count = 0;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->layer_type == OUTPUT_LAYER) {
            output_count++;
        }
        if (nodes[i]->layer_type == INPUT_LAYER) {
            input_count++;
        }
    }

    int32_t output_name_index = 0;
    outfile << "\t{" << endl;
    outfile << "\t\trank = sink;" << endl;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->layer_type != OUTPUT_LAYER) {
            continue;
        }
        output_name_index++;
        outfile << "\t\tnode" << nodes[i]->get_innovation_number() << " [shape=box,color=blue,label=\"output "
                << nodes[i]->innovation_number << "\\ndepth " << nodes[i]->depth;

        if (output_parameter_names.size() != 0) {
            outfile << "\\n" << output_parameter_names[output_name_index - 1];
        }

        outfile << "\"];" << endl;
    }
    outfile << "\t}" << endl;
    outfile << endl;

    bool printed_first = false;

    if (input_count > 1) {
        for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
            if (nodes[i]->layer_type != INPUT_LAYER) {
                continue;
            }
            if (nodes[i]->total_outputs == 0) {
                continue;
            }

            if (!printed_first) {
                printed_first = true;
                outfile << "\tnode" << nodes[i]->get_innovation_number();
            } else {
                outfile << " -> node" << nodes[i]->get_innovation_number();
            }
        }
        outfile << " [style=invis];" << endl << endl;

        outfile << endl;
    }

    if (output_count > 1) {
        printed_first = false;
        for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
            if (nodes[i]->layer_type != OUTPUT_LAYER) {
                continue;
            }

            if (!printed_first) {
                printed_first = true;
                outfile << "\tnode" << nodes[i]->get_innovation_number();
            } else {
                outfile << " -> node" << nodes[i]->get_innovation_number();
            }
        }
        outfile << " [style=invis];" << endl << endl;
        outfile << endl;
    }

    // draw the hidden nodes
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->layer_type != HIDDEN_LAYER) {
            continue;
        }
        if (!nodes[i]->is_reachable()) {
            continue;
        }

        string color = "black";
        string node_type = NODE_TYPES[nodes[i]->node_type];

        outfile << "\t\tnode" << nodes[i]->get_innovation_number() << " [shape=box,color=" << color << ",label=\""
                << node_type << " node #" << nodes[i]->get_innovation_number() << "\\ndepth " << nodes[i]->depth
                << "\"];" << endl;
    }
    outfile << endl;

    // draw the enabled edges
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (!edges[i]->is_reachable()) {
            continue;
        }

        outfile << "\tnode" << edges[i]->get_input_node()->get_innovation_number() << " -> node"
                << edges[i]->get_output_node()->get_innovation_number() << " [color=\"#"
                << get_color(edges[i]->weight, false) << "\"]; /* weight: " << edges[i]->weight << " */" << endl;
    }
    outfile << endl;

    // draw the enabled recurrent edges
    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (!recurrent_edges[i]->is_reachable()) {
            continue;
        }

        outfile << "\tnode" << recurrent_edges[i]->get_input_node()->get_innovation_number() << " -> node"
                << recurrent_edges[i]->get_output_node()->get_innovation_number() << " [color=\"#"
                << get_color(recurrent_edges[i]->weight, true)
                << "\",style=dotted]; /* weight: " << recurrent_edges[i]->weight
                << ", recurrent_depth: " << recurrent_edges[i]->recurrent_depth << " */" << endl;
    }
    outfile << endl;

    outfile << "}" << endl;
    outfile.close();
}

void read_map(istream& in, map<string, double>& m) {
    int32_t map_size;
    in >> map_size;
    for (int32_t i = 0; i < map_size; i++) {
        string key;
        in >> key;
        double value;
        in >> value;

        m[key] = value;
    }
}

void write_map(ostream& out, map<string, double>& m) {
    out << m.size();

    for (auto iterator = m.begin(); iterator != m.end(); iterator++) {
        out << " " << iterator->first;
        out << " " << iterator->second;
    }
}

void read_map(istream& in, map<string, int32_t>& m) {
    int32_t map_size;
    in >> map_size;
    for (int32_t i = 0; i < map_size; i++) {
        string key;
        in >> key;
        int32_t value;
        in >> value;

        m[key] = value;
    }
}

void write_map(ostream& out, map<string, int32_t>& m) {
    out << m.size();
    for (auto iterator = m.begin(); iterator != m.end(); iterator++) {
        out << " " << iterator->first;
        out << " " << iterator->second;
    }
}

void write_binary_string(ostream& out, string s, string name) {
    int32_t n = (int32_t) s.size();
    Log::debug("writing %d %s characters '%s'\n", n, name.c_str(), s.c_str());
    out.write((char*) &n, sizeof(int32_t));
    if (n > 0) {
        out.write((char*) &s[0], sizeof(char) * s.size());
    }
}

void read_binary_string(istream& in, string& s, string name) {
    int32_t n;
    in.read((char*) &n, sizeof(int32_t));

    Log::debug("reading %d %s characters.\n", n, name.c_str());
    if (n > 0) {
        char* s_v = new char[n];
        in.read((char*) s_v, sizeof(char) * n);
        s.assign(s_v, s_v + n);
        delete[] s_v;
    } else {
        s.assign("");
    }

    Log::debug("read %d %s characters '%s'\n", n, name.c_str(), s.c_str());
}

RNN_Genome::RNN_Genome(string binary_filename) {
    ifstream bin_infile(binary_filename, ios::in | ios::binary);

    if (!bin_infile.good()) {
        Log::fatal("ERROR: could not open RNN genome file '%s' for reading.\n", binary_filename.c_str());
        exit(1);
    }

    read_from_stream(bin_infile);
    bin_infile.close();
}

RNN_Genome::RNN_Genome(char* array, int32_t length) {
    read_from_array(array, length);
}

RNN_Genome::RNN_Genome(istream& bin_infile) {
    read_from_stream(bin_infile);
}

void RNN_Genome::read_from_array(char* array, int32_t length) {
    string array_str;
    for (int32_t i = 0; i < length; i++) {
        array_str.push_back(array[i]);
    }

    istringstream iss(array_str);
    read_from_stream(iss);
}

RNN_Node_Interface* RNN_Genome::read_node_from_stream(istream& bin_istream) {
    int32_t innovation_number, layer_type, node_type;
    double depth;
    bool enabled;

    bin_istream.read((char*) &innovation_number, sizeof(int32_t));
    bin_istream.read((char*) &layer_type, sizeof(int32_t));
    bin_istream.read((char*) &node_type, sizeof(int32_t));
    bin_istream.read((char*) &depth, sizeof(double));
    bin_istream.read((char*) &enabled, sizeof(bool));

    string parameter_name;
    read_binary_string(bin_istream, parameter_name, "parameter_name");
    Log::debug(
        "NODE: %d %d %d %lf %d '%s'\n", innovation_number, layer_type, node_type, depth, enabled, parameter_name.c_str()
    );

    RNN_Node_Interface* node = nullptr;
    if (node_type == LSTM_NODE) {
        node = new LSTM_Node(innovation_number, layer_type, depth);
    } else if (node_type == DELTA_NODE) {
        node = new Delta_Node(innovation_number, layer_type, depth);
    } else if (node_type == GRU_NODE) {
        node = new GRU_Node(innovation_number, layer_type, depth);
    } else if (node_type == ENARC_NODE) {
        node = new ENARC_Node(innovation_number, layer_type, depth);
    } else if (node_type == ENAS_DAG_NODE) {
        node = new ENAS_DAG_Node(innovation_number, layer_type, depth);
    } else if (node_type == RANDOM_DAG_NODE) {
        node = new RANDOM_DAG_Node(innovation_number, layer_type, depth);
    } else if (node_type == MGU_NODE) {
        node = new MGU_Node(innovation_number, layer_type, depth);
    } else if (node_type == UGRNN_NODE) {
        node = new UGRNN_Node(innovation_number, layer_type, depth);
    } else if (node_type == SIMPLE_NODE || node_type == JORDAN_NODE || node_type == ELMAN_NODE || node_type == OUTPUT_NODE_GP || node_type == INPUT_NODE_GP) {
        if (layer_type == HIDDEN_LAYER) {
            node = new RNN_Node(innovation_number, layer_type, depth, node_type);
        } else {
            node = new RNN_Node(innovation_number, layer_type, depth, node_type, parameter_name);
        }
    } else if (node_type == DNAS_NODE) {
        int32_t n_nodes;
        bin_istream.read((char*) &n_nodes, sizeof(int32_t));

        int32_t counter;
        bin_istream.read((char*) &counter, sizeof(int32_t));
        vector<double> pi(n_nodes, 0.0);
        bin_istream.read((char*) &pi[0], sizeof(double) * n_nodes);

        vector<RNN_Node_Interface*> nodes(n_nodes, nullptr);
        for (int32_t i = 0; i < n_nodes; i++) {
            nodes[i] = RNN_Genome::read_node_from_stream(bin_istream);
        }

        DNASNode* dnas_node = new DNASNode(move(nodes), innovation_number, layer_type, depth, counter);
        dnas_node->set_pi(pi);
        node = (RNN_Node_Interface*) dnas_node;
    } else if (node_type == SIN_NODE) {
        node = new SIN_Node(innovation_number, layer_type, depth);
    } else if (node_type == SUM_NODE) {
        node = new SUM_Node(innovation_number, layer_type, depth);
    } else if (node_type == COS_NODE) {
        node = new COS_Node(innovation_number, layer_type, depth);
    } else if (node_type == TANH_NODE) {
        node = new TANH_Node(innovation_number, layer_type, depth);
    } else if (node_type == SIGMOID_NODE) {
        node = new SIGMOID_Node(innovation_number, layer_type, depth);
    } else if (node_type == INVERSE_NODE) {
        node = new INVERSE_Node(innovation_number, layer_type, depth);
    } else if (node_type == MULTIPLY_NODE) {
        node = new MULTIPLY_Node(innovation_number, layer_type, depth);
    } else if (node_type == SIN_NODE_GP) {
        node = new SIN_Node_GP(innovation_number, layer_type, depth);
    } else if (node_type == COS_NODE_GP) {
        node = new COS_Node_GP(innovation_number, layer_type, depth);
    } else if (node_type == TANH_NODE_GP) {
        node = new TANH_Node_GP(innovation_number, layer_type, depth);
    } else if (node_type == SIGMOID_NODE_GP) {
        node = new SIGMOID_Node_GP(innovation_number, layer_type, depth);
    } else if (node_type == INVERSE_NODE_GP) {
        node = new INVERSE_Node_GP(innovation_number, layer_type, depth);
    } else if (node_type == MULTIPLY_NODE_GP) {
        node = new MULTIPLY_Node_GP(innovation_number, layer_type, depth);
    } else if (node_type == SUM_NODE_GP) {
        node = new SUM_Node_GP(innovation_number, layer_type, depth);
    } else {
        Log::fatal("Error reading node from stream, unknown node_type: %d\n", node_type);
        exit(1);
    }

    node->enabled = enabled;
    return node;
}
void RNN_Genome::read_from_stream(istream& bin_istream) {
    Log::debug("READING GENOME FROM STREAM\n");

    bin_istream.read((char*) &generation_id, sizeof(int32_t));
    bin_istream.read((char*) &group_id, sizeof(int32_t));
    bin_istream.read((char*) &bp_iterations, sizeof(int32_t));
    bin_istream.read((char*) &genome_type, sizeof(int32_t));

    bin_istream.read((char*) &use_dropout, sizeof(bool));
    bin_istream.read((char*) &dropout_probability, sizeof(double));

    // WeightType weight_initialize = WeightType::NONE;
    // WeightType weight_inheritance = WeightType::NONE;
    // WeightType mutated_component_weight = WeightType::NONE;

    // bin_istream.read((char*) &weight_initialize, sizeof(int32_t));
    // bin_istream.read((char*) &weight_inheritance, sizeof(int32_t));
    // bin_istream.read((char*) &mutated_component_weight, sizeof(int32_t));

    // weight_rules = new WeightRules();
    // weight_rules->set_weight_initialize_method(weight_initialize);
    // weight_rules->set_weight_inheritance_method(weight_inheritance);
    // weight_rules->set_mutated_components_weight_method(mutated_component_weight);

    Log::debug("generation_id: %d\n", generation_id);
    Log::debug("bp_iterations: %d\n", bp_iterations);

    Log::debug("use_dropout: %d\n", use_dropout);
    Log::debug("dropout_probability: %lf\n", dropout_probability);

    // Log::debug(": %s\n", WEIGHT_TYPES_STRING[weight_initialize].c_str());
    // Log::debug("weight inheritance: %s\n", WEIGHT_TYPES_STRING[weight_inheritance].c_str());
    // Log::debug("new component weight: %s\n", WEIGHT_TYPES_STRING[mutated_component_weight].c_str());

    read_binary_string(bin_istream, log_filename, "log_filename");
    string generator_str;
    read_binary_string(bin_istream, generator_str, "generator");
    istringstream generator_iss(generator_str);
    generator_iss >> generator;

    string rng_0_1_str;
    read_binary_string(bin_istream, rng_0_1_str, "rng_0_1");
    // So for some reason this was serialized incorrectly for some genomes,
    // but the value should always be the same so we really don't need to de-serialize it anways and can just
    // assign it a constant value
    rng_0_1 = uniform_real_distribution<double>(0.0, 1.0);
    // Formerly:
    // istringstream rng_0_1_iss(rng_0_1_str);
    // rng_0_1_iss >> rng_0_1;



    bin_istream.read((char*) &best_validation_mse, sizeof(double));
    bin_istream.read((char*) &best_validation_mae, sizeof(double));

    int32_t n_initial_parameters;
    bin_istream.read((char*) &n_initial_parameters, sizeof(int32_t));
    Log::debug("reading %d initial parameters.\n", n_initial_parameters);
    double* initial_parameters_v = new double[n_initial_parameters];
    bin_istream.read((char*) initial_parameters_v, sizeof(double) * n_initial_parameters);
    initial_parameters.assign(initial_parameters_v, initial_parameters_v + n_initial_parameters);
    delete[] initial_parameters_v;

    int32_t n_best_parameters;
    bin_istream.read((char*) &n_best_parameters, sizeof(int32_t));
    Log::debug("reading %d best parameters.\n", n_best_parameters);
    if (n_best_parameters > 0) {  // ← ADD THIS CONDITION
        double* best_parameters_v = new double[n_best_parameters];
        bin_istream.read((char*) best_parameters_v, sizeof(double) * n_best_parameters);
        best_parameters.assign(best_parameters_v, best_parameters_v + n_best_parameters);
        delete[] best_parameters_v;
    } else {
        best_parameters.clear();  // ← Ensure it's empty when n_best_parameters is 0
    }

    input_parameter_names.clear();
    int32_t n_input_parameter_names;
    bin_istream.read((char*) &n_input_parameter_names, sizeof(int32_t));
    Log::debug("reading %d input parameter names.\n", n_input_parameter_names);
    for (int32_t i = 0; i < n_input_parameter_names; i++) {
        string input_parameter_name;
        read_binary_string(bin_istream, input_parameter_name, "input_parameter_names[" + std::to_string(i) + "]");
        input_parameter_names.push_back(input_parameter_name);
    }

    output_parameter_names.clear();
    int32_t n_output_parameter_names;
    bin_istream.read((char*) &n_output_parameter_names, sizeof(int32_t));
    Log::debug("reading %d output parameter names.\n", n_output_parameter_names);
    for (int32_t i = 0; i < n_output_parameter_names; i++) {
        string output_parameter_name;
        read_binary_string(bin_istream, output_parameter_name, "output_parameter_names[" + std::to_string(i) + "]");
        output_parameter_names.push_back(output_parameter_name);
    }

    int32_t n_nodes;
    bin_istream.read((char*) &n_nodes, sizeof(int32_t));
    Log::debug("reading %d nodes.\n", n_nodes);

    nodes.clear();
    for (int32_t i = 0; i < n_nodes; i++) {
        nodes.push_back(RNN_Genome::read_node_from_stream(bin_istream));
    }

    int32_t n_edges;
    bin_istream.read((char*) &n_edges, sizeof(int32_t));
    Log::debug("reading %d edges.\n", n_edges);

    edges.clear();
    for (int32_t i = 0; i < n_edges; i++) {
        int32_t innovation_number;
        int32_t input_innovation_number;
        int32_t output_innovation_number;
        bool enabled;

        bin_istream.read((char*) &innovation_number, sizeof(int32_t));
        bin_istream.read((char*) &input_innovation_number, sizeof(int32_t));
        bin_istream.read((char*) &output_innovation_number, sizeof(int32_t));
        bin_istream.read((char*) &enabled, sizeof(bool));

        Log::debug(
            "EDGE: %d %d %d %d\n", innovation_number, input_innovation_number, output_innovation_number, enabled
        );

        RNN_Edge* edge = new RNN_Edge(innovation_number, input_innovation_number, output_innovation_number, nodes);
        // innovation_list.push_back(innovation_number);
        edge->enabled = enabled;
        edges.push_back(edge);
    }

    int32_t n_recurrent_edges;
    bin_istream.read((char*) &n_recurrent_edges, sizeof(int32_t));
    Log::debug("reading %d recurrent_edges.\n", n_recurrent_edges);

    recurrent_edges.clear();
    for (int32_t i = 0; i < n_recurrent_edges; i++) {
        int32_t innovation_number;
        int32_t recurrent_depth;
        int32_t input_innovation_number;
        int32_t output_innovation_number;
        bool enabled;

        bin_istream.read((char*) &innovation_number, sizeof(int32_t));
        bin_istream.read((char*) &recurrent_depth, sizeof(int32_t));
        bin_istream.read((char*) &input_innovation_number, sizeof(int32_t));
        bin_istream.read((char*) &output_innovation_number, sizeof(int32_t));
        bin_istream.read((char*) &enabled, sizeof(bool));

        Log::debug(
            "RECURRENT EDGE: %d %d %d %d %d\n", innovation_number, recurrent_depth, input_innovation_number,
            output_innovation_number, enabled
        );

        RNN_Recurrent_Edge* recurrent_edge = new RNN_Recurrent_Edge(
            innovation_number, recurrent_depth, input_innovation_number, output_innovation_number, nodes
        );
        // innovation_list.push_back(innovation_number);
        recurrent_edge->enabled = enabled;
        recurrent_edges.push_back(recurrent_edge);
    }

    read_binary_string(bin_istream, normalize_type, "normalize_type");

    string normalize_mins_str;
    read_binary_string(bin_istream, normalize_mins_str, "normalize_mins");
    istringstream normalize_mins_iss(normalize_mins_str);
    read_map(normalize_mins_iss, normalize_mins);

    string normalize_maxs_str;
    read_binary_string(bin_istream, normalize_maxs_str, "normalize_maxs");
    istringstream normalize_maxs_iss(normalize_maxs_str);
    read_map(normalize_maxs_iss, normalize_maxs);

    string normalize_avgs_str;
    read_binary_string(bin_istream, normalize_avgs_str, "normalize_avgs");
    istringstream normalize_avgs_iss(normalize_avgs_str);
    read_map(normalize_avgs_iss, normalize_avgs);

    string normalize_std_devs_str;
    read_binary_string(bin_istream, normalize_std_devs_str, "normalize_std_devs");
    istringstream normalize_std_devs_iss(normalize_std_devs_str);
    read_map(normalize_std_devs_iss, normalize_std_devs);

    // Read training indices
    int32_t training_indices_size;
    bin_istream.read((char*) &training_indices_size, sizeof(int32_t));
    training_indices.clear();
    training_indices.resize(training_indices_size);
    for (int32_t i = 0; i < training_indices_size; i++) {
        bin_istream.read((char*) &training_indices[i], sizeof(int32_t));
    }

    assign_reachability();
}

void RNN_Genome::write_to_array(char** bytes, int32_t& length) {
    ostringstream oss;
    write_to_stream(oss);

    string bytes_str = oss.str();
    length = bytes_str.size();
    (*bytes) = (char*) malloc(length * sizeof(char));
    for (int32_t i = 0; i < length; i++) {
        (*bytes)[i] = bytes_str[i];
    }
}

void RNN_Genome::write_to_file(string bin_filename) {
    ofstream bin_outfile(bin_filename, ios::out | ios::binary);
    write_to_stream(bin_outfile);
    bin_outfile.close();
}

void RNN_Genome::write_to_stream(ostream& bin_ostream) {

    bin_ostream.write((char*) &generation_id, sizeof(int32_t));
    bin_ostream.write((char*) &group_id, sizeof(int32_t));
    bin_ostream.write((char*) &bp_iterations, sizeof(int32_t));
    bin_ostream.write((char*) &genome_type, sizeof(int32_t));

    bin_ostream.write((char*) &use_dropout, sizeof(bool));
    bin_ostream.write((char*) &dropout_probability, sizeof(double));

    // WeightType weight_initialize = weight_rules->get_weight_initialize_method();
    // WeightType weight_inheritance = weight_rules->get_weight_inheritance_method();
    // WeightType mutated_component_weight = weight_rules->get_mutated_components_weight_method();
    // bin_ostream.write((char*) &weight_initialize, sizeof(int32_t));
    // bin_ostream.write((char*) &weight_inheritance, sizeof(int32_t));
    // bin_ostream.write((char*) &mutated_component_weight, sizeof(int32_t));

    // Log::info("generation_id: %d\n", generation_id);
    // Log::info("bp_iterations: %d\n", bp_iterations);

    // Log::info("use_dropout: %d\n", use_dropout);
    // Log::info("dropout_probability: %lf\n", dropout_probability);

    // Log::info("weight initialize: %s\n", WEIGHT_TYPES_STRING[weight_initialize].c_str());
    // Log::info("weight inheritance: %s\n", WEIGHT_TYPES_STRING[weight_inheritance].c_str());
    // Log::info("new component weight: %s\n", WEIGHT_TYPES_STRING[mutated_component_weight].c_str());

    write_binary_string(bin_ostream, log_filename, "log_filename");

    ostringstream generator_oss;
    generator_oss << generator;
    string generator_str = generator_oss.str();
    write_binary_string(bin_ostream, generator_str, "generator");

    ostringstream rng_0_1_oss;
    rng_0_1_oss << rng_0_1;
    string rng_0_1_str = rng_0_1_oss.str();
    write_binary_string(bin_ostream, rng_0_1_str, "rng_0_1");



    bin_ostream.write((char*) &best_validation_mse, sizeof(double));
    bin_ostream.write((char*) &best_validation_mae, sizeof(double));

    int32_t n_initial_parameters = (int32_t) initial_parameters.size();
    Log::debug("writing %d initial parameters.\n", n_initial_parameters);
    bin_ostream.write((char*) &n_initial_parameters, sizeof(int32_t));
    bin_ostream.write((char*) &initial_parameters[0], sizeof(double) * initial_parameters.size());

    int32_t n_best_parameters = (int32_t) best_parameters.size();
    bin_ostream.write((char*) &n_best_parameters, sizeof(int32_t));
    if (n_best_parameters) {
        bin_ostream.write((char*) &best_parameters[0], sizeof(double) * best_parameters.size());
    }

    int32_t n_input_parameter_names = (int32_t) input_parameter_names.size();
    bin_ostream.write((char*) &n_input_parameter_names, sizeof(int32_t));
    for (int32_t i = 0; i < (int32_t) input_parameter_names.size(); i++) {
        write_binary_string(bin_ostream, input_parameter_names[i], "input_parameter_names[" + std::to_string(i) + "]");
    }

    int32_t n_output_parameter_names = (int32_t) output_parameter_names.size();
    bin_ostream.write((char*) &n_output_parameter_names, sizeof(int32_t));
    for (int32_t i = 0; i < (int32_t) output_parameter_names.size(); i++) {
        write_binary_string(
            bin_ostream, output_parameter_names[i], "output_parameter_names[" + std::to_string(i) + "]"
        );
    }

    int32_t n_nodes = (int32_t) nodes.size();
    bin_ostream.write((char*) &n_nodes, sizeof(int32_t));
    Log::debug("writing %d nodes.\n", n_nodes);

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        Log::debug(
            "NODE: %d %d %d %lf '%s'\n", nodes[i]->innovation_number, nodes[i]->layer_type, nodes[i]->node_type,
            nodes[i]->depth, nodes[i]->parameter_name.c_str()
        );
        nodes[i]->write_to_stream(bin_ostream);
    }

    int32_t n_edges = (int32_t) edges.size();
    bin_ostream.write((char*) &n_edges, sizeof(int32_t));
    Log::debug("writing %d edges.\n", n_edges);

    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        Log::debug(
            "EDGE: %d %d %d\n", edges[i]->innovation_number, edges[i]->input_innovation_number,
            edges[i]->output_innovation_number
        );
        edges[i]->write_to_stream(bin_ostream);
    }

    int32_t n_recurrent_edges = (int32_t) recurrent_edges.size();
    bin_ostream.write((char*) &n_recurrent_edges, sizeof(int32_t));
    Log::debug("writing %d recurrent edges.\n", n_recurrent_edges);

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        Log::debug(
            "RECURRENT EDGE: %d %d %d %d\n", recurrent_edges[i]->innovation_number, recurrent_edges[i]->recurrent_depth,
            recurrent_edges[i]->input_innovation_number, recurrent_edges[i]->output_innovation_number
        );

        recurrent_edges[i]->write_to_stream(bin_ostream);
    }

    write_binary_string(bin_ostream, normalize_type, "normalize_type");

    ostringstream normalize_mins_oss;
    write_map(normalize_mins_oss, normalize_mins);
    string normalize_mins_str = normalize_mins_oss.str();
    write_binary_string(bin_ostream, normalize_mins_str, "normalize_mins");

    ostringstream normalize_maxs_oss;
    write_map(normalize_maxs_oss, normalize_maxs);
    string normalize_maxs_str = normalize_maxs_oss.str();
    write_binary_string(bin_ostream, normalize_maxs_str, "normalize_maxs");

    ostringstream normalize_avgs_oss;
    write_map(normalize_avgs_oss, normalize_avgs);
    string normalize_avgs_str = normalize_avgs_oss.str();
    write_binary_string(bin_ostream, normalize_avgs_str, "normalize_avgs");

    ostringstream normalize_std_devs_oss;
    write_map(normalize_std_devs_oss, normalize_std_devs);
    string normalize_std_devs_str = normalize_std_devs_oss.str();
    write_binary_string(bin_ostream, normalize_std_devs_str, "normalize_std_devs");

    // Write training indices
    int32_t training_indices_size = training_indices.size();
    bin_ostream.write((char*) &training_indices_size, sizeof(int32_t));
    for (int32_t i = 0; i < training_indices_size; i++) {
        bin_ostream.write((char*) &training_indices[i], sizeof(int32_t));
    }
}

void RNN_Genome::update_innovation_counts(int32_t& node_innovation_count, int32_t& edge_innovation_count) {
    int32_t max_node_innovation_count = -1;

    for (int32_t i = 0; i < (int32_t) this->nodes.size(); i += 1) {
        RNN_Node_Interface* node = this->nodes[i];
        max_node_innovation_count = std::max(max_node_innovation_count, node->innovation_number);
    }

    int32_t max_edge_innovation_count = -1;
    for (int32_t i = 0; i < (int32_t) this->edges.size(); i += 1) {
        RNN_Edge* edge = this->edges[i];
        max_edge_innovation_count = std::max(max_edge_innovation_count, edge->innovation_number);
    }
    for (int32_t i = 0; i < (int32_t) this->recurrent_edges.size(); i += 1) {
        RNN_Recurrent_Edge* redge = this->recurrent_edges[i];
        max_edge_innovation_count = std::max(max_edge_innovation_count, redge->innovation_number);
    }

    if (max_node_innovation_count == -1) {
        // Fatal log message
        Log::fatal(
            "Seed genome had max node innovation number of -1 - this should never happen (unless the genome is empty "
            ":)"
        );
    }
    if (max_edge_innovation_count == -1) {
        // Fatal log message
        Log::fatal(
            "Seed genome had max node innovation number of -1 - this should never happen (and the genome isn't empty "
            "since max_node_innovation_count > -1)"
        );
    }

    // One more than the highest we've seen should be good enough.
    node_innovation_count = max_node_innovation_count + 1;
    edge_innovation_count = max_edge_innovation_count + 1;
}
// return sorted innovation list
vector<int32_t> RNN_Genome::get_innovation_list() {
    vector<int32_t> innovations;
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        int32_t innovation = edges[i]->get_innovation_number();
        auto it = std::upper_bound(innovations.begin(), innovations.end(), innovation);
        innovations.insert(it, innovation);
    }
    return innovations;
}

string RNN_Genome::get_structural_hash() const {
    return structural_hash;
}

int32_t RNN_Genome::get_max_node_innovation_count() {
    int32_t max = 0;

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->innovation_number > max) {
            max = nodes[i]->innovation_number;
        }
    }

    return max;
}

int32_t RNN_Genome::get_max_edge_innovation_count() {
    int32_t max = 0;

    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->innovation_number > max) {
            max = edges[i]->innovation_number;
        }
    }

    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->innovation_number > max) {
            max = recurrent_edges[i]->innovation_number;
        }
    }

    return max;
}

void RNN_Genome::transfer_to(
    const vector<string>& new_input_parameter_names, const vector<string>& new_output_parameter_names,
    string transfer_learning_version, bool epigenetic_weights, int32_t min_recurrent_depth, int32_t max_recurrent_depth,
    WeightRules* weight_rules
) {
    Log::info("DOING TRANSFER OF GENOME!\n");

    double mu, sigma;
    set_weights(best_parameters);
    get_mu_sigma(best_parameters, mu, sigma);
    Log::info("before transfer, mu: %lf, sigma: %lf\n", mu, sigma);
    // make sure we don't duplicate new node/edge innovation numbers

    int32_t node_innovation_count = get_max_node_innovation_count() + 1;
    int32_t edge_innovation_count = get_max_edge_innovation_count() + 1;

    vector<RNN_Node_Interface*> input_nodes;
    vector<RNN_Node_Interface*> output_nodes;

    // work backwards so we don't skip removing anything
    for (int32_t i = (int32_t) nodes.size() - 1; i >= 0; i--) {
        Log::info("checking node: %d\n", i);

        // add all the input and output nodes to the input_nodes and output_nodes vectors,
        // and remove them from the node vector for the time being
        RNN_Node_Interface* node = nodes[i];
        if (node->layer_type == INPUT_LAYER) {
            input_nodes.push_back(node);
            Log::info("erasing node: %d of %d\n", i, nodes.size());
            nodes.erase(nodes.begin() + i);
            Log::info("input node with parameter name: '%s'\n", node->parameter_name.c_str());

        } else if (node->layer_type == OUTPUT_LAYER) {
            output_nodes.push_back(node);
            Log::info("erasing node: %d of %d\n", i, nodes.size());
            nodes.erase(nodes.begin() + i);
            Log::info("output node with parameter name: '%s'\n", node->parameter_name.c_str());
        }
    }

    Log::info("original input parameter names:\n");
    for (int32_t i = 0; i < (int32_t) input_parameter_names.size(); i++) {
        Log::info_no_header(" %s", input_parameter_names[i].c_str());
    }
    Log::info_no_header("\n");

    Log::info("new input parameter names:\n");
    for (int32_t i = 0; i < (int32_t) new_input_parameter_names.size(); i++) {
        Log::info_no_header(" %s", new_input_parameter_names[i].c_str());
    }
    Log::info_no_header("\n");

    // first figure out which input nodes we're keeping, and add new input
    // nodes as needed
    vector<RNN_Node_Interface*> new_input_nodes;
    vector<bool> new_inputs;  // this will track if new input node was new (true) or added from the genome (false)

    for (int32_t i = 0; i < (int32_t) new_input_parameter_names.size(); i++) {
        int32_t parameter_position = -1;
        // iterate through the input parameter names to find the input
        // node related to this new input paramter name, if it is
        // not found we need to make a new node for it
        for (int32_t j = 0; j < (int32_t) input_nodes.size(); j++) {
            if (input_nodes[j]->parameter_name.compare(new_input_parameter_names[i]) == 0) {
                parameter_position = j;
                break;
            }
        }

        if (parameter_position >= 0) {
            Log::info("keeping input node for parameter '%s'\n", new_input_parameter_names[i].c_str());
            // this input node already existed in the genome
            new_input_nodes.push_back(input_nodes[parameter_position]);
            new_inputs.push_back(false);

            // erase this parameter from the input nodes and input parameter names so we don't
            // re-use it
            input_nodes.erase(input_nodes.begin() + parameter_position);
        } else {
            Log::info("creating new input node for parameter '%s'\n", new_input_parameter_names[i].c_str());
            // create a new input node for this parameter
            new_inputs.push_back(true);
            RNN_Node* node = new RNN_Node(
                ++node_innovation_count, INPUT_LAYER, 0.0 /*input nodes should be depth 0*/, SIMPLE_NODE,
                new_input_parameter_names[i]
            );
            new_input_nodes.push_back(node);
        }
    }

    Log::info("new input node parameter names (should be the same as new input parameter names):\n");
    for (int32_t i = 0; i < (int32_t) new_input_nodes.size(); i++) {
        Log::info("\t%s (new: %s)\n", new_input_nodes[i]->parameter_name.c_str(), new_inputs[i] ? "true" : "false");
    }

    // delete all the input nodes that were not kept in the transfer process
    for (int32_t i = (int32_t) input_nodes.size() - 1; i >= 0; i--) {
        Log::info(
            "deleting outgoing edges for input node[%d] with parameter name: '%s' and innovation number %d\n", i,
            input_nodes[i]->parameter_name.c_str(), input_nodes[i]->innovation_number
        );

        // first delete any outgoing edges from the input node to be deleted
        for (int32_t j = (int32_t) edges.size() - 1; j >= 0; j--) {
            if (edges[j]->input_innovation_number == input_nodes[i]->innovation_number) {
                Log::info(
                    "deleting edges[%d] with innovation number: %d and input_innovation_number %d\n", j,
                    edges[j]->innovation_number, edges[j]->input_innovation_number
                );
                delete edges[j];
                edges.erase(edges.begin() + j);
            }
        }

        Log::info("deleting recurrent edges\n");

        // do the same for any outgoing recurrent edges
        for (int32_t j = (int32_t) recurrent_edges.size() - 1; j >= 0; j--) {
            // recurrent edges shouldn't go into input nodes, but check to see if it has a connection either way to the
            // node being deleted
            if (recurrent_edges[j]->input_innovation_number == input_nodes[i]->innovation_number
                || recurrent_edges[j]->output_innovation_number == input_nodes[i]->innovation_number) {
                Log::info(
                    "deleting recurrent_edges[%d] with innovation number: %d and input_innovation_number %d\n", j,
                    recurrent_edges[j]->innovation_number, recurrent_edges[j]->input_innovation_number
                );
                delete recurrent_edges[j];
                recurrent_edges.erase(recurrent_edges.begin() + j);
            }
        }

        delete input_nodes[i];
        input_nodes.erase(input_nodes.begin() + i);
    }

    Log::info("original output parameter names:\n");
    for (int32_t i = 0; i < (int32_t) output_parameter_names.size(); i++) {
        Log::info_no_header(" %s", output_parameter_names[i].c_str());
    }
    Log::info_no_header("\n");

    Log::info("new output parameter names:\n");
    for (int32_t i = 0; i < (int32_t) new_output_parameter_names.size(); i++) {
        Log::info_no_header(" %s", new_output_parameter_names[i].c_str());
    }
    Log::info_no_header("\n");

    // first figure out which output nodes we're keeping, and add new output
    // nodes as needed
    vector<RNN_Node_Interface*> new_output_nodes;
    vector<bool> new_outputs;  // this will track if new output node was new (true) or added from the genome (false)

    for (int32_t i = 0; i < (int32_t) new_output_parameter_names.size(); i++) {
        Log::info("finding output node with parameter name: '%s\n", new_output_parameter_names[i].c_str());

        int32_t parameter_position = -1;
        // iterate through the output parameter names to find the output
        // node related to this new output paramter name, if it is
        // not found we need to make a new node for it
        for (int32_t j = 0; j < (int32_t) output_nodes.size(); j++) {
            Log::info(
                "\tchecking output_nodes[%d]->parameter_name: '%s'\n", j, output_nodes[j]->parameter_name.c_str()
            );
            if (output_nodes[j]->parameter_name.compare(new_output_parameter_names[i]) == 0) {
                Log::info("\t\tMATCH!\n");
                parameter_position = j;
                break;
            }
        }

        if (parameter_position >= 0) {
            Log::info("keeping output node for parameter '%s'\n", new_output_parameter_names[i].c_str());
            // this output node already existed in the genome
            new_output_nodes.push_back(output_nodes[parameter_position]);
            new_outputs.push_back(false);

            // erase this parameter from the output nodes and output parameter names so we don't
            // re-use it
            output_nodes.erase(output_nodes.begin() + parameter_position);
        } else {
            Log::info("creating new output node for parameter '%s'\n", new_output_parameter_names[i].c_str());
            // create a new output node for this parameter
            new_outputs.push_back(true);
            RNN_Node* node = new RNN_Node(
                ++node_innovation_count, OUTPUT_LAYER, 1.0 /*output nodes should be depth 1*/, SIMPLE_NODE,
                new_output_parameter_names[i]
            );
            new_output_nodes.push_back(node);
        }
    }

    Log::info("new output node parameter names (should be the same as new output parameter names):\n");
    for (int32_t i = 0; i < (int32_t) new_output_nodes.size(); i++) {
        Log::info("\t%s (new: %s)\n", new_output_nodes[i]->parameter_name.c_str(), new_outputs[i] ? "true" : "false");
    }

    // delete all the output nodes that were not kept in the transfer process
    for (int32_t i = (int32_t) output_nodes.size() - 1; i >= 0; i--) {
        Log::info(
            "deleting incoming edges for output node[%d] with parameter name: '%s' and innovation number %d\n", i,
            output_nodes[i]->parameter_name.c_str(), output_nodes[i]->innovation_number
        );

        // first delete any incoming edges to the output node to be deleted
        for (int32_t j = (int32_t) edges.size() - 1; j >= 0; j--) {
            if (edges[j]->output_innovation_number == output_nodes[i]->innovation_number) {
                Log::info(
                    "deleting edges[%d] with innovation number: %d and output_innovation_number %d\n", j,
                    edges[j]->innovation_number, edges[j]->output_innovation_number
                );
                delete edges[j];
                edges.erase(edges.begin() + j);
            }
        }

        Log::info("doing recurrent edges\n");

        // do the same for any outgoing recurrent edges
        for (int32_t j = (int32_t) recurrent_edges.size() - 1; j >= 0; j--) {
            // output nodes can be the input to a recurrent edge so we need to delete those recurrent edges too if the
            // output node is being deleted
            if (recurrent_edges[j]->output_innovation_number == output_nodes[i]->innovation_number
                || recurrent_edges[j]->input_innovation_number == output_nodes[i]->innovation_number) {
                Log::info(
                    "deleting recurrent_edges[%d] with innovation number: %d and output_innovation_number %d\n", j,
                    recurrent_edges[j]->innovation_number, recurrent_edges[j]->output_innovation_number
                );
                delete recurrent_edges[j];
                recurrent_edges.erase(recurrent_edges.begin() + j);
            }
        }

        Log::info("deleting output_nodes[%d]\n", i);

        delete output_nodes[i];
        output_nodes.erase(output_nodes.begin() + i);
    }

    /* TRANSFER LEARNING VERSIONS:
        - V1: All new inputs to all outputs, all new outputs to all inputs
        - V2: new inputs and new outputs to random hidden
        - V3: new inputs and new outputs to all hidden nodes
    */

    Log::info("starting transfer learning versions\n");

    if (transfer_learning_version.compare("v1") != 0 && transfer_learning_version.compare("v2") != 0
        && transfer_learning_version.compare("v3") != 0 && transfer_learning_version.compare("v1+v2") != 0
        && transfer_learning_version.compare("v1+v3") != 0) {
        Log::fatal(
            "ERROR: unknown transfer learning version specified, '%s', options are:\n",
            transfer_learning_version.c_str()
        );
        Log::fatal("v1: connects all new inputs to all outputs and all new outputs to all inputs\n");
        Log::fatal("v2: randomly connects all new inputs and outputs to hidden nodes\n");
        Log::fatal("v3: connects all new inputs and outputs to hidden nodes\n");
        Log::fatal("v1+v2: does both v1 and v2\n");
        Log::fatal("v1+v3: does both v1 and v3\n");

        exit(1);
    }

    sort_edges_by_depth();
    sort_recurrent_edges_by_depth();

    if (transfer_learning_version.compare("v1") == 0 || transfer_learning_version.compare("v1+v2") == 0) {
        Log::info("doing transfer v1\n");
        for (int32_t i = 0; i < (int32_t) new_input_nodes.size(); i++) {
            if (!new_inputs[i]) {
                continue;
            }
            Log::info(
                "adding connections for new input node[%d] '%s'\n", i, new_input_nodes[i]->parameter_name.c_str()
            );

            for (int32_t j = 0; j < (int32_t) new_output_nodes.size(); j++) {
                attempt_edge_insert(new_input_nodes[i], new_output_nodes[j], mu, sigma, edge_innovation_count, weight_rules);
            }
        }

        for (int32_t i = 0; i < (int32_t) new_output_nodes.size(); i++) {
            if (!new_outputs[i]) {
                continue;
            }
            Log::info(
                "adding connections for new output node[%d] '%s'\n", i, new_output_nodes[i]->parameter_name.c_str()
            );

            for (int32_t j = 0; j < (int32_t) new_input_nodes.size(); j++) {
                attempt_edge_insert(new_input_nodes[j], new_output_nodes[i], mu, sigma, edge_innovation_count, weight_rules);
            }
        }
    }

    sort_edges_by_depth();
    sort_recurrent_edges_by_depth();

    uniform_int_distribution<int32_t> rec_depth_dist(min_recurrent_depth, max_recurrent_depth);
    if (transfer_learning_version.compare("v2") == 0 || transfer_learning_version.compare("v1+v2") == 0) {
        Log::info("doing transfer v2\n");
        bool not_all_hidden = true;
        for (auto node : new_input_nodes) {
            Log::debug("BEFORE -- CHECK EDGE INNOVATION COUNT: %d\n", edge_innovation_count);
            connect_new_input_node(mu, sigma, node, rec_depth_dist, edge_innovation_count, not_all_hidden, weight_rules);
            Log::debug("AFTER -- CHECK EDGE INNOVATION COUNT: %d\n", edge_innovation_count);
        }

        for (auto node : new_output_nodes) {
            Log::debug("BEFORE -- CHECK EDGE INNOVATION COUNT: %d\n", edge_innovation_count);
            connect_new_output_node(mu, sigma, node, rec_depth_dist, edge_innovation_count, not_all_hidden, weight_rules);
            Log::debug("AFTER -- CHECK EDGE INNOVATION COUNT: %d\n", edge_innovation_count);
        }
    }
    if (transfer_learning_version.compare("v3") == 0 || transfer_learning_version.compare("v1+v3") == 0) {
        Log::info("doing transfer v3\n");
        bool not_all_hidden = false;
        for (auto node : new_input_nodes) {
            Log::debug("BEFORE -- CHECK EDGE INNOVATION COUNT: %d\n", edge_innovation_count);
            connect_new_input_node(mu, sigma, node, rec_depth_dist, edge_innovation_count, not_all_hidden, weight_rules);
            Log::debug("AFTER -- CHECK EDGE INNOVATION COUNT: %d\n", edge_innovation_count);
        }

        for (auto node : new_output_nodes) {
            Log::debug("BEFORE -- CHECK EDGE INNOVATION COUNT: %d\n", edge_innovation_count);
            connect_new_output_node(mu, sigma, node, rec_depth_dist, edge_innovation_count, not_all_hidden, weight_rules);
            Log::debug("AFTER -- CHECK EDGE INNOVATION COUNT: %d\n", edge_innovation_count);
        }
    }

    Log::info("adding new_input_nodes and new_output_nodes to nodes\n");
    // add the new input and new output nodes back into the genome's node vector
    nodes.insert(nodes.begin(), new_input_nodes.begin(), new_input_nodes.end());
    nodes.insert(nodes.end(), new_output_nodes.begin(), new_output_nodes.end());

    Log::info("assigning reachability\n");
    // need to recalculate the reachability of each node
    assign_reachability();

    sort_edges_by_depth();
    sort_recurrent_edges_by_depth();

    // need to make sure that each input and each output has at least one connection
    for (auto node : nodes) {
        Log::info(
            "node[%d], depth: %lf, total_inputs: %d, total_outputs: %d\n", node->get_innovation_number(),
            node->get_depth(), node->get_total_inputs(), node->get_total_outputs()
        );

        if (node->get_layer_type() == INPUT_LAYER) {
            if (node->get_total_outputs() == 0) {
                Log::info("input node[%d] had no outputs, connecting it!\n", node->get_innovation_number());
                // if an input has no outgoing edges randomly connect it
                connect_new_input_node(mu, sigma, node, rec_depth_dist, edge_innovation_count, true, weight_rules);
            }

        } else if (node->get_layer_type() == OUTPUT_LAYER) {
            if (node->get_total_inputs() == 0) {
                Log::info("output node[%d] had no inputs, connecting it!\n", node->get_innovation_number());
                // if an output has no incoming edges randomly connect it
                connect_new_output_node(mu, sigma, node, rec_depth_dist, edge_innovation_count, true, weight_rules);
            }
        }
    }

    Log::info("assigning reachability again\n");
    // update the reachabaility again
    assign_reachability();

    Log::info("new_parameters.size() before get weights: %d\n", initial_parameters.size());

    // update the new and best parameter lengths because this will have added edges
    vector<double> updated_genome_parameters;
    get_weights(updated_genome_parameters);
    if (!epigenetic_weights) {
        Log::info("resetting genome parameters to randomly betwen -0.5 and 0.5\n");
        for (int32_t i = 0; i < (int32_t) updated_genome_parameters.size(); i++) {
            updated_genome_parameters[i] = rng_0_1(generator) - 0.5;
        }
    } else {
        Log::info("not resetting weights\n");
    }
    set_initial_parameters(updated_genome_parameters);
    set_best_parameters(updated_genome_parameters);

    best_validation_mse = EXAMM_MAX_DOUBLE;
    best_validation_mae = EXAMM_MAX_DOUBLE;

    get_mu_sigma(best_parameters, mu, sigma);
    Log::info("after transfer, mu: %lf, sigma: %lf\n", mu, sigma);
    // make sure we don't duplicate new node/edge innovation numbers

    Log::info("new_parameters.size() after get weights: %d\n", updated_genome_parameters.size());

    Log::info("FINISHING PREPARING INITIAL GENOME\n");
}

void RNN_Genome::set_stochastic(bool stochastic) {
    for (RNN_Node_Interface* n : nodes) {
        if (DNASNode* node = dynamic_cast<DNASNode*>(n)) {
            if (node != nullptr) {
                node->set_stochastic(stochastic);
            }
        }
    }
}

void RNN_Genome::print_equations() {
    write_equations(cout);
}

void RNN_Genome::write_equations(ostream& outstream) {
    this->set_weights(best_parameters);
    sort_nodes_by_depth();
    unordered_map<int32_t, string> innovation_to_label;
    unordered_map<int32_t, string> innovation_to_equation;
    unordered_map<int32_t, int32_t> innovation_to_inputs_fired;
    int32_t count = 0;
    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->layer_type == HIDDEN_LAYER && nodes[i]->is_reachable()) {
            string label = "H" + to_string(count);
            innovation_to_label.emplace(nodes[i]->innovation_number, label);
            count++;
        } else if (nodes[i]->is_reachable()) {
            innovation_to_label.emplace(nodes[i]->innovation_number, nodes[i]->parameter_name);
        }
        innovation_to_equation.emplace(nodes[i]->innovation_number, "");
        innovation_to_inputs_fired.emplace(nodes[i]->innovation_number, 0);
    }

    sort_edges_by_depth();
    for (int32_t i = 0; i < (int32_t) edges.size(); i++) {
        if (edges[i]->is_reachable()) {
            RNN_Node_Interface* input_node = edges[i]->input_node;
            RNN_Node_Interface* output_node = edges[i]->output_node;
            vector<double> output_bias_vector;
            output_node->get_weights(output_bias_vector);
            double output_bias = output_bias_vector[0];

            string input_equation;
            if (edges[i]->weight == 1) {
                input_equation = innovation_to_label[input_node->innovation_number];
            } else if (edges[i]->weight == 0.0) {
                input_equation = "";
            } else {
                input_equation = "(" + innovation_to_label[input_node->innovation_number] + " * "
                                 + to_string(round(edges[i]->weight * pow(10, 6)) / pow(10, 6)) + ")";
            }
            innovation_to_inputs_fired[output_node->innovation_number]++;
            string current_output_equation = innovation_to_equation[output_node->innovation_number];
            if (innovation_to_inputs_fired[output_node->innovation_number] == 1) {
                if (output_node->node_type == SIMPLE_NODE || output_node->node_type == JORDAN_NODE
                    || output_node->node_type == ELMAN_NODE) {
                    current_output_equation += "tanh(" + input_equation;
                } else if (output_node->node_type == SIN_NODE || output_node->node_type == SIN_NODE_GP) {
                    current_output_equation += "sin(" + input_equation;
                } else if (output_node->node_type == COS_NODE || output_node->node_type == COS_NODE_GP) {
                    current_output_equation += "cos(" + input_equation;
                } else if (output_node->node_type == TANH_NODE || output_node->node_type == TANH_NODE_GP) {
                    current_output_equation += "tanh(" + input_equation;
                } else if (output_node->node_type == SIGMOID_NODE || output_node->node_type == SIGMOID_NODE_GP) {
                    current_output_equation += "sigmoid(" + input_equation;
                } else if (output_node->node_type == SUM_NODE || output_node->node_type == SUM_NODE_GP || output_node->node_type == OUTPUT_NODE_GP) {
                    current_output_equation += input_equation;
                } else if (output_node->node_type == MULTIPLY_NODE || output_node->node_type == MULTIPLY_NODE_GP) {
                    current_output_equation += input_equation;
                } else if (output_node->node_type == INVERSE_NODE || output_node->node_type == INVERSE_NODE_GP) {
                    current_output_equation += "1.0 /(" + input_equation;
                } else if (output_node->node_type == UGRNN_NODE) {
                    current_output_equation += "ugrnn(" + input_equation;
                } else if (output_node->node_type == MGU_NODE) {
                    current_output_equation += "mgu(" + input_equation;
                } else if (output_node->node_type == GRU_NODE) {
                    current_output_equation += "gru(" + input_equation;
                } else if (output_node->node_type == DELTA_NODE) {
                    current_output_equation += "delta(" + input_equation;
                } else if (output_node->node_type == LSTM_NODE) {
                    current_output_equation += "lstm(" + input_equation;
                } else if (output_node->node_type == ENARC_NODE) {
                    current_output_equation += "enarc(" + input_equation;
                } else if (output_node->node_type == ENAS_DAG_NODE) {
                    current_output_equation += "enas(" + input_equation;
                } else if (output_node->node_type == RANDOM_DAG_NODE) {
                    current_output_equation += "rdag(" + input_equation;
                } else if (output_node->node_type == DNAS_NODE) {
                    current_output_equation += "dnas(" + input_equation;
                } else {
                    Log::fatal("ERROR: output_node not correct type\n");
                    exit(1);
                }
            } else if (innovation_to_inputs_fired[output_node->innovation_number] > 1
                       && innovation_to_inputs_fired[output_node->innovation_number] < output_node->total_inputs) {
                if (output_node->node_type == MULTIPLY_NODE || output_node->node_type == MULTIPLY_NODE_GP) {
                    current_output_equation += " * " + input_equation;
                } else if (edges[i]->weight == 0.0) {
                    current_output_equation += input_equation;
                } else {
                    current_output_equation += " + " + input_equation;
                }
            } else if (innovation_to_inputs_fired[output_node->innovation_number] == output_node->total_inputs) {
                if (output_bias == 0.0) {
                    if (output_node->node_type == MULTIPLY_NODE) {
                        current_output_equation = to_string(0.0);
                    } else if (output_node->node_type == MULTIPLY_NODE_GP) {
                        current_output_equation = to_string(0.0);
                    } else if (output_node->node_type == SUM_NODE || output_node->node_type == SUM_NODE_GP
                               || output_node->node_type == OUTPUT_NODE_GP) {
                        if (input_equation.compare("") != 0) {
                            current_output_equation += " + " + input_equation;
                        }
                    } else {
                        current_output_equation += " + " + input_equation + ")";
                    }
                } else {
                    if (output_node->node_type == MULTIPLY_NODE) {
                        current_output_equation +=
                            " * " + input_equation + " + " + to_string(round(output_bias * pow(10, 6)) / pow(10, 6));
                    } else if (output_node->node_type == MULTIPLY_NODE_GP) {
                        current_output_equation +=
                            " * " + input_equation + " * " + to_string(round(output_bias * pow(10, 6)) / pow(10, 6));
                    } else if (output_node->node_type == SUM_NODE || output_node->node_type == SUM_NODE_GP
                               || output_node->node_type == OUTPUT_NODE_GP) {
                        current_output_equation +=
                            " + " + input_equation + " + " + to_string(round(output_bias * pow(10, 6)) / pow(10, 6));
                    } else {
                        current_output_equation += " + " + input_equation + " + "
                                                   + to_string(round(output_bias * pow(10, 6)) / pow(10, 6)) + ")";
                    }
                }
            } else {
                Log::fatal("ERROR: inputs_fired not within bounds of total inputs\n");
                cout << "output: " << innovation_to_label[output_node->innovation_number] << endl;
                cout << "input: " << innovation_to_label[input_node->innovation_number] << endl;
                cout << "edge_weight: " << edges[i]->weight << endl;
                cout << "forward_reachable: " << output_node->forward_reachable << endl;
                Log::fatal(
                    "\ninputs_fired: %d \ntotal_inputs: %d\n",
                    innovation_to_inputs_fired[output_node->innovation_number], output_node->total_inputs
                );
                exit(1);
            }
            innovation_to_equation[output_node->innovation_number] = current_output_equation;
        }
    }

    sort_recurrent_edges_by_depth();
    for (int32_t i = 0; i < (int32_t) recurrent_edges.size(); i++) {
        if (recurrent_edges[i]->is_reachable()) {
            RNN_Node_Interface* input_node = recurrent_edges[i]->input_node;
            RNN_Node_Interface* output_node = recurrent_edges[i]->output_node;
            vector<double> output_bias_vector;
            output_node->get_weights(output_bias_vector);
            double output_bias = output_bias_vector[0];
            string input_equation = "";
            if (recurrent_edges[i]->weight == 1.0) {
                input_equation = innovation_to_label[input_node->innovation_number] + "(t - "
                                 + to_string(recurrent_edges[i]->recurrent_depth) + ")";
            } else if (recurrent_edges[i]->weight == 0.0) {
                input_equation = "";
            } else {
                input_equation = "(" + innovation_to_label[input_node->innovation_number] + "(t - "
                                 + to_string(recurrent_edges[i]->recurrent_depth) + ")" + " * "
                                 + to_string(round(recurrent_edges[i]->weight * pow(10, 6)) / pow(10, 6)) + ")";
            }
            innovation_to_inputs_fired[output_node->innovation_number]++;
            string current_output_equation = innovation_to_equation[output_node->innovation_number];
            if (innovation_to_inputs_fired[output_node->innovation_number] == 1) {
                if (output_node->node_type == SIMPLE_NODE || output_node->node_type == JORDAN_NODE
                    || output_node->node_type == ELMAN_NODE) {
                    current_output_equation += "tanh(" + input_equation;
                } else if (output_node->node_type == SIN_NODE || output_node->node_type == SIN_NODE_GP) {
                    current_output_equation += "sin(" + input_equation;
                } else if (output_node->node_type == COS_NODE || output_node->node_type == COS_NODE_GP) {
                    current_output_equation += "cos(" + input_equation;
                } else if (output_node->node_type == TANH_NODE || output_node->node_type == TANH_NODE_GP) {
                    current_output_equation += "tanh(" + input_equation;
                } else if (output_node->node_type == SIGMOID_NODE || output_node->node_type == SIGMOID_NODE_GP) {
                    current_output_equation += "sigmoid(" + input_equation;
                } else if (output_node->node_type == SUM_NODE || output_node->node_type == SUM_NODE_GP || output_node->node_type == OUTPUT_NODE_GP) {
                    current_output_equation += input_equation;
                } else if (output_node->node_type == MULTIPLY_NODE || output_node->node_type == MULTIPLY_NODE_GP) {
                    current_output_equation += input_equation;
                } else if (output_node->node_type == INVERSE_NODE || output_node->node_type == INVERSE_NODE_GP) {
                    current_output_equation += "1.0 /(" + input_equation;
                } else if (output_node->node_type == UGRNN_NODE) {
                    current_output_equation += "ugrnn(" + input_equation;
                } else if (output_node->node_type == MGU_NODE) {
                    current_output_equation += "mgu(" + input_equation;
                } else if (output_node->node_type == GRU_NODE) {
                    current_output_equation += "gru(" + input_equation;
                } else if (output_node->node_type == DELTA_NODE) {
                    current_output_equation += "delta(" + input_equation;
                } else if (output_node->node_type == LSTM_NODE) {
                    current_output_equation += "lstm(" + input_equation;
                } else if (output_node->node_type == ENARC_NODE) {
                    current_output_equation += "enarc(" + input_equation;
                } else if (output_node->node_type == ENAS_DAG_NODE) {
                    current_output_equation += "enas(" + input_equation;
                } else if (output_node->node_type == RANDOM_DAG_NODE) {
                    current_output_equation += "rdag(" + input_equation;
                } else if (output_node->node_type == DNAS_NODE) {
                    current_output_equation += "dnas(" + input_equation;
                } else {
                    Log::fatal("ERROR: output_node not correct type");
                    exit(1);
                }
            } else if (innovation_to_inputs_fired[output_node->innovation_number] > 1
                       && innovation_to_inputs_fired[output_node->innovation_number] < output_node->total_inputs) {
                if (output_node->node_type == MULTIPLY_NODE || output_node->node_type == MULTIPLY_NODE_GP) {
                    current_output_equation += " * " + input_equation;
                } else if (recurrent_edges[i]->weight == 0.0) {
                    current_output_equation += input_equation;
                } else {
                    current_output_equation += " + " + input_equation;
                }
            } else if (innovation_to_inputs_fired[output_node->innovation_number] == output_node->total_inputs) {
                if (output_bias == 0.0) {
                    if (output_node->node_type == MULTIPLY_NODE) {
                        current_output_equation = to_string(0.0);
                    } else if (output_node->node_type == MULTIPLY_NODE_GP) {
                        current_output_equation = to_string(0.0);
                    } else if (output_node->node_type == SUM_NODE || output_node->node_type == SUM_NODE_GP
                               || output_node->node_type == OUTPUT_NODE_GP) {
                        if (input_equation.compare("") != 0) {
                            current_output_equation += " + " + input_equation;
                        }
                    } else {
                        current_output_equation += " + " + input_equation + ")";
                    }
                } else {
                    if (output_node->node_type == MULTIPLY_NODE) {
                        current_output_equation +=
                            " * " + input_equation + " + " + to_string(round(output_bias * pow(10, 6)) / pow(10, 6));
                    } else if (output_node->node_type == MULTIPLY_NODE_GP) {
                        current_output_equation +=
                            " * " + input_equation + " * " + to_string(round(output_bias * pow(10, 6)) / pow(10, 6));
                    } else if (output_node->node_type == SUM_NODE || output_node->node_type == SUM_NODE_GP
                               || output_node->node_type == OUTPUT_NODE_GP) {
                        current_output_equation +=
                            " + " + input_equation + " + " + to_string(round(output_bias * pow(10, 6)) / pow(10, 6));
                    } else {
                        current_output_equation += " + " + input_equation + " + "
                                                   + to_string(round(output_bias * pow(10, 6)) / pow(10, 6)) + ")";
                    }
                }
            } else {
                Log::fatal("ERROR: inputs_fired not within bounds of total inputs \n");
                cout << "output: " << innovation_to_label[output_node->innovation_number] << endl;
                cout << "input: " << innovation_to_label[input_node->innovation_number] << endl;
                cout << "recurrent_depth: " << recurrent_edges[i]->recurrent_depth << endl;
                cout << "recurrent_edge_weight: " << recurrent_edges[i]->weight << endl;
                cout << "forward_reachable: " << output_node->forward_reachable << endl;
                Log::fatal(
                    "\ninputs_fired: %d \ntotal_inputs: %d\n",
                    innovation_to_inputs_fired[output_node->innovation_number], output_node->total_inputs
                );
                exit(1);
            }
            innovation_to_equation[output_node->innovation_number] = current_output_equation;
        }
    }

    for (int32_t i = 0; i < (int32_t) nodes.size(); i++) {
        if (nodes[i]->layer_type == HIDDEN_LAYER && nodes[i]->is_reachable()) {
            string output = innovation_to_label[nodes[i]->innovation_number] + " = "
                            + innovation_to_equation[nodes[i]->innovation_number];
            string to_search = "= + ";
            string replacement = "= ";
            size_t pos = 0;
            while ((pos = output.find(to_search, pos)) != string::npos) {
                output.replace(pos, to_search.length(), replacement);
                pos += replacement.length();
            }
            to_search = " + + ";
            replacement = " + ";
            pos = 0;
            while ((pos = output.find(to_search, pos)) != string::npos) {
                output.replace(pos, to_search.length(), replacement);
                pos += replacement.length();
            }
            outstream << output << endl;
            //  outstream << "total_connections: " << innovation_to_inputs_fired[nodes[i]->innovation_number] << endl;
            //  outstream << "total_inputs: " << nodes[i]->total_inputs << endl;
            //  outstream << "is_reachable: " << nodes[i]->is_reachable() << endl;
            outstream << endl;
        } else if (nodes[i]->layer_type == OUTPUT_LAYER && nodes[i]->is_reachable()) {
            string output = innovation_to_label[nodes[i]->innovation_number] + "(t + 1)" + " ="
                            + innovation_to_equation[nodes[i]->innovation_number];
            string to_search = "= + ";
            string replacement = "= ";
            size_t pos = 0;
            while ((pos = output.find(to_search, pos)) != string::npos) {
                output.replace(pos, to_search.length(), replacement);
                pos += replacement.length();
            }
            to_search = " + + ";
            replacement = " + ";
            pos = 0;
            while ((pos = output.find(to_search, pos)) != string::npos) {
                output.replace(pos, to_search.length(), replacement);
                pos += replacement.length();
            }
            outstream << output << endl;
            outstream << endl;
        }
    }
    outstream << "best_validation_mse: " << to_string(this->get_best_validation_mse()) << endl;
    outstream << endl;
}

// Training indices management for online learning
void RNN_Genome::set_training_indices(const vector<int32_t>& indices) {
    training_indices = indices;
}

vector<int32_t> RNN_Genome::get_training_indices() const {
    return training_indices;
}
