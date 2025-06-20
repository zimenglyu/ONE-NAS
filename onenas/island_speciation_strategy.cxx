#include <functional>
using std::function;

#include <chrono>

// #include <iostream>

#include <random>

using std::minstd_rand0;
using std::uniform_real_distribution;

#include <string>
using std::string;

#include "common/log.hxx"
#include "examm.hxx"
#include "island_speciation_strategy.hxx"
#include "rnn/rnn_genome.hxx"

/**
 *
 */
IslandSpeciationStrategy::IslandSpeciationStrategy(
    int32_t _number_of_islands, int32_t _max_island_size, double _mutation_rate, double _intra_island_crossover_rate,
    double _inter_island_crossover_rate, RNN_Genome* _seed_genome, string _island_ranking_method,
    string _repopulation_method, int32_t _extinction_event_generation_number, int32_t _num_mutations,
    int32_t _islands_to_exterminate, int32_t _max_genomes, bool _repeat_extinction, bool _start_filled,
    bool _transfer_learning, string _transfer_learning_version, int32_t _seed_stirs, bool _tl_epigenetic_weights
)
    : generation_island(0),
      number_of_islands(_number_of_islands),
      max_island_size(_max_island_size),
      mutation_rate(_mutation_rate),
      intra_island_crossover_rate(_intra_island_crossover_rate),
      inter_island_crossover_rate(_inter_island_crossover_rate),
      generated_genomes(0),
      evaluated_genomes(0),
      seed_genome(_seed_genome),
      island_ranking_method(_island_ranking_method),
      repopulation_method(_repopulation_method),
      extinction_event_generation_number(_extinction_event_generation_number),
      num_mutations(_num_mutations),
      islands_to_exterminate(_islands_to_exterminate),
      max_genomes(_max_genomes),
      repeat_extinction(_repeat_extinction),
      start_filled(_start_filled),
      transfer_learning(_transfer_learning),
      transfer_learning_version(_transfer_learning_version),
      seed_stirs(_seed_stirs),
      tl_epigenetic_weights(_tl_epigenetic_weights) {
    double rate_sum = mutation_rate + intra_island_crossover_rate + inter_island_crossover_rate;
    if (rate_sum != 1.0) {
        mutation_rate = mutation_rate / rate_sum;
        intra_island_crossover_rate = intra_island_crossover_rate / rate_sum;
        inter_island_crossover_rate = inter_island_crossover_rate / rate_sum;
    }

    intra_island_crossover_rate += mutation_rate;
    inter_island_crossover_rate += intra_island_crossover_rate;

    // set the generation id for the initial minimal genome
    seed_genome->set_generation_id(generated_genomes);
    global_best_genome = NULL;

    Log::info("Speciation method is: island (Default is the island-based speciation strategy).\n");
    Log::info("Island Strategy: Number of mutations is set to %d\n", num_mutations);
    if (extinction_event_generation_number > 0) {
        Log::info("Island Strategy: Doing island repopulation\n");
        Log::info("Island Strategy: Extinction event generation number is %d\n", extinction_event_generation_number);
        Log::info("Island Strategy: Repopulation method is %s\n", repopulation_method.c_str());
        Log::info("Island Strategy: Island ranking method is %s\n", island_ranking_method.c_str());
        Log::info("Island Strategy: Repeat extinction is set to %s\n", repeat_extinction ? "true" : "false");
        Log::info("Island Strategy: islands_to_exterminate is set to %d\n", islands_to_exterminate);
    }

    Log::info("Island Strategy: Doing transfer learning: %s\n", transfer_learning ? "true" : "false");

    if (transfer_learning) {
        Log::info("Transfer learning version is %s\n", transfer_learning_version.c_str());
        Log::info("Apply seed stirs: %d\n", seed_stirs);
    }
}

void IslandSpeciationStrategy::initialize_population(function<void(int32_t, RNN_Genome*)>& mutate, WeightRules* weight_rules) {
    for (int32_t i = 0; i < number_of_islands; i++) {
        Island* new_island = new Island(i, max_island_size);
        if (start_filled) {
            new_island->fill_with_mutated_genomes(seed_genome, seed_stirs, tl_epigenetic_weights, mutate, weight_rules);
        }
        islands.push_back(new_island);
    }
}

int32_t IslandSpeciationStrategy::get_generated_genomes() const {
    return generated_genomes;
}

int32_t IslandSpeciationStrategy::get_evaluated_genomes() const {
    return evaluated_genomes;
}

RNN_Genome* IslandSpeciationStrategy::get_best_genome() {
    // the global_best_genome is updated every time a genome is inserted
    return global_best_genome;
}

RNN_Genome* IslandSpeciationStrategy::get_worst_genome() {
    int32_t worst_genome_island = -1;
    double worst_fitness = -EXAMM_MAX_DOUBLE;

    for (int32_t i = 0; i < (int32_t) islands.size(); i++) {
        if (islands[i]->size() > 0) {
            double island_worst_fitness = islands[i]->get_worst_fitness();
            if (island_worst_fitness > worst_fitness) {
                worst_fitness = island_worst_fitness;
                worst_genome_island = i;
            }
        }
    }

    if (worst_genome_island < 0) {
        return NULL;
    } else {
        return islands[worst_genome_island]->get_worst_genome();
    }
}

double IslandSpeciationStrategy::get_best_fitness() {
    RNN_Genome* best_genome = get_best_genome();
    if (best_genome == NULL) {
        return EXAMM_MAX_DOUBLE;
    } else {
        return best_genome->get_fitness();
    }
}

double IslandSpeciationStrategy::get_worst_fitness() {
    RNN_Genome* worst_genome = get_worst_genome();
    if (worst_genome == NULL) {
        return EXAMM_MAX_DOUBLE;
    } else {
        return worst_genome->get_fitness();
    }
}

bool IslandSpeciationStrategy::islands_full() const {
    for (int32_t i = 0; i < (int32_t) islands.size(); i++) {
        if (!islands[i]->is_full()) {
            return false;
        }
    }

    return true;
}

// this will insert a COPY, original needs to be deleted
// returns 0 if a new global best, < 0 if not inserted, > 0 otherwise
int32_t IslandSpeciationStrategy::insert_genome(RNN_Genome* genome) {
    Log::debug("inserting genome!\n");
    repopulate();

    bool new_global_best = false;
    if (global_best_genome == NULL) {
        // this is the first insert of a genome so it's the global best by default
        global_best_genome = genome->copy();
        new_global_best = true;
    } else if (global_best_genome->get_fitness() > genome->get_fitness()) {
        // since we're re-setting this to a copy you need to delete it.
        delete global_best_genome;
        global_best_genome = genome->copy();
        new_global_best = true;
    }
    evaluated_genomes++;
    int32_t island = genome->get_group_id();

    Log::info("Island %d: inserting genome\n", island);
    /*
    Log::info("genome weight init type: %s\n", genome->weight_rules->get_weight_initialize_method_name().c_str());
    Log::info("genome weight inherit type: %s\n", genome->weight_rules->get_weight_inheritance_method_name().c_str());
    Log::info(
        "genome mutated component type: %s\n", genome->weight_rules->get_mutated_components_weight_method_name().c_str()
    );
    */
    if (islands[island] == NULL) {
        Log::fatal("ERROR: island[%d] is null!\n", island);
    }
    int32_t insert_position = islands[island]->insert_genome(genome);
    Log::info("Island %d: Insert position was: %d\n", island, insert_position);

    if (insert_position == 0) {
        if (new_global_best) {
            return 0;
        } else {
            return 1;
        }
    } else {
        return insert_position;  // will be -1 if not inserted, or > 0 if not the global best
    }
}

int32_t IslandSpeciationStrategy::get_worst_island_by_best_genome() {
    int32_t worst_island = -1;
    double worst_best_fitness = 0;
    for (int32_t i = 0; i < (int32_t) islands.size(); i++) {
        if (islands[i]->size() > 0) {
            if (islands[i]->get_erase_again_num() > 0) {
                continue;
            }
            double island_best_fitness = islands[i]->get_best_fitness();
            if (island_best_fitness > worst_best_fitness) {
                worst_best_fitness = island_best_fitness;
                worst_island = i;
            }
        }
    }
    return worst_island;
}

void IslandSpeciationStrategy::repopulate() {
    if (extinction_event_generation_number != 0) {
        if (evaluated_genomes > 1 && evaluated_genomes % extinction_event_generation_number == 0
            && max_genomes - evaluated_genomes >= extinction_event_generation_number) {
            if (island_ranking_method.compare("EraseWorst") == 0 || island_ranking_method.compare("") == 0) {
                global_best_genome = get_best_genome()->copy();
                vector<int32_t> rank = rank_islands();
                for (int32_t i = 0; i < islands_to_exterminate; i++) {
                    if (rank[i] >= 0) {
                        Log::info("found island: %d is the worst island \n", rank[0]);
                        islands[rank[i]]->erase_island();
                        islands[rank[i]]->erase_structure_map();
                        islands[rank[i]]->set_status(Island::REPOPULATING);
                    } else {
                        Log::error("Didn't find the worst island!");
                    }
                    // set this so the island would not be re-killed in 5 rounds
                    if (!repeat_extinction) {
                        set_erased_islands_status();
                    }
                }
            }
        }
    }
}

vector<int32_t> IslandSpeciationStrategy::rank_islands() {
    vector<int32_t> island_rank;
    int32_t temp;
    double fitness_j1, fitness_j2;
    Log::info("ranking islands \n");
    Log::info("repeat extinction: %s \n", repeat_extinction ? "true" : "false");
    for (int32_t i = 0; i < number_of_islands; i++) {
        if (repeat_extinction) {
            island_rank.push_back(i);
        } else {
            if (islands[i]->get_erase_again_num() == 0) {
                island_rank.push_back(i);
            }
        }
    }

    for (int32_t i = 0; i < (int32_t) island_rank.size() - 1; i++) {
        for (int32_t j = 0; j < (int32_t) island_rank.size() - i - 1; j++) {
            fitness_j1 = islands[island_rank[j]]->get_best_fitness();
            fitness_j2 = islands[island_rank[j + 1]]->get_best_fitness();
            if (fitness_j1 < fitness_j2) {
                temp = island_rank[j];
                island_rank[j] = island_rank[j + 1];
                island_rank[j + 1] = temp;
            }
        }
    }
    Log::debug("island rank: \n");
    for (int32_t i = 0; i < (int32_t) island_rank.size(); i++) {
        Log::debug("island: %d fitness %f \n", island_rank[i], islands[island_rank[i]]->get_best_fitness());
    }
    return island_rank;
}

RNN_Genome* IslandSpeciationStrategy::generate_for_initializing_island(
    uniform_real_distribution<double>& rng_0_1, minstd_rand0& generator, function<void(int32_t, RNN_Genome*)>& mutate, WeightRules* weight_rules
) {
    Island* current_island = islands[generation_island];
    RNN_Genome* new_genome = NULL;
    if (current_island->size() == 0) {
        Log::info("Island %d: starting island with minimal genome\n", generation_island);
        new_genome = seed_genome->copy();
        new_genome->initialize_randomly(weight_rules);

        bool stir_seed_genome = false;
        if (stir_seed_genome) {
            Log::info("Stir the seed genome with %d mutations\n", seed_stirs);
            mutate(seed_stirs, new_genome);
            if (!tl_epigenetic_weights) {
                new_genome->initialize_randomly(weight_rules);
            }
        }
    } else {
        Log::info("Island %d: island is initializing but not empty, mutating a random genome\n", generation_island);
        while (new_genome == NULL) {
            current_island->copy_random_genome(rng_0_1, generator, &new_genome);
            mutate(num_mutations, new_genome);
            if (new_genome->outputs_unreachable()) {
                // no path from at least one input to the outputs
                delete new_genome;
                new_genome = NULL;
            }
        }
    }
    new_genome->best_validation_mse = EXAMM_MAX_DOUBLE;
    new_genome->best_validation_mae = EXAMM_MAX_DOUBLE;

    return new_genome;
}

RNN_Genome* IslandSpeciationStrategy::generate_for_repopulating_island(
    uniform_real_distribution<double>& rng_0_1, minstd_rand0& generator, function<void(int32_t, RNN_Genome*)>& mutate,
    function<RNN_Genome*(RNN_Genome*, RNN_Genome*)>& crossover, WeightRules* weight_rules
) {
    // Island *current_island = islands[generation_island];
    RNN_Genome* new_genome = NULL;

    if (repopulation_method.compare("randomParents") == 0 || repopulation_method.compare("randomparents") == 0) {
        Log::info("Island %d: island is repopulating through random parents method!\n", generation_island);
        new_genome = parents_repopulation("randomParents", rng_0_1, generator, mutate, crossover, weight_rules);

    } else if (repopulation_method.compare("bestParents") == 0 || repopulation_method.compare("bestparents") == 0) {
        Log::info("Island %d: island is repopulating through best parents method!\n", generation_island);
        new_genome = parents_repopulation("bestParents", rng_0_1, generator, mutate, crossover, weight_rules);

    } else if (repopulation_method.compare("bestGenome") == 0 || repopulation_method.compare("bestgenome") == 0) {
        new_genome = get_global_best_genome()->copy();
        mutate(num_mutations, new_genome);

    } else if (repopulation_method.compare("bestIsland") == 0 || repopulation_method.compare("bestisland") == 0) {
        // copy the best island to the worst at once
        Log::info(
            "Island %d: island is repopulating through bestIsland method! Coping the best island to the population "
            "island\n",
            generation_island
        );
        Log::info(
            "Island %d: island current size is: %d \n", generation_island,
            islands[generation_island]->get_genomes().size()
        );
        int32_t best_island_id = get_best_genome()->get_group_id();
        repopulate_by_copy_island(best_island_id, mutate);
        if (new_genome == NULL) {
            new_genome = generate_for_filled_island(rng_0_1, generator, mutate, crossover);
        }
    } else {
        Log::fatal("Wrong repopulation method: %s\n", repopulation_method.c_str());
        exit(1);
    }
    return new_genome;
}

RNN_Genome* IslandSpeciationStrategy::generate_genome(
    uniform_real_distribution<double>& rng_0_1, minstd_rand0& generator, function<void(int32_t, RNN_Genome*)>& mutate,
    function<RNN_Genome*(RNN_Genome*, RNN_Genome*)>& crossover, WeightRules* weight_rules
) {
    Log::debug("getting island: %d\n", generation_island);
    Island* current_island = islands[generation_island];
    RNN_Genome* new_genome = NULL;

    if (current_island->is_initializing()) {
        // islands could start with full of mutated seed genomes, it can be used with or without transfer learning
        new_genome = generate_for_initializing_island(rng_0_1, generator, mutate, weight_rules);
    } else if (current_island->is_full()) {
        new_genome = generate_for_filled_island(rng_0_1, generator, mutate, crossover);
    } else if (current_island->is_repopulating()) {
        new_genome = generate_for_repopulating_island(rng_0_1, generator, mutate, crossover, weight_rules);
    }
    if (new_genome == NULL) {
        Log::info("Island %d: new genome is still null, regenerating\n", generation_island);
        new_genome = generate_genome(rng_0_1, generator, mutate, crossover, weight_rules);
    }
    generated_genomes++;
    new_genome->set_generation_id(generated_genomes);
    islands[generation_island]->set_latest_generation_id(generated_genomes);
    new_genome->set_group_id(generation_island);

    if (current_island->is_initializing()) {
        RNN_Genome* genome_copy = new_genome->copy();
        Log::debug("inserting genome copy!\n");
        insert_genome(genome_copy);
    }
    generation_island++;
    if (generation_island >= (int32_t) islands.size()) {
        generation_island = 0;
    }

    return new_genome;
}

int32_t IslandSpeciationStrategy::number_filled_islands() {
    int32_t n_filled = 0;

    for (int32_t i = 0; i < number_of_islands; i++) {
        if (islands[i]->is_full()) {
            n_filled++;
        }
    }

    return n_filled;
}

int32_t IslandSpeciationStrategy::get_other_full_island(
    uniform_real_distribution<double>& rng_0_1, minstd_rand0& generator, int32_t first_island
) {
    // select a different island randomly

    Log::debug("other filled islands:\n");
    vector<int32_t> other_filled_islands;
    for (int32_t i = 0; i < number_of_islands; i++) {
        if (i != first_island && islands[i]->is_full()) {
            other_filled_islands.push_back(i);
            Log::debug("\t %d\n", i);
        }
    }
    int32_t other_island = other_filled_islands[rng_0_1(generator) * other_filled_islands.size()];
    Log::debug("other island: %d\n", other_island);

    return other_island;
}

RNN_Genome* IslandSpeciationStrategy::generate_for_filled_island(
    uniform_real_distribution<double>& rng_0_1, minstd_rand0& generator, function<void(int32_t, RNN_Genome*)>& mutate,
    function<RNN_Genome*(RNN_Genome*, RNN_Genome*)>& crossover
) {
    Island* island = islands[generation_island];
    RNN_Genome* genome;
    double r = rng_0_1(generator);

    // if the island isn't full, or our random number is less
    // than the mutation rate, generate a new genome by mutation.
    if (!island->is_full() || r < mutation_rate) {
        Log::debug("performing mutation\n");
        island->copy_random_genome(rng_0_1, generator, &genome);
        mutate(num_mutations, genome);

    } else if (r < intra_island_crossover_rate || number_filled_islands() == 1) {
        // if the island is full and if we're under the intra island crossover rate, or
        // if there are no other filled islands, do intra-island crossover
        Log::debug("performing intra-island crossover\n");
        // select two distinct parent genomes in the same island
        RNN_Genome *parent1 = NULL, *parent2 = NULL;
        island->copy_two_random_genomes(rng_0_1, generator, &parent1, &parent2);
        genome = crossover(parent1, parent2);
        delete parent1;
        delete parent2;
    } else {
        // get a random genome from this island
        RNN_Genome* parent1 = NULL;
        island->copy_random_genome(rng_0_1, generator, &parent1);

        int32_t other_island_index = get_other_full_island(rng_0_1, generator, generation_island);

        // get the best genome from the other island
        RNN_Genome* parent2 = islands[other_island_index]->get_best_genome()->copy();  // new RNN GENOME

        // swap so the first parent is the more fit parent
        if (parent1->get_fitness() > parent2->get_fitness()) {
            RNN_Genome* tmp = parent1;
            parent1 = parent2;
            parent2 = tmp;
        }
        genome = crossover(parent1, parent2);  // new RNN GENOME

        delete parent1;
        delete parent2;
    }

    if (genome->outputs_unreachable()) {
        // no path from at least one input to the outputs
        delete genome;
        genome = NULL;
    }
    return genome;
}

void IslandSpeciationStrategy::print(string indent) const {
    Log::trace("%sIslands: \n", indent.c_str());
    for (int32_t i = 0; i < (int32_t) islands.size(); i++) {
        Log::trace("%sIsland %d:\n", indent.c_str(), i);
        islands[i]->print(indent + "\t");
    }
}

/**
 * Gets speciation strategy information headers for logs
 */
string IslandSpeciationStrategy::get_strategy_information_headers() const {
    string info_header = "";
    for (int32_t i = 0; i < (int32_t) islands.size(); i++) {
        info_header.append(",");
        info_header.append("Island_");
        info_header.append(to_string(i));
        info_header.append("_best_fitness");
        info_header.append(",");
        info_header.append("Island_");
        info_header.append(to_string(i));
        info_header.append("_worst_fitness");
    }
    return info_header;
}

/**
 * Gets speciation strategy information values for logs
 */
string IslandSpeciationStrategy::get_strategy_information_values() const {
    string info_value = "";
    for (int32_t i = 0; i < (int32_t) islands.size(); i++) {
        double best_fitness = islands[i]->get_best_fitness();
        double worst_fitness = islands[i]->get_worst_fitness();
        info_value.append(",");
        info_value.append(to_string(best_fitness));
        info_value.append(",");
        info_value.append(to_string(worst_fitness));
    }
    return info_value;
}

RNN_Genome* IslandSpeciationStrategy::parents_repopulation(
    string method, uniform_real_distribution<double>& rng_0_1, minstd_rand0& generator,
    function<void(int32_t, RNN_Genome*)>& mutate, function<RNN_Genome*(RNN_Genome*, RNN_Genome*)>& crossover, WeightRules* weight_rules
) {
    RNN_Genome* genome = NULL;

    Log::debug("generation island: %d \n", generation_island);

    if (number_filled_islands() < 2) {
        Log::fatal("tried to do parent repopulation when there were only %d full islands.\n", number_filled_islands());
        exit(1);
    }

    int32_t parent_island1 = get_other_full_island(rng_0_1, generator, -1);
    int32_t parent_island2 = get_other_full_island(rng_0_1, generator, parent_island1);

    RNN_Genome* parent1 = NULL;
    RNN_Genome* parent2 = NULL;

    // Track whether parents are copies that need deletion
    bool parent1_is_copy = false;
    bool parent2_is_copy = false;
    
    while (parent1 == NULL) {
        if (method.compare("randomParents") == 0) {
            islands[parent_island1]->copy_random_genome(rng_0_1, generator, &parent1);
            parent1_is_copy = true; // This is a copy that needs deletion
        } else if (method.compare("bestParents") == 0) {
            parent1 = islands[parent_island1]->get_best_genome();
            parent1_is_copy = false; // This is a reference, don't delete
        }
    }

    while (parent2 == NULL) {
        if (method.compare("randomParents") == 0) {
            islands[parent_island2]->copy_random_genome(rng_0_1, generator, &parent2);
            parent2_is_copy = true; // This is a copy that needs deletion
        } else if (method.compare("bestParents") == 0) {
            parent2 = islands[parent_island2]->get_best_genome();
            parent2_is_copy = false; // This is a reference, don't delete
        }
    }

    Log::debug(
        "current island is %d, the parent1 island is %d, parent 2 island is %d (parent1_copy: %d, parent2_copy: %d)\n", 
        generation_island, parent_island1, parent_island2, parent1_is_copy, parent2_is_copy
    );

    // swap so the first parent is the more fit parent
    if (parent1->get_fitness() > parent2->get_fitness()) {
        RNN_Genome* tmp = parent1;
        parent1 = parent2;
        parent2 = tmp;
        // Also swap the copy flags
        bool tmp_flag = parent1_is_copy;
        parent1_is_copy = parent2_is_copy;
        parent2_is_copy = tmp_flag;
    }
    genome = crossover(parent1, parent2);

    // CRITICAL FIX: Delete parent copies to prevent memory leaks
    if (parent1_is_copy) {
        Log::debug("Deleting parent1 copy at %p\n", parent1);
        delete parent1;
        parent1 = NULL;
    }
    if (parent2_is_copy) {
        Log::debug("Deleting parent2 copy at %p\n", parent2);
        delete parent2;
        parent2 = NULL;
    }

    mutate(num_mutations, genome);

    if (genome->outputs_unreachable()) {
        // no path from at least one input to the outputs
        delete genome;
        genome = generate_genome(rng_0_1, generator, mutate, crossover, weight_rules);
    }
    return genome;
}

void IslandSpeciationStrategy::repopulate_by_copy_island(
    int32_t best_island_id, function<void(int32_t, RNN_Genome*)>& mutate
) {
    vector<RNN_Genome*> best_island_genomes = islands[best_island_id]->get_genomes();
    for (int32_t i = 0; i < (int32_t) best_island_genomes.size(); i++) {
        // copy the genome from the best island
        RNN_Genome* copy = best_island_genomes[i]->copy();
        mutate(num_mutations, copy);

        generated_genomes++;
        copy->set_generation_id(generated_genomes);
        islands[generation_island]->set_latest_generation_id(generated_genomes);
        copy->set_group_id(generation_island);
        insert_genome(copy);
    }
}

RNN_Genome* IslandSpeciationStrategy::get_global_best_genome() {
    return global_best_genome;
}

void IslandSpeciationStrategy::set_erased_islands_status() {
    for (int32_t i = 0; i < (int32_t) islands.size(); i++) {
        if (islands[i]->get_erase_again_num() > 0) {
            islands[i]->set_erase_again_num();
            Log::debug("Island %d can be removed in %d rounds.\n", i, islands[i]->get_erase_again_num());
        }
    }
}

RNN_Genome* IslandSpeciationStrategy::get_seed_genome() {
    return seed_genome;
}
// write a save entire population function with an input saving function

void IslandSpeciationStrategy::save_entire_population(string output_path) {
    for (int32_t i = 0; i < (int32_t) islands.size(); i++) {
        islands[i]->save_population(output_path);
    }
}

void IslandSpeciationStrategy::finalize_generation(int32_t current_generation, const vector< vector< vector<double> > > &validation_input, const vector< vector< vector<double> > > &validation_output, const vector< vector< vector<double> > > &test_input, const vector< vector< vector<double> > > &test_output) {
    Log::info("Finalizing generation %d\n", current_generation);
}

