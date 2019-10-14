#include <stdio.h>
#include <cstdlib>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <immintrin.h>
#include "pack512.h"

#define NUMDOCS (1024 * 1024 * 128)
#define NUMLISTS 499692
#define MAXSELECTORS 10000


int main(int argc, char *argv[])
	{
	const char *filename = "../pme/postings.bin";
	FILE *fp;
	if (NULL == (fp = fopen(filename, "rb")))
		exit(printf("Cannot open %s\n", filename));

	int total_raw_size = 0;
	int total_compressed_size = 0;
	
	int listnumber = 0;
	uint length;
	int *postings_list = new int[NUMDOCS];
	int *dgaps = new int[NUMDOCS];
	int *payload = new int [NUMDOCS];
	int *selectors = new int [MAXSELECTORS];
	int *decoded = new int [NUMDOCS];
	uint8_t *compressed_selectors = new uint8_t [MAXSELECTORS];
	
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
		Pack512::listrecord result = whakaiti.avx_optimal_pack(payload, selectors, num_selectors, dgaps, dgaps + length);
		Pack512::listrecord result2 = whakaiti.avx_compress(payload, compressed_selectors, selectors, num_selectors, dgaps, dgaps + length);
		//Pack512::listrecord result3 = whakaiti.avx_r_compress(payload, twice_compressed_selectors, selectors, num_selectors, dgaps, dgaps + length);

		int raw_bytes = length * 4;
		total_raw_size += raw_bytes;
		total_compressed_size += result2.payload_bytes;
		total_compressed_size += result2.selector_bytes;
		
		//printf("%d, %d, %d, %d\n", raw_bytes, result2.payload_bytes, result2.selector_bytes, result2.payload_bytes + result2.selector_bytes);
		
		
		/* 
			Decompression
		 */
		int nd = whakaiti.avx_unpack_list(decoded, selectors, num_selectors, payload, result.dgaps_compressed);
		
		/* 
			Error checking
		 */
		if ((uint) nd != length)
			exit(printf("decompressed wrong number of dgaps\n"));
		for (int i = 0; i < result.dgaps_compressed; i++)
			if (dgaps[i] != decoded[i])
				exit(printf("decompressed data != original\n"));

		listnumber++;
		}

	printf("raw bytes: %d\n", total_raw_size);
	printf("compressed bytes: %d\n", total_compressed_size);

	delete [] payload;
	delete [] selectors;
	delete [] dgaps;
	delete [] postings_list;
	delete [] decoded;
	delete [] compressed_selectors;
	fclose(fp);
		
	return 0;
	}
