//
// CF.hpp : Cuckoo Filter by S. Pontarelli and CB-CF by J. Martinez
//

#include <cstdint>

using namespace std;

class CF {	
	public:

		int **table;               // filter memory
		unsigned char *fp_count;   // fingerprint counter
		int cf_mode;               // 0:standard cf, 1:sbcf
		int cf_size;               // memory size
		int cf_cells;			   // number of cells in each bucket
		int fp_size;               // small fingerprint: 1 << f
		int fp_bsize;              // large fingerprint: (1 << ((f * 4) / 3))
		int cf_scrub_iter;         // scrub iterations
		int cf_num_items;   	   // number of items in the filter

		int victim_fingerprint;
		int victim_pointer;

		CF(int mode, int size, int cells, int f, int scrub_iterations);
		virtual ~CF();
		int insert(uint64_t key);
		void scrub();
		void get_bucket_occupancy(double *&bucket_occupancy);
		void random_remove();
		bool query(uint64_t key);
		int get_cf_items() { return cf_num_items; }
		int get_bucket_fingerprints(int p) { return fp_count[p]; };
		int get_fingerprints();
		int get_size() { return cf_cells*cf_size; }


	private:

		int insert_scrub(uint64_t p, int fingerprint, int scrub_iterations);
        int hash(uint64_t key, int i, int s);
        int RSHash(uint64_t key);
        int JSHash(uint64_t key);

};
