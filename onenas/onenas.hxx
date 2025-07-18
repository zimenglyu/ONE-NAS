#ifndef ONENAS_HXX
#define ONENAS_HXX

#include <fstream>
using std::ofstream;

#include <map>
using std::map;

#include <sstream>
using std::ostringstream;

#include <string>
using std::string;
using std::to_string;

#include <vector>
using std::vector;

#include "rnn/genome_property.hxx"
#include "rnn/rnn_genome.hxx"
#include "speciation_strategy.hxx"
#include "time_series/time_series.hxx"
#include "weights/weight_rules.hxx"

class ONENAS {
   private:
    // int32_t island_size;
    int32_t number_islands;

    // int32_t max_genomes;
    int32_t total_bp_epochs;

    // int32_t elite_population_size;
    // int32_t generated_population_size;

    SpeciationStrategy* speciation_strategy;
    WeightRules* weight_rules;
    GenomeProperty* genome_property;

    int32_t edge_innovation_count;
    int32_t node_innovation_count;



    bool generate_op_log;

    minstd_rand0 generator;
    uniform_real_distribution<double> rng_0_1;
    uniform_real_distribution<double> rng_crossover_weight;

    double more_fit_crossover_rate;
    double less_fit_crossover_rate;

    double clone_rate;

    double add_edge_rate;
    double add_recurrent_edge_rate;
    double enable_edge_rate;
    double disable_edge_rate;
    double split_edge_rate;

    double add_node_rate;
    double enable_node_rate;
    double disable_node_rate;
    double split_node_rate;
    double merge_node_rate;

    vector<int32_t> possible_node_types = {SIMPLE_NODE, JORDAN_NODE, ELMAN_NODE, UGRNN_NODE,
                                           MGU_NODE,    GRU_NODE,    DELTA_NODE, LSTM_NODE};



    string output_directory;
    ofstream* log_file;
    ofstream* op_log_file;

    std::chrono::time_point<std::chrono::system_clock> startClock;

    string genome_file_name;
    string save_genome_option;

   public:
    ONENAS(
        int32_t _number_islands, SpeciationStrategy* _speciation_strategy,
        WeightRules* _weight_rules, GenomeProperty* _genome_property, string _output_directory,
        string _save_genome_option
    );

    ~ONENAS();

    void print();
    void update_log();

    void set_possible_node_types(vector<string> possible_node_type_strings);

    uniform_int_distribution<int32_t> get_recurrent_depth_dist();

    int32_t get_random_node_type();

    RNN_Genome* generate_genome();
    bool insert_genome(RNN_Genome* genome);

    void mutate(int32_t max_mutations, RNN_Genome* p1);

    void attempt_node_insert(
        vector<RNN_Node_Interface*>& child_nodes, const RNN_Node_Interface* node, const vector<double>& new_weights
    );
    void attempt_edge_insert(
        vector<RNN_Edge*>& child_edges, vector<RNN_Node_Interface*>& child_nodes, RNN_Edge* edge, RNN_Edge* second_edge,
        bool set_enabled
    );
    void attempt_recurrent_edge_insert(
        vector<RNN_Recurrent_Edge*>& child_recurrent_edges, vector<RNN_Node_Interface*>& child_nodes,
        RNN_Recurrent_Edge* recurrent_edge, RNN_Recurrent_Edge* second_edge, bool set_enabled
    );
    RNN_Genome* crossover(RNN_Genome* p1, RNN_Genome* p2);

    double get_best_fitness();
    double get_worst_fitness();
    RNN_Genome* get_best_genome();
    RNN_Genome* get_worst_genome();

    SpeciationStrategy* get_speciation_strategy();

    void save_genome(RNN_Genome* genome, string genome_name);

    string get_output_directory() const;

    void check_weight_initialize_validity();
    void generate_log();
    void set_evolution_hyper_parameters();
    void initialize_seed_genome();
    void update_op_log_statistics(RNN_Genome* genome, int32_t insert_position);

    /**
     * Reduces add mutation rates to control network growth
     * \param factor the factor by which to reduce the rates (default: 0.5)
     */
    void reduce_add_mutation_rates(double factor = 0.5);

    void finalize_generation(int32_t current_generation, const vector< vector< vector<double> > > &validation_input, const vector< vector< vector<double> > > &validation_output, const vector< vector< vector<double> > > &test_input, const vector< vector< vector<double> > > &test_output);
};

#endif
