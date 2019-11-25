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
//	const char *filename = "../pme/postings.bin";
	const char *filename = "../interpolative_coding/data/test_collection.docs";

	FILE *fp;
	if (NULL == (fp = fopen(filename, "rb")))
		exit(printf("Cannot open %s\n", filename));

	int listnumber = 0;
	uint length;
	int cumulativesize = 0;
	int *postings_list = new int[NUMDOCS];
	int *dgaps = new int[NUMDOCS];
	int *payload = new int [NUMDOCS];
	uint8_t *selectors = new uint8_t [MAXSELECTORS];

	FILE *compressed_lists = fopen("compressedlists.bin", "w");
	FILE *listdata = fopen("listdata.bin", "w");
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
		  Print raw dgaps
		*/
//		printf("list %d, length %d: ", listnumber, length);
//		for (int i = 0; i < length; i++)
//			printf("%d, ", dgaps[i]);
//		printf("\n");
				

		/*
		   AVX512 compression
		*/
		Pack512 whakaiti;
		int num_selectors = whakaiti.generate_selectors(selectors, dgaps, dgaps + length);
		Pack512::listrecord result = whakaiti.avx_compress(payload, selectors, num_selectors,
			dgaps, dgaps + length);

		/* 
			Write pointers to compressed payloads
		*/
		fwrite(&cumulativesize, 4, 1, locations);  

      /*
		   Write list metadata
		 */
		if (listnumber > 113300)
			printf("%d: %d, %d, %d\n", listnumber, result.dgaps_compressed, result.payload_bytes, result.selector_bytes);
		fwrite(&result.dgaps_compressed, 4, 1, listdata);
		fwrite(&result.payload_bytes, 4, 1, listdata);
		fwrite(&result.selector_bytes, 4, 1, listdata);
		
		/* 
			Serialise compressed payloads and selectors
		*/
		fwrite(payload, 1, result.payload_bytes, compressed_lists);
		cumulativesize += result.payload_bytes;

		fwrite(selectors, 1, result.selector_bytes, compressed_lists);
		cumulativesize += result.selector_bytes;

		if (listnumber == 1)
			{
			printf("length: %d, payload bytes: %d, num selectors: %d\n",
				result.dgaps_compressed, result.payload_bytes, result.num_selectors);
			for (int i = 0; i < length; i++)
				printf("%d, ", dgaps[i]);
			printf("\n");
			}

		// if (listnumber == 1)
		// 	{
		// 	printf("list zero selectors:\n");
		// 	for (int j = 0; j < num_selectors; j++)
		// 		printf("%d, ", selectors[j]);
		// 	printf("\n");
		//	}

		listnumber++;
		}
	printf("total compressed bytes: %d\n", cumulativesize);

	delete [] payload;
	delete [] selectors;
	delete [] dgaps;
	delete [] postings_list;
	fclose(fp);
	fclose(listdata);
	fclose(compressed_lists);
	fclose(locations);
		
	return 0;
	}
