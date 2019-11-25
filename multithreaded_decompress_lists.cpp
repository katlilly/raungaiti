#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include <thread>
#include <atomic>
#include <vector>
#include "pack512.h"

//#define DEBUG 


class list_data
	{
public:
	int length = 0;
	int payload_bytes = 0;
	int selector_bytes = 0;

public:
	list_data() {}
	};


class Flag
	{
public:
	std::atomic <int> flag;

	Flag() : flag(0) {}
	};


void thread_decode(std::vector<Flag> &flags, std::vector<std::vector<uint8_t>> payloads, std::vector<std::vector<uint8_t>> selectors, list_data *metadata)
	{
	Pack512 whakanui;
	int *decoded = new int[10000];
	
	for (uint i = 0; i < flags.size(); i++)
		if (flags[i].flag == 0)
			{
			int expected = 0;
			if (!flags[i].flag.compare_exchange_strong(expected, 1))
				continue;

			int num_decompressed = whakanui.decompress(decoded, selectors[i].data(), metadata[i].selector_bytes, (int *) payloads[i].data(), metadata[i].length);
			}
	}


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
	if (!fread(metadata, 3, st.st_size, fp))
		exit(printf("failed to read in metadata\n"));
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
	  Write the compressed data to a vector of vectors to make it a
	  fair comparison with bic decoding time experiments
	 */
	int cumulative_length = 0;
	std::vector<std::vector<uint8_t>> payloads;
	std::vector<std::vector<uint8_t>> selectors;
	for (int i = 0; i < num_elements; i++)
		{
		int this_payload_length = metadata[i].payload_bytes;
		int this_selector_length =  metadata[i].selector_bytes;
		std::vector<uint8_t> current_payload(compressedpostings + cumulative_length, compressedpostings + cumulative_length + this_payload_length);
		std::vector<uint8_t> current_selector(compressedpostings + cumulative_length + this_payload_length, compressedpostings + cumulative_length + this_payload_length + this_selector_length);
		payloads.push_back(current_payload);
//		if (this_payload_length == 0)
//			exit(printf("%d\n", i));
//		printf("%d: %d %d\n", i, this_payload_length, this_selector_length);
		selectors.push_back(current_selector);
		cumulative_length += this_payload_length;
		cumulative_length += this_selector_length;
		}
	
	/*
	  Multithreaded decoding time experiments
	 */
	int maxthreads = 17;
	int num_repeats = 20;
	for (int num_threads = 1; num_threads < maxthreads; num_threads++)
		{
		double times[num_repeats];

		for (int repeat = 0; repeat < num_repeats; repeat++)
			{
			/*
			  Set up flags and threads
			*/
			std::vector<std::thread> decoder_pool;
			std::vector<Flag> done(num_elements);
			for (int i = 0; i < num_threads; i++)
				decoder_pool.push_back(std::thread(thread_decode, std::ref(done), payloads, selectors, metadata));

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

	/*
	  Decompress a single list 
	*/
	// int i = 1;
	// printf("start: %d\n", pointers[i]);
	// int listlength = metadata[i].length;
	// int payload_bytes = metadata[i].payload_bytes;
	// int numselectors = metadata[i].selector_bytes;
	// printf("length: %d, payload bytes: %d, selector bytes: %d\n",
	// 	listlength, payload_bytes, numselectors);

	// Pack512 whakanui;
	// //allocate memory to decode into
	// int *decoded = new int[listlength];
	// // point to compressed payload and selectors
	// uint8_t *payload = compressedpostings + pointers[i];
	// uint8_t *selectors = compressedpostings + pointers[i] + payload_bytes;
	
	// int num_decompressed = whakanui.decompress(decoded, selectors, numselectors,
	// 	(int *) payload, listlength);

	// printf("decompressed %d dgaps\n", num_decompressed);

	// #ifdef DEBUG
	// for (int i = 0; i < num_decompressed; i++)
	// 	printf("%d, ", decoded[i]);
	// printf("\n");
	// #endif
	
	return 0;
	}
