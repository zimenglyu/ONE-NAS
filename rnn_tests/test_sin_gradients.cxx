#include <chrono>
#include <fstream>
using std::getline;
using std::ifstream;

#include <random>
using std::minstd_rand0;
using std::uniform_real_distribution;

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <iostream>
using std::cout;
using std::endl;

#include "common/arguments.hxx"
#include "common/log.hxx"
#include "gradient_test.hxx"
#include "rnn/generate_nn.hxx"
#include "rnn/rnn_edge.hxx"
#include "rnn/rnn_genome.hxx"
#include "rnn/rnn_node.hxx"
#include "rnn/rnn_node_interface.hxx"
#include "rnn/sin_node.hxx"
#include "time_series/time_series.hxx"
#include "weights/weight_rules.hxx"

int main(int argc, char** argv) {
    vector<string> arguments = vector<string>(argv, argv + argc);

    Log::initialize(arguments);
    Log::set_id("main");

    initialize_generator();

    RNN_Genome* genome;

    Log::info("TESTING SIN\n");

    vector<vector<double> > inputs;
    vector<vector<double> > outputs;

    int input_length = 10;
    get_argument(arguments, "--input_length", true, input_length);

    WeightRules* weight_rules = new WeightRules();
    weight_rules->initialize_from_args(arguments);

    for (int32_t max_recurrent_depth = 1; max_recurrent_depth <= 5; max_recurrent_depth++) {
        Log::info("testing with max recurrent depth: %d\n", max_recurrent_depth);

        inputs.resize(1);
        outputs.resize(1);

        generate_random_vector(input_length, inputs[0]);
        generate_random_vector(input_length, outputs[0]);

        vector<string> inputs1{"input 1"};
        vector<string> outputs1{"output 1"};

        // Test 1 input, 1 output, no hidden
        genome = create_sin(inputs1, 0, 0, outputs1, max_recurrent_depth, weight_rules);
        gradient_test("SIN: 1 Input, 1 Output", genome, inputs, outputs, weight_rules);
        delete genome;

        genome = create_sin(inputs1, 1, 1, outputs1, max_recurrent_depth, weight_rules);
        gradient_test("SIN: 1 Input, 1x1 Hidden, 1 Output", genome, inputs, outputs, weight_rules);
        delete genome;

        genome = create_sin(inputs1, 1, 2, outputs1, max_recurrent_depth, weight_rules);
        gradient_test("SIN: 1 Input, 1x2 Hidden, 1 Output", genome, inputs, outputs, weight_rules);
        delete genome;

        genome = create_sin(inputs1, 2, 2, outputs1, max_recurrent_depth, weight_rules);
        gradient_test("SIN: 1 Input, 2x2 Hidden, 1 Output", genome, inputs, outputs, weight_rules);
        delete genome;

        vector<string> inputs2{"input 1", "input 2"};
        vector<string> outputs2{"output 1", "output 2"};

        // Test 2 inputs, 2 outputs, no hidden
        genome = create_sin(inputs2, 0, 0, outputs2, max_recurrent_depth, weight_rules);

        inputs.resize(2);
        outputs.resize(2);
        generate_random_vector(input_length, inputs[0]);
        generate_random_vector(input_length, outputs[0]);
        generate_random_vector(input_length, inputs[1]);
        generate_random_vector(input_length, outputs[1]);

        gradient_test("SIN: 2 Input, 2 Output", genome, inputs, outputs, weight_rules);
        delete genome;

        genome = create_sin(inputs2, 2, 2, outputs2, max_recurrent_depth, weight_rules);
        gradient_test("SIN: 2 Input, 2x2 Hidden, 2 Output", genome, inputs, outputs, weight_rules);
        delete genome;

        genome = create_sin(inputs2, 2, 3, outputs2, max_recurrent_depth, weight_rules);
        gradient_test("SIN: 2 Input, 2x3 Hidden, 2 Output", genome, inputs, outputs, weight_rules);
        delete genome;

        genome = create_sin(inputs2, 3, 3, outputs2, max_recurrent_depth, weight_rules);
        gradient_test("SIN: 2 Input, 3x3 Hidden, 2 Output", genome, inputs, outputs, weight_rules);
        delete genome;

        vector<string> inputs3{"input 1", "input 2", "input 3"};
        vector<string> outputs3{"output 1", "output 2", "input 3"};

        // Test 3 inputs, 3 outputs, no hidden
        genome = create_sin(inputs3, 0, 0, outputs3, max_recurrent_depth, weight_rules);

        inputs.resize(3);
        outputs.resize(3);
        generate_random_vector(input_length, inputs[0]);
        generate_random_vector(input_length, outputs[0]);
        generate_random_vector(input_length, inputs[1]);
        generate_random_vector(input_length, outputs[1]);
        generate_random_vector(input_length, inputs[2]);
        generate_random_vector(input_length, outputs[2]);

        gradient_test("SIN: Three Input, Three Output", genome, inputs, outputs, weight_rules);
        delete genome;

        genome = create_sin(inputs3, 3, 3, outputs3, max_recurrent_depth, weight_rules);
        gradient_test("SIN: 3 Input, 3x3 Hidden, 3 Output", genome, inputs, outputs, weight_rules);
        delete genome;

        genome = create_sin(inputs3, 3, 4, outputs3, max_recurrent_depth, weight_rules);
        gradient_test("SIN: 3 Input, 3x4 Hidden, 3 Output", genome, inputs, outputs, weight_rules);
        delete genome;

        genome = create_sin(inputs3, 4, 4, outputs3, max_recurrent_depth, weight_rules);
        gradient_test("SIN: 3 Input, 4x4 Hidden, 3 Output", genome, inputs, outputs, weight_rules);
        delete genome;
    }
}
