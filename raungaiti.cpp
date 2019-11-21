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

/* 
	Test AVX compression and decompression. Record data about
	compressed sizes.
 */
int main(int argc, char *argv[])
	{
	const char *filename = "../pme/postings.bin";
	FILE *fp;
	if (NULL == (fp = fopen(filename, "rb")))
		exit(printf("Cannot open %s\n", filename));

	int total_raw_size = 0;
	int total_compressed_size = 0;

	int large_payload_total_size = 0;  // sum of total bytes used by lists where compressed payload > 7800
	int small_payload_total_size = 0;  // sum of total bytes used by lists where compressed payload == 64 bytes
	
	int listnumber = 0;
	uint length;
	int *postings_list = new int[NUMDOCS];
	int *dgaps = new int[NUMDOCS];
	int *payload = new int [NUMDOCS];
	uint8_t *selectors = new uint8_t [MAXSELECTORS];
	int *decoded = new int [NUMDOCS];
	uint8_t *compressed_selectors = new uint8_t [MAXSELECTORS];
	
//	int *sel_selectors = new int [MAXSELECTORS];
//	int *rec_payload = new int [NUMDOCS];
//	uint8_t *compressed_rec_selectors = new uint8_t [MAXSELECTORS];

	long totalints = 0;
//	long totalbytes = 0;
	long total_payload_size = 0;
	long total_selector_size = 0;
	
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
		
		Pack512::listrecord result = whakaiti.avx_compress(payload, selectors, num_selectors, dgaps, dgaps + length);

//		if (result.payload_bytes > 7800)
//			{
//			int num_sel_selectors = whakaiti.generate_selectors(sel_selectors, selectors, selectors + num_selectors);
//			Pack512::listrecord rec_result = whakaiti.avx_compress(rec_payload, compressed_rec_selectors, sel_selectors, num_sel_selectors, selectors, selectors + num_selectors);
//			printf("old selector size: %4d bytes, new selector size: %4d bytes\n", result.selector_bytes, rec_result.selector_bytes + rec_result.payload_bytes);
//			}

			
		/*
		   Count total sizes of raw and compressed data
		 */
		int raw_bytes = length * 4;
		total_raw_size += raw_bytes;
//		if (raw_bytes < result.payload_bytes)
//			{
//			total_compressed_size += raw_bytes;
//			}
//		else
//			{
			total_compressed_size += result.payload_bytes;
			total_compressed_size += result.selector_bytes;
//			}
		total_payload_size += result.payload_bytes;
		total_selector_size += result.selector_bytes;
		totalints += length;
		
		if (result.payload_bytes == 64)
			small_payload_total_size += 64;
		else if (result.payload_bytes > 7800)
			large_payload_total_size += result.payload_bytes;

		/* 
			Decompression
		*/
		int num_decompressed = whakaiti.decompress(decoded, compressed_selectors, result.selector_bytes, payload, result.dgaps_compressed);
		
		/* 
			Error checking
		*/
		if ((uint) num_decompressed != length)
			exit(printf("decompressed wrong number of dgaps\n"));

		for (int i = 0; i < result.dgaps_compressed; i++)
			if (dgaps[i] != decoded[i])
				exit(printf("decompressed data != original\n"));
			
		listnumber++;
		}

	
	printf("raw bytes:                 %d\n", total_raw_size);
	printf("compressed bytes:           %d\n", total_compressed_size);
	printf("total bytes in large lists: %d\n", large_payload_total_size);
	printf("total bytes in short lists: %d\n", small_payload_total_size);

	double bitsperint = (double) total_compressed_size * 8 / totalints;
	double payloadbitsperint = (double) total_payload_size * 8 / totalints;
	double selectorbitsperint = (double) total_selector_size * 8 / totalints;
	printf("bits per int:          %.2f\n", bitsperint);
	printf("payload bits per int:  %.2f\n", payloadbitsperint);
	printf("selector bits per int:  %.2f\n", selectorbitsperint);

	
	delete [] payload;
	delete [] selectors;
	delete [] dgaps;
	delete [] postings_list;
	delete [] decoded;
	delete [] compressed_selectors;
	fclose(fp);
		
	return 0;
	}
