//
// CB-CF Configurable Bucket Cuckoo filter by S. Pontarelli and J. Martinez
//

#include "pch.h"
#include "CF.hpp"
#include <iostream>
#include <fstream>
#include <time.h>
#include <sstream>
#include <string>
 

/*
 * test_setup stores the command line arguments
 */

struct test_setup {
	int filter_mode;
	int filter_size;
	int filter_cells_in_bucket;
	int occupancy;
	int run_trials;
	int scrubbing;
	int scrub_iterations;
	int *fingerprint_bits;
	int fingerprint_bits_len;
};


/*
 * test_data stores the test output
 */

struct test_data {
	int cf_items;
	double occupancy;
	int total_iterations;
	int max_iterations;
	double avg_iterations;
	int filter_errors;
	int query_errors;
	double FPR;
};


/*
 * test setup using the command line arguments
 */

test_setup setup_CBCF_test(int argc, char* argv[]) {
	test_setup setup;

	setup.filter_mode      = -1;
	setup.filter_size      = -1;
	setup.occupancy        = -1;
	setup.run_trials       = -1;
	setup.scrubbing        = -1;
	setup.scrub_iterations = -1;
	int f                  = -1;

	setup.filter_cells_in_bucket  = 4;

	for (int i = 1; i < argc; i++) {
		std::stringstream str;
		str << argv[i];

		std::string argument = str.str();

		// argument 'm' is used for mode, default value is m=1 (0:cf, 1:cbcf)

		if (argument[0] == 'm' || argument[0] == 'M') {
			int len = argument.size();
			setup.filter_mode = atoi(argument.substr(2, len - 1).c_str());
		}

		// argument 's' is used for filter size, default value is s=8192

		if (argument[0] == 's' || argument[0] == 'S') {
			int len = argument.size();
			setup.filter_size = atoi(argument.substr(2, len - 1).c_str());
		}

		// argument 'o' is used for occupancy, default value is o=95

		if (argument[0] == 'o' || argument[0] == 'O') {
			int len = argument.size();
			setup.occupancy = atoi(argument.substr(2, len - 1).c_str());
		}

		// argument 'r' is used for runs (trials), default value is r=10000

		if (argument[0] == 'r' || argument[0] == 'R') {
			int len = argument.size();
			setup.run_trials = atoi(argument.substr(2, len - 1).c_str());
		}

		// argument 'b' is used for filter scrubbing before the test, default value is b=1

		if (argument[0] == 'b' || argument[0] == 'B') {
			int len = argument.size();
			setup.scrubbing = atoi(argument.substr(2, len - 1).c_str());
		}

		// argument 'i' is used for scrub iterations, default value is 20

		if (argument[0] == 'i' || argument[0] == 'I') {
			int len = argument.size();
			setup.scrub_iterations = atoi(argument.substr(2, len - 1).c_str());
		}

		// argument 'f' is used for fingerprint_bits, default value is f = {12}

		if (argument[0] == 'f' || argument[0] == 'F') {
			int len = argument.size();
			f = atoi(argument.substr(2, len - 1).c_str());
		}
	}

	// set default values for filter mode, filter size, occupancy, runs (trials) and fingerprint_bits

	if (setup.filter_mode == -1)
		setup.filter_mode = 1;

	if (setup.filter_size == -1)
		setup.filter_size = 8192;

	if (setup.occupancy == -1)
		setup.occupancy = 95;

	if (setup.run_trials == -1)
		setup.run_trials = 10000;

	if (setup.scrubbing == -1)
		setup.scrubbing = 1;

	if (setup.scrub_iterations == -1)
		setup.scrub_iterations = 20;

	if (f == -1) {
		setup.fingerprint_bits_len = 3;
		setup.fingerprint_bits = new int[setup.fingerprint_bits_len];
		setup.fingerprint_bits[0] = 12;
		setup.fingerprint_bits[1] = 15;
		setup.fingerprint_bits[2] = 18;
	}
	else {
		setup.fingerprint_bits_len = 1;
		setup.fingerprint_bits = new int[setup.fingerprint_bits_len];
		setup.fingerprint_bits[0] = f;
	}

	return setup;
}


/*
 * displays the test setup on the console
 */

void display_CBCF_test_parameters(test_setup setup) {
	if (setup.filter_mode == 0)
		std::cout << "CF Test \n\n";
	else
		std::cout << "Configurable Bucket CF Test \n\n";

	std::cout << "Filter mode      m = " << setup.filter_mode << "\n";
	std::cout << "Filter size      s = " << setup.filter_size << "\n";
	std::cout << "Cells in bucket  c = " << setup.filter_cells_in_bucket << "\n";
	std::cout << "Occupancy        o = " << setup.occupancy << "%\n";
	std::cout << "Runs (trials)    r = " << setup.run_trials << "\n";
	std::cout << "Scrubbing        b = " << setup.scrubbing << "\n";
	std::cout << "Scrub Iterations i = " << setup.scrub_iterations << "\n";

	std::cout << "Fingerprint bits f = {";

	for (int i = 0; i < setup.fingerprint_bits_len; i++)
		std::cout << " " << setup.fingerprint_bits[i] << ((i == setup.fingerprint_bits_len - 1) ? " " : ",");

	std::cout << "} \n";
}


/*
 * test: filter initialization, filter warm-up and FPR calculation
 */

test_data test(int total_keys, int filter_scrub, int scrub_iterations, int filter_mode, int filter_size, int filter_cells_in_bucket, int fingerprint_bits) {
	test_data res;

	CF filter = CF(filter_mode, filter_size, filter_cells_in_bucket, fingerprint_bits, scrub_iterations);

	int rand_rule;
	uint64_t key;
	const uint64_t base = 17179869184;
	const uint64_t prefix_offset = 8589934592;
	
	int filter_errors = 0;
	int query_errors  = 0;

	// filter initialization using random keys

	for (int i = 1; i <= total_keys; i++) {
		rand_rule = rand() % 1000000 + 1;

		key = (uint64_t)i + (uint64_t)(rand_rule * base) + (uint64_t)prefix_offset;

		int insert_attemps = -1;

		insert_attemps = filter.insert(key);

		if (insert_attemps == -1) {
			filter_errors++;
		}
		else {
			if (!filter.query(key)) {
				query_errors++;
			}
		}
	}

	// filter warm-up deleting and inserting filter_size * filter_cells_in_bucket keys

	int offset = total_keys + 1;

	filter_errors = 0;
	query_errors  = 0;

	int total_iterations = 0;
	int max_iterations   = 0;

	for (int i = 1; i <= (filter_size * filter_cells_in_bucket); i++) {
		// a random key is removed 

		filter.random_remove();

		// a new random key is calculated

		rand_rule = rand() % 1000000 + 1;

		key = (uint64_t)offset + (uint64_t)i + (uint64_t)(rand_rule * base) + (uint64_t)prefix_offset;

		int insert_attemps = -1;

		insert_attemps = filter.insert(key);

		if (insert_attemps == -1) {
			filter_errors++;
		}
		else {
			total_iterations = total_iterations + insert_attemps;

			if (max_iterations < insert_attemps)
				max_iterations = insert_attemps;

			if (!filter.query(key)) {
				query_errors++;
			}
		}
	}

	// fingerprints from full buckets are moved to buckets having lower occupancy to reduce the FPR
	
	if (filter_scrub == 1)
		for (int k = 1; k <= scrub_iterations; k++)
			filter.scrub();

	// the filter test inserts total_new_keys not present in the filter and calculates max iterations and avg. iterations

	offset = total_keys + 1 + (filter_size * filter_cells_in_bucket);

	int total_queries   = 1000000;
	int total_false_pos = 0;

	for (int i = 1; i <= total_queries; i++) {
		rand_rule = rand() % 1000000 + 1;

		key = (uint64_t)offset + (uint64_t)i + (uint64_t)(rand_rule * base) + (uint64_t)prefix_offset;

		if (filter.query(key))
			total_false_pos++;
	}

	res.cf_items = filter.get_cf_items();
	res.occupancy = (double) (res.cf_items) / (double) (filter_size * filter_cells_in_bucket);
	res.total_iterations = total_iterations;
	res.max_iterations = max_iterations;
	res.avg_iterations = (double) total_iterations / (filter_size * filter_cells_in_bucket);
	res.filter_errors = filter_errors;
	res.query_errors = query_errors;
	res.FPR = (double)total_false_pos / (double)total_queries;

	return res;
}


/*
 * runs the test multiple times for each fingerprint size {12, 15, 18}
 */

void CBCF_test(int total_keys, test_setup setup) {
	test_data res;
	int f;

	for (int i = 0; i < setup.fingerprint_bits_len; i++) {
		f = setup.fingerprint_bits[i];

		int max_iterations    = 0;
		double avg_iterations = 0.0;
		double avg_FPR        = 0.0;

		std::cout << "\nTesting b=" << setup.scrubbing << ", i=" << setup.scrub_iterations << ", m=" << setup.filter_mode << ", s=" << setup.filter_size << ", c=" << setup.filter_cells_in_bucket << ", o=" << setup.occupancy << "%, r=" << setup.run_trials << ", f=" << f;

		string data_file;

		std::stringstream ss;
		ss << "b" << setup.scrubbing << "_i" << setup.scrub_iterations << "_m" << setup.filter_mode << "_s" << setup.filter_size << "_c" << setup.filter_cells_in_bucket << "_o" << setup.occupancy << "_f" << f << "_r" << setup.run_trials << ".txt";
		data_file = ss.str();

		ofstream test_output(data_file);
		
		if (setup.filter_mode == 0)
			test_output << "%Standard CF filter \n";
		else
			test_output << "%Configurable Bucket CF filter \n";

		test_output << "%keys " << total_keys << "\n";
		test_output << "%\n";
		test_output << "%items\titer\t max\tavg\terr\tocc\t\tFPR\n";

		for (int r = 1; r <= setup.run_trials; r++) {
			res = test(total_keys, setup.scrubbing, setup.scrub_iterations, setup.filter_mode, setup.filter_size, setup.filter_cells_in_bucket, f);

			if (res.max_iterations > max_iterations)
				max_iterations = res.max_iterations;

			avg_iterations = avg_iterations + res.avg_iterations;
			avg_FPR = avg_FPR + res.FPR;

			test_output << res.cf_items << "\t" << res.total_iterations << "\t " << res.max_iterations << "\t" << res.avg_iterations << "\t" << res.filter_errors << "\t" << res.occupancy << "\t" << res.FPR << "\n";
		}

		avg_iterations = avg_iterations / (double)setup.run_trials;
		avg_FPR = avg_FPR / (double)setup.run_trials;

		test_output << "%\n";
		test_output << "%Max iterations " << max_iterations << "\n";
		test_output << "%Avg iterations " << avg_iterations << "\n";
		test_output << "%Avg FPR        " << avg_FPR << "\n";

		test_output.close();

		// the occupancy and the average FPR of the test using f fingerprint bits is stored 

		ss = std::stringstream();
		ss << "b" << setup.scrubbing << "_i" << setup.scrub_iterations << "_m" << setup.filter_mode << "_f" << f << ".txt";
		data_file = ss.str();

		ofstream test_avg(data_file, ios::out | ios::app);

		test_avg << setup.occupancy << "\t" << avg_FPR << "\n";

		test_avg.close();
	}
}


int main(int argc, char* argv[]) {
	test_setup setup;

	// command line arguments for CB-CF test program
	//
	// m: filter mode, default value is 1 for cbcf, 0 is for standard cf
	// s: filter size, default value is 8192
	// o: filter occupancy, default value is 95%
	// r: runs (trials), default value is 10000
	// b: scrubbing, default value is 1
	// i: scrub iterations, default value is 20
	// f: fingerprint_bits, default value is f = {12, 15, 18}
	//
	// example: CBCF.exe o=94 r=1000 f=12

	setup = setup_CBCF_test(argc, argv);

	// CB-CF test parameters

	display_CBCF_test_parameters(setup);

	// CB-CF test

	int total_keys = (setup.filter_size * setup.filter_cells_in_bucket) * (double)(setup.occupancy) / 100.0;

	CBCF_test(total_keys, setup);

	return 0;

}
