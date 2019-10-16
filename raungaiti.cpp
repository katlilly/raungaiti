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


int new_run_length_encode(uint8_t *dest, int *source, int length)
	{
	int index, current, runlength;
	int bytes_used = 0;

	index = 0;
	while (index < length)
		{
		current = source[index];
//		printf("current width: %d, ", current);
		runlength = 1;
		while (current == source[index + runlength] && index + runlength < length)
			{
//			printf("next: %d, ", source[index + runlength]);
			runlength++;
			}
//			printf("run length: %d\n", runlength);
		int repeats = runlength - 1;
		dest[bytes_used++] = current;
		dest[bytes_used++] = repeats;
		if (repeats > 255)
			exit(printf("run length can fit in a byte\n"));
		index += runlength;
		}
//	printf("bytes used: %d\n", bytes_used);
	return bytes_used;
	}


int new_run_length_decode(int *dest, uint8_t *source, int length)
	{
	int index = 0;
	int decoded_length = 0;

	while (index < length)
		{
		int value = source[index];
		int runlength = source[index + 1] + 1;
//		printf("index: %d, value: %d, runlength: %d\n", index, value, runlength);

		for (int i = 0; i < runlength; i++)
			dest[decoded_length + i] = value;
		decoded_length += runlength;
		index += 2;
		}
	
	return decoded_length;
	}


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
	int *decompressed_selectors = new int [MAXSELECTORS];
	
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

//		if (listnumber == 94)
		if (true)
			{
		/*
		   AVX512 compression
		*/
		Pack512 whakaiti;
		int num_selectors = whakaiti.generate_selectors(selectors, dgaps, dgaps + length);
		Pack512::listrecord result = whakaiti.avx_compress(payload, compressed_selectors, selectors, num_selectors, dgaps, dgaps + length);
		// also do a test with recursive compression 
		
		int raw_bytes = length * 4;
		total_raw_size += raw_bytes;
		total_compressed_size += result2.payload_bytes;
		total_compressed_size += result2.selector_bytes;
		
		
		/* 
			Decompression
		 */
		int nd = whakaiti.avx_unpack_list(decoded, selectors, num_selectors, payload, result.dgaps_compressed);
		int nd2 = whakaiti.decompress(decoded, compressed_selectors, result2.selector_bytes, payload, result.dgaps_compressed);

		printf("length: %4d, payload bytes: %4d, selector bytes: %4d\n", length, result2.payload_bytes, result2.selector_bytes);
		
		
		/* 
			Error checking
		 */
//		if (selectors_decompressed != num_selectors)
//			exit(printf("list number %d, decompressed wrong number of selectors\n", listnumber));
//		for (int i = 0; i < selectors_decompressed; i++)
//			if (decompressed_selectors[i] != selectors[i])
//				exit(printf("selector (de)compression error\n"));

		if ((uint) nd != length)
			exit(printf("decompressed wrong number of dgaps\n"));

		for (int i = 0; i < result.dgaps_compressed; i++)
			if (dgaps[i] != decoded[i])
				exit(printf("decompressed data != original\n"));

			}
		listnumber++;
		}

	printf("raw bytes:        %d\n", total_raw_size);
	printf("compressed bytes:  %d\n", total_compressed_size);

	delete [] payload;
	delete [] selectors;
	delete [] dgaps;
	delete [] postings_list;
	delete [] decoded;
	delete [] compressed_selectors;
	fclose(fp);
		
	return 0;
	}
