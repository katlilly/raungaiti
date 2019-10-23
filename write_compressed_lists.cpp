#include <stdio.h>
#include <cstdlib>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <fstream>
#include <immintrin.h>
#include "pack512.h"

#define NUMDOCS (1024 * 1024 * 128)
#define NUMLISTS 499692
#define MAXSELECTORS 10000

/* 
	Serialise AVX-compressed postings lists, in order to produce data
	for testing multithreaded decompression
 */
int main(int argc, char *argv[])
	{
	const char *filename = "../pme/postings.bin";
	FILE *fp;
	if (NULL == (fp = fopen(filename, "rb")))
		exit(printf("Cannot open %s\n", filename));

	int listnumber = 0;
	uint length;
	int cumulativesize = 0;
	int *postings_list = new int[NUMDOCS];
	int *dgaps = new int[NUMDOCS];
	int *payload = new int [NUMDOCS];
	int *selectors = new int [MAXSELECTORS];
	uint8_t *compressed_selectors = new uint8_t [MAXSELECTORS];

	FILE *compressed_lists = fopen("compressedlists.bin", "w");
	FILE *locations = fopen("locations.bin", "w");
	
	while (fread(&length, sizeof(length), 1, fp) == 1)
		{
		if (fread(postings_list, sizeof(*postings_list), length, fp) != length)
			exit(printf("error reading in postings list, listnumber: %d\n", listnumber));
		
		/* 
			Convert docnums to dgaps
		*/
		dgaps[0] = postings_list[0];
		int prev = postings_list[0];
		for (uint i = 1; i < length; i++)
			{
			dgaps[i] = postings_list[i] - prev;
			prev = postings_list[i];
			}

		/*
		   AVX512 compression
		*/
		Pack512 whakaiti;
		int num_selectors = whakaiti.generate_selectors(selectors, dgaps, dgaps + length);
		Pack512::listrecord result = whakaiti.avx_compress(payload, compressed_selectors, selectors, num_selectors, dgaps, dgaps + length);

		/* 
			Serialise compressed data
		*/
		int listsize = 16 + result.payload_bytes + result.selector_bytes;
		fwrite(&cumulativesize, 4, 1, locations);  // write start location
		fwrite(&listsize, 4, 1, locations);        // write length (listsize)
		cumulativesize += listsize;

		// compressed_lists file
		fwrite(&result.payload_bytes, 4, 1, compressed_lists);
		fwrite(&result.dgaps_compressed, 4, 1, compressed_lists);
		fwrite(payload, 1, result.payload_bytes, compressed_lists);
		fwrite(&result.selector_bytes, 4, 1, compressed_lists);
		fwrite(&result.num_selectors, 4, 1, compressed_lists);
		fwrite(compressed_selectors, 1, result.selector_bytes, compressed_lists);

		
		listnumber++;
		}

	delete [] payload;
	delete [] selectors;
	delete [] dgaps;
	delete [] postings_list;
	delete [] compressed_selectors;
	fclose(fp);
	fclose(compressed_lists);
	fclose(locations);
		
	return 0;
	}
