//
// CF.cpp : Cuckoo Filter by S. Pontarelli and CB-CF by J. Martinez
//

#include "pch.h"
#include <iostream>
#include "CF.hpp"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include <cstring>
#include <smmintrin.h>


/*
 * CF (Cuckoo Filter) constructor
 */

CF::CF(int mode, int size, int cells, int f, int scrub_iterations)
{
	cf_mode  = mode;
	cf_size  = size;
	cf_cells = cells;
	fp_size  = (1 << f);
	fp_bsize = (1 << ((f * 4) / 3));

	cf_scrub_iter = scrub_iterations;

	// fp_size is the small fingerprint and fp_bsize is the large fingerprint
	
	table = new int*[cf_size];
	fp_count = new unsigned char[cf_size];
	
	for (int i = 0; i < cf_size; i++)
		table[i] = new int[cf_cells];
	
	// the CF memory is set to -1 and the fingerprint count fp_count[i] is initialized

	cf_num_items = 0;
	victim_fingerprint = -1;
	victim_pointer = -1;

	for (int i = 0; i < cf_size; i++)
	{
		// if the filter mode is 0 (standard CF), fp_count[i] is set to cf_cells, otherwise is set to 0

		fp_count[i] = (cf_mode == 1) ? 0 : cf_cells;

		for (int j = 0; j < cf_cells; j++)
			table[i][j] = -1;
	}
}


/*
 * CF (Cuckoo Filter) destructor
 */

CF::~CF()
{
	for (int i = 0; i < cf_size; i++)
		delete[] table[i];
	
	delete[] table;
	delete[] fp_count;
}


/*
 * insert calculates the fingerprint and calls insert2 function, which performs the insertion in the
 * table and increments the fingerprint count of the bucket
 */ 

int CF::insert(uint64_t key)
{
	// the fingerprint is calculated using fp_bsize, the large f bits

	int fingerprint = hash(key, 1, fp_bsize);

	int p = hash(key, 2, cf_size);
	p = p % cf_size;

	return insert_scrub(p, fingerprint, 0);
}


/*
 * insert_scrub inserts an element in the table using the large fingerprint and increments the fingerprint
 * count of the bucket p after insertion
 *
 * if the insertion fails returns -1, otherwise returns the number of insertion attempts run
 */

int CF::insert_scrub(uint64_t p, int fingerprint, int scrub_iterations)
{
	int p1;
	int j = 0;
	int jj = 0;
	int pushed_fingerprint = -1;

	for (int t = 1; t <= 1000; t++) {

		// the fingerprint may be inserted in 2 x cf_cells different buckets (using 2 hash functions)

		for (jj = 0; jj < cf_cells; jj++) {

			// buckets p and p1 are chosen randomly

			if (rand() % 2 == 0) {
				p = p % cf_size;
				p1 = p ^ hash(fingerprint, 2, cf_size);
			}
			else {
				p = p ^ hash(fingerprint, 2, cf_size);
				p1 = p % cf_size;
			}

			// if scrub_iteration is 0 the standard insertion is run, otherwise scrub_iteration
			// is used to select a bucket having low or high occupancy

			if (scrub_iterations == 0) {

				// standard insertion using buckets p or p1 with an empty cell

				if (table[p][jj] == -1) {
					table[p][jj] = fingerprint;
					fp_count[p]++;
					cf_num_items++;

					return t;
				}

				p1 = p1 % cf_size;

				if (table[p1][jj] == -1) {
					table[p1][jj] = fingerprint;
					fp_count[p1]++;
					cf_num_items++;

					return t;
				}

			}
			else {

				// if the insertion attempt is less than scrub_iterations, it considers only
				// buckets p or p1 having occupancy up to 75%

				if (t < scrub_iterations) {

					// checks if the occupancy of buckets p or p1 is less than 50% 

					if (fp_count[p] < cf_cells - 1) {
						if (table[p][jj] == -1) {
							table[p][jj] = fingerprint;
							fp_count[p]++;
							cf_num_items++;

							return t;
						}
					}

					if (fp_count[p1] < cf_cells - 1) {
						p1 = p1 % cf_size;

						if (table[p1][jj] == -1) {
							table[p1][jj] = fingerprint;
							fp_count[p1]++;
							cf_num_items++;

							return t;
						}
					}

				}
				else {

					// checks if there is an empty cell in buckets p or p1 

					if (fp_count[p] < cf_cells) {
						if (table[p][jj] == -1) {
							table[p][jj] = fingerprint;
							fp_count[p]++;
							cf_num_items++;

							return t;
						}
					}

					if (fp_count[p1] < cf_cells) {
						p1 = p1 % cf_size;

						if (table[p1][jj] == -1) {
							table[p1][jj] = fingerprint;
							fp_count[p1]++;
							cf_num_items++;

							return t;
						}
					}

				}

			}

		}

		// at this point the insertion has failed and the new fingerprint pushes an element previously stored

		j = rand() % 2;
		p = p ^ (j*hash(fingerprint, 2, cf_size));
		p = p % cf_size;

		jj = rand() % cf_cells;

		pushed_fingerprint = table[p][jj];

		table[p][jj] = fingerprint;

		// check if the pushed fingerprint is a fingerprint

		if (pushed_fingerprint == -1) {
			fp_count[p]++;
			cf_num_items++;
			return t;
		}

		// find a new place for the pushed fingerprint

		fingerprint = pushed_fingerprint;
	}

	// at this point the insertion has failed after 1000 attempts

	victim_pointer = p;
	victim_fingerprint = fingerprint;

	return -1;
}


/*
 * scrub moves fingerprints from full buckets to buckets having low occupancy
 */


void CF::scrub() {
	int q, fingerprint;
	int *fp_pos = new int[cf_cells];
	bool *fp_sel = new bool[cf_cells];

	for (int p = 0; p < cf_size; p++) {

		// if bucket p is full, it tries to move a fingerprint stored in this bucket

		if (fp_count[p] == cf_cells) {

			// fp_pos vector stores random positions to select the fingerprint to move

			for (int i = 0; i < cf_cells; i++)
				fp_sel[i] = false;

			for (int i = 0; i < cf_cells; i++) {
				do {
					q = rand() % cf_cells;
				} while (fp_sel[q]);

				fp_pos[i] = q;
				fp_sel[q] = true;
			}

			// removes the fingerprint from the bucket and the fingerprint is inserted again

			fingerprint = table[p][fp_pos[0]];

			table[p][fp_pos[0]] = -1;
			fp_count[p]--;
			cf_num_items--;

			insert_scrub(p, fingerprint, cf_scrub_iter);
		}
	}
}


/*
 * random_remove deletes a random element
 */

void CF::random_remove()
{
	int p1, p2;

	do {
		p1 = rand() % cf_size;
		p2 = rand() % cf_cells;

	} while (table[p1][p2] == -1);

	table[p1][p2] = -1;
	fp_count[p1]--;
	cf_num_items--;

	// the fingerprints are moved to fill the first positions of the bucket
	
	while (p2 < cf_cells - 1)
	{
		table[p1][p2] = table[p1][p2 + 1];
		table[p1][p2 + 1] = -1;
		p2++;
	}
}


/*
 * query lookups an element in the table
 */

bool CF::query(uint64_t key)
{
	int fp_value;
	int p = hash(key, 2, cf_size);

	// the fingerprint fp_value is calculated considering the fp_count[p], if the counter is less than
	// cf_cells, it takes fp_bsize (the large fingerprint), otherwise it takes fp_size (the small fingerprint)
	
	fp_value = (fp_count[p] < cf_cells) ? fp_bsize : fp_size;
	
	int fingerprint = hash(key, 1, fp_value);
	fingerprint = fingerprint % fp_value;
	
	if ((fingerprint == victim_fingerprint) && (p == victim_pointer)) return true;
	
	for (int j = 0; j < 2; j++) {
		fingerprint = hash(key, 1, fp_bsize);
		fingerprint = fingerprint % fp_bsize;

		p = hash(key, 2, cf_size) ^ (j*hash(fingerprint, 2, cf_size));
		p = p % cf_size;
		
		// all the elements of the bucket should be checked, from table[p][0] to table[p][cf_cells-1]
		
		fp_value = (fp_count[p] < cf_cells) ? fp_bsize : fp_size; 
		
		fingerprint = hash(key, 1, fp_value);
		fingerprint = fingerprint % fp_value;
		
		for (int jj = 0; jj < cf_cells; jj++) {
			if ((table[p][jj] % fp_value) == fingerprint)
				return true;
		}
	}

	return false;
}


/*
 * get_bucket_occupancy returns the average occupancy of buckets having 0, 1, 2, ... cf_cells fingerprints
 */

void CF::get_bucket_occupancy(double *&bucket_occupancy) {
	for (int i = 0; i <= cf_cells; i++)
		bucket_occupancy[i] = 0;

	for (int i = 0; i < cf_size; i++)
		bucket_occupancy[get_bucket_fingerprints(i)]++;

	for (int i = 0; i <= cf_cells; i++)
		bucket_occupancy[i] = (double)bucket_occupancy[i] / (double)cf_size;
}


/*
 * get_fingerprints returns the number of elements stored in the table
 */

int CF::get_fingerprints()
{
	int fingerprints = 0;

	for (int i = 0; i < cf_size; i++)
		for (int j = 0; j < cf_cells; j++)
			if (table[i][j] != -1)
				fingerprints++;

	return fingerprints;
};


/*
 * hash functions
 */

int CF::hash(uint64_t key, int i, int s)
{
	unsigned int  val = RSHash(key) + i * JSHash(key);
	
	if (i == 2) val = RSHash(key);
	if (i == 1) val = JSHash(key);
	
	return (val % s);
}

// RSHash function

int CF::RSHash(uint64_t key)
{
	int b = 378551;
	int a = 63689;
	int hash = 0;
	int i = 0;
	char k[8];

	memcpy(k, &key, 8);

	for (i = 0; i < 8; i++)
	{
		hash = hash * a + k[i];
		a = a * b;
	}

	return hash;
}

// JSHash function

int CF::JSHash(uint64_t key)
{
	int hash = 1315423911;
	int i = 0;
	char k[8];

	memcpy(k, &key, 8);

	for (i = 0; i < 8; i++)
	{
		hash ^= ((hash << 5) + k[i] + (hash >> 2));
	}

	return hash;
}
