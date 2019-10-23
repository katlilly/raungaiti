#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>

class list_location
	{
public:
	int start;
	int length;
  
public:
	list_location() : start(0), length(0)
		{
		// nothing
		}
	};


int main(void)
	{
	/*
	  Read postings list locations
   */
	const char *filename = "locations.bin";
	FILE *fp = fopen(filename, "r");
	if (!fp)
		exit(printf("couldn't open file: \"%s\"\n", filename));
	struct stat st;
	stat(filename, &st);
	int num_elements = st.st_size / sizeof(list_location);
	list_location *locations = new list_location [num_elements];

	if (!fread(locations, 1, st.st_size, fp))
		exit(printf("failed to read in list locations\n"));
	fclose(fp);

	for (int i = 0; i < num_elements; i++)
		printf("%d: start: %d, size: %d\n", i, locations[i].start, locations[i].length);

	
   /*
	  Read in the postings lists
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
	  Decompress list #65
	 */
	
	int payload_bytes = *(compressedpostings + locations[65].start);
		int listlength = *(compressedpostings + locations[65].start + 4);
	int selector_bytes = *(compressedpostings + locations[65].start + 4 + payload_bytes);
	int numselectors = *(compressedpostings + locations[65].start + 8 + payload_bytes);
	printf("%d, %d, %d, %d\n", payload_bytes, listlength, selector_bytes, numselectors);
		

	return 0;
	}
