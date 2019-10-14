#pragma once
#include <cstdint>

class Pack512
	{
	public:

	/*
	  Data type returned by list compressor
	 */
	struct listrecord {
	int dgaps_compressed;
	int payload_bytes;
	int selector_bytes;
	};

	/*
	  Data type used in compression and decompression of one 512-bit vector
	 */
	struct wordrecord {
	int n_compressed;
	int n_columns;
	};

	Pack512()
		{
		// nothing
		}
	
	~Pack512()
		{
		// nothing
		}
	
	public:

	/*
	  Find the column widths for the optimal packing of dgaps into a
	  16-int vector. Write those widths to the int array "selectors"
	  and return the number of selectors generated.
	 */
	int generate_selectors(int *selectors, int *dgaps, int *end);

	/*
	  Pack a postings list (dgaps) using those selectors. Write
	  compressed data to "payload"
	 */
	listrecord avx_optimal_pack(int *payload, int *selectors, int num_selectors,
		int *raw, int *end);

	/* 
		Compress both the payload and the selectors
	 */
	listrecord avx_compress(int *payload, byte *compressed_selectors,
		int *selectors, int num_selectors, int *raw, int *end);

	/*
	  Decompress a postings list. Currently using a non-compressed list
	  of selectors
	 */
	int avx_unpack_list(int *decoded, int *selectors, int num_selectors,
		int *payload, int to_decompress);


	private:

	/*
	  Find the number of bits needed
	 */
	int get_bitwidth(uint x);

	/*
	  Pack one 512-bit word
	 */
	wordrecord encode_one_word(int *payload, int *selectors, int num_selectors,
		int *raw, int *end);

	/*
	  Decompress one 512-bit word
	 */
	wordrecord decode_one_word(int *decoded, int *selectors, int num_selectors,
		int *payload, int length);

	/*
	  Run length encoding for selectors
	 */
	int run_length_encode(byte *dest, int *source, int length);

	};
