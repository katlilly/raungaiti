#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include "pack512.h"

// class list_location
// 	{
// public:
// 	int start;
// 	int length;
  
// public:
// 	list_location() : start(0), length(0)
// 		{
// 		// nothing
// 		}
// 	};


class list_data
	{
public:
	int length;
	int payload_bytes;
	int selector_bytes;

public:
	list_data() : payload_bytes(0), length(0), selector_bytes(0)
		{
		//nothing
		}
	};


int main(void)
	{

	/*
	  Read in pointers to compressed lists
	*/
	const char *filename = "locations.bin";
	FILE *fp = fopen(filename, "r");
	if (!fp)
		exit(printf("couldn't open file: \"%s\"\n", filename));
	struct stat st;
	stat(filename, &st);
	int num_elements = st.st_size / sizeof(int);
	int *pointers = new int [num_elements];

	if (!fread(pointers, 1, st.st_size, fp))
		exit(printf("failed to read in pointers to starts of lists\n"));
	fclose(fp);

	for (int i = 0; i < num_elements; i++)
		;//printf("%d: start: %d\n", i, pointers[i]);


	/*
	  Read in lists metadata
	*/
	filename = "listdata.bin";
	fp = fopen(filename, "r");
	list_data *metadata = new list_data [num_elements];
	if (!fread(metadata, 1, st.st_size, fp))
		exit(printf("failed to read in pointers to starts of lists\n"));
	fclose(fp);
	for (int i = 0; i < num_elements; i++)
		printf("%d: length %d, payload bytes %d, selector bytes %d\n",i, metadata[i].length, metadata[i].payload_bytes, metadata[i].selector_bytes);

	
   /*
	  Read in the compressed payloads and selectors
   */
	filename = "compressedlists.bin";
	fp = fopen(filename, "r");
	if (!fp)
		exit(printf("couldn't open file: \"%s\"\n", filename));
	stat(filename, &st);
	uint8_t *compressedpostings = (uint8_t *) malloc(st.st_size);
	if (!fread(compressedpostings, 1, st.st_size, fp))
		exit(printf("failed to read in postings lists\n"));
	fclose(fp);


	/*
	  Decompress list #1
	*/
	int i = 2;
	printf("start: %d\n", pointers[i]);
	int payload_bytes = *((int *)compressedpostings + pointers[i]);
	int listlength = *(compressedpostings + pointers[i] + 4);
	int selector_bytes = *(compressedpostings + pointers[i] + 8);
	int numselectors = *(compressedpostings + pointers[i] + 12);
	printf("payload bytes: %d, length: %d, selector bytes: %d, n selectors: %d\n", payload_bytes, listlength, selector_bytes, numselectors);

//	Pack512 whakanui;
//	int *decoded = new int[listlength];
//	uint8_t *selectors = compressedpostings + locations[i] + 12 + payload_bytes;
//	for (int j = 0; j < numselectors; j++)
//		printf("%d, ", selectors[j]);
//	printf("\n");
	//int num_decompressed = whakanui.decompress(decoded, (uint8_t *) compressed_selectors, selector_bytes, (int *) payload, listlength);


	return 0;
	}
