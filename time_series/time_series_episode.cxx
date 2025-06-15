#include <algorithm>
#include <cmath>
#include <fstream>

#include "common/log.hxx"
#include "time_series_episode.hxx"

using std::max;
using std::ofstream;
using std::ifstream;

TimeSeriesEpisode::TimeSeriesEpisode(int32_t id) 
    : episode_id(id), training_score(1), is_loaded(false) {
}

TimeSeriesEpisode::TimeSeriesEpisode(int32_t id, const vector<vector<double>>& _inputs, const vector<vector<double>>& _outputs)
    : episode_id(id), inputs(_inputs), outputs(_outputs), training_score(1), is_loaded(true) {
}

TimeSeriesEpisode::~TimeSeriesEpisode() {
    // Cleanup handled automatically by vectors
}

// void TimeSeriesEpisode::update_access_time() {
//     access_count++;
// }

const vector<vector<double>>& TimeSeriesEpisode::get_inputs() {
    ensure_loaded();
    // update_access_time();
    return inputs;
}

const vector<vector<double>>& TimeSeriesEpisode::get_outputs() {
    ensure_loaded();
    // update_access_time();
    return outputs;
}

void TimeSeriesEpisode::set_data(const vector<vector<double>>& _inputs, const vector<vector<double>>& _outputs) {
    inputs = _inputs;
    outputs = _outputs;
    is_loaded = true;
    // update_access_time();
}

void TimeSeriesEpisode::update_training_score(int32_t increment) {
    training_score += increment;
    // update_access_time();
}

int32_t TimeSeriesEpisode::get_training_score() const {
    return training_score;
}

void TimeSeriesEpisode::add_training_generation(int32_t generation_id) {
    // Only add if not already present
    if (std::find(training_generations.begin(), training_generations.end(), generation_id) == training_generations.end()) {
        training_generations.push_back(generation_id);
    }
}

void TimeSeriesEpisode::update_genome_performance(int32_t genome_id, int32_t score) {
    genome_performance[genome_id] = score;
    // update_access_time();
}

const vector<int32_t>& TimeSeriesEpisode::get_training_generations() const {
    return training_generations;
}

bool TimeSeriesEpisode::is_data_loaded() const {
    return is_loaded;
}

void TimeSeriesEpisode::ensure_loaded() {
    if (!is_loaded) {
        // For now, this is a no-op since we keep everything in memory
        // Future implementation could load from disk here
        Log::debug("Episode %d data access - already in memory\n", episode_id);
    }
}

// void TimeSeriesEpisode::unload_data() {
//     // Future implementation for memory management
//     // For now, keep everything in memory
//     Log::debug("Episode %d unload requested - keeping in memory for now\n", episode_id);
// }

// int32_t TimeSeriesEpisode::get_access_count() const {
//     return access_count;
// }

// double TimeSeriesEpisode::calculate_importance(int32_t current_generation) const {
//     // Calculate importance based on multiple factors:
//     // 1. Training score (higher = more important)
//     // 2. Number of training generations (more usage = more important)
//     // 3. Access count (more accessed = more important)
    
//     double score_factor = static_cast<double>(training_score) / 10.0;  // Normalize training score
    
//     // Usage frequency factor
//     double usage_factor = static_cast<double>(training_generations.size()) / 10.0;
    
//     // Access count factor
//     double access_factor = static_cast<double>(access_count) / 100.0;
    
//     return score_factor + usage_factor + access_factor;
// }

// bool TimeSeriesEpisode::should_keep(int32_t current_generation, int32_t max_age) const {
//     // Keep episode if:
//     // 1. Recently accessed (within max_age generations)
//     // 2. High training score
//     // 3. Used in many training generations
    
//     if (!training_generations.empty()) {
//         int32_t last_training_gen = *std::max_element(training_generations.begin(), training_generations.end());
//         if (current_generation - last_training_gen < max_age) {
//             return true;
//         }
//     }
    
//     // Keep high-scoring episodes
//     if (training_score > 5) {
//         return true;
//     }
    
//     // Keep frequently used episodes
//     if (training_generations.size() > 3) {
//         return true;
//     }
    
//     return false;
// }

int32_t TimeSeriesEpisode::get_episode_id() const {
    return episode_id;
}

void TimeSeriesEpisode::print_stats() const {
    Log::info("Episode %d Stats:\n", episode_id);
    Log::info("  Training Score: %d\n", training_score);
    // Log::info("  Access Count: %d\n", access_count);
    Log::info("  Training Generations: %d\n", static_cast<int32_t>(training_generations.size()));
    Log::info("  Loaded: %s\n", is_loaded ? "Yes" : "No");
    // Log::info("  Importance: %.3f\n", calculate_importance());
}

// void TimeSeriesEpisode::save_to_file(const string& filepath) {
//     // Stub for future disk-based storage
//     Log::debug("Episode %d save to file: %s (not implemented)\n", episode_id, filepath.c_str());
// }

// void TimeSeriesEpisode::load_from_file(const string& filepath) {
//     // Stub for future disk-based storage
//     Log::debug("Episode %d load from file: %s (not implemented)\n", episode_id, filepath.c_str());
// } 