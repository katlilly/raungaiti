#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include "pack512.h"


class list_data
	{
public:
	int length = 0;
	int payload_bytes = 0;
	int selector_bytes = 0;

public:
	list_data() //: payload_bytes(0), length(0), selector_bytes(0)
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
		;//printf("%d: length %d, payload bytes %d, selector bytes %d\n",i, metadata[i].length, metadata[i].payload_bytes, metadata[i].selector_bytes);

	
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
	int i = 1;
	printf("start: %d\n", pointers[i]);
	int listlength = metadata[i].length;
	int payload_bytes = metadata[i].payload_bytes;
	int numselectors = metadata[i].selector_bytes;
	printf("length: %d, payload bytes: %d, selector bytes: %d\n",
		listlength, payload_bytes, numselectors);

	Pack512 whakanui;
	//allocate memory to decode into
	int *decoded = new int[listlength];
	// pointer to payload
	uint8_t *payload = compressedpostings + pointers[i];
	// pointer to selectors
	uint8_t *selectors = compressedpostings + pointers[i] + payload_bytes;
//	for (int j = 0; j < numselectors; j++)
//		printf("%d, ", selectors[j]);
//	printf("\n");
	
	int num_decompressed = whakanui.decompress(decoded, selectors, numselectors, (int *) payload, listlength);

	printf("decompressed %d dgaps\n", num_decompressed);

	for (int i = 0; i < num_decompressed; i++)
		printf("%d, ", decoded[i]);
	printf("\n");
	
	return 0;
	}
