#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include <thread>
#include <atomic>
#include <vector>
#include "pack512.h"

//#define DEBUG 

/*
  A class to hold the metadata read in for each list
 */
class list_data
	{
public:
	int length = 0;
	int payload_bytes = 0;
	int selector_bytes = 0;

public:
	list_data() {}
	};

/*
  A class to hold the atomic "done" flags
 */
class Flag
	{
public:
	std::atomic <int> flag;

	Flag() : flag(0) {}
	};

/*
  Decode a whole block of compressed lists with multiple threads
*/
void thread_decode(std::vector<Flag> &flags, std::vector<std::vector<uint8_t>> payloads, std::vector<std::vector<uint8_t>> selectors, list_data *metadata, std::vector<int> *out)
	{
	Pack512 whakanui;
	int *decoded = new int[10000];
		
	for (uint i = 0; i < flags.size(); i++)
		if (flags[i].flag == 0)
			{
			// find a list that needs decompressing
			int expected = 0;
			if (!flags[i].flag.compare_exchange_strong(expected, 1))
				continue;

			// decompress that list
			int num_decompressed = whakanui.decompress(decoded, selectors[i].data(), metadata[i].selector_bytes, (int *) payloads[i].data(), metadata[i].length);

			// write decompressed output so we can check for errors
			std::vector<int> currentlist(decoded, decoded + num_decompressed);
			out[i] = currentlist;
			}
	delete [] decoded;
	}

/*
  Multithreaded decoding time experiments
 */
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

	/*
	  Read in lists metadata
	*/
	filename = "listdata.bin";
	fp = fopen(filename, "r");
	stat(filename, &st);
	list_data *metadata = new list_data [num_elements];
	if (!fread(metadata, 1, st.st_size, fp))
		exit(printf("failed to read in metadata\n"));
	fclose(fp);
#ifdef DEBUG
	for (int i = 0; i < num_elements; i++)
		printf("%d: length %d, payload bytes %d, selector bytes %d\n",i, metadata[i].length, metadata[i].payload_bytes, metadata[i].selector_bytes);
#endif
	
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
	  Write the compressed data to a vector of vectors to make it a
	  fair comparison with bic decoding time experiments
	 */
	int cumulative_length = 0;
	std::vector<std::vector<uint8_t>> payloads;
	std::vector<std::vector<uint8_t>> selectors;
	for (int i = 0; i < num_elements; i++)
		{
		std::vector<uint8_t> current_payload(compressedpostings + cumulative_length, compressedpostings + cumulative_length + metadata[i].payload_bytes);
		std::vector<uint8_t> current_selector(compressedpostings + cumulative_length + metadata[i].payload_bytes, compressedpostings + cumulative_length + metadata[i].payload_bytes + metadata[i].selector_bytes);
		payloads.push_back(current_payload);
		selectors.push_back(current_selector);
		cumulative_length += metadata[i].payload_bytes + metadata[i].selector_bytes;
		}
	
	/*
	  Multithreaded decoding time experiments
	 */
	int maxthreads = 17;
	int num_repeats = 50;

	std::vector<int> *output = new std::vector<int>[num_elements];
	
	for (int num_threads = 1; num_threads < maxthreads; num_threads++)
		{
		double times[num_repeats];

		for (int repeat = 0; repeat < num_repeats; repeat++)
			{
			/*
			  Set up done flags and threads
			*/
			std::vector<std::thread> decoder_pool;
			std::vector<Flag> done(num_elements);
			for (int i = 0; i < num_threads; i++)
				decoder_pool.push_back(std::thread(thread_decode, std::ref(done), payloads, selectors, metadata, output));

			/*
			  Set up timers and start threads
			*/
			auto start = std::chrono::steady_clock::now();
			for (auto &decoder : decoder_pool)
				decoder.join();
			auto finish = std::chrono::steady_clock::now();
			std::chrono::duration<double> elapsed = finish - start;
			times[repeat] = elapsed.count();
			}

		/*
		  Print results of timing experiments
		 */
		printf("%d, ", num_threads);
		for (int i = 0; i < num_repeats; i++)
			printf("%5f, ", times[i]);
		printf("\n");
		}

#ifdef DEBUG	
	for (int i = 0; i < 10; i++)
		{
		printf("%d: %lu: ", i, output[i].size());
		for (uint j = 0; j < output[i].size(); j++)
			printf("%d, ", output[i][j]);
		printf("\n");
		}

	/*
	  Decompress a single list 
	*/
	int i = 4;
	printf("start: %d, length: %d, ", pointers[i], metadata[i].length);
	printf("payload bytes: %d, selector bytes: %d\n", metadata[i].payload_bytes, metadata[i].selector_bytes);

 	Pack512 whakanui;
 	int *decoded = new int[10000];
 	int num_decompressed = whakanui.decompress(decoded, selectors[i].data(), metadata[i].selector_bytes, (int *) payloads[i].data(), metadata[i].length);
 	printf("decompressed %d dgaps\n", num_decompressed);


 	for (int i = 0; i < num_decompressed; i++)
 		printf("%d, ", decoded[i]);
 	printf("\n");
#endif
	
	return 0;
	}
