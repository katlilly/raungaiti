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
		int num_selectors;
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
	listrecord avx_compress(int *payload, uint8_t *compressed_selectors,
		int *selectors, int num_selectors, int *raw, int *end);

	/*
	  Unpack a postings list. Currently using a non-compressed list
	  of selectors
	*/
	int avx_unpack_list(int *decoded, int *selectors, int num_selectors, int *payload, int to_decompress);

	/* 
		Decompress both the payload and the selectors
	*/
	int decompress(int *decoded, uint8_t *compressed_selectors, int selector_bytes, int *payload, int dgaps_to_decompress);

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
	  Run length encoding for selectors, return number of compressed
	  bytes
	*/
	int run_length_encode(uint8_t *dest, int *source, int length);

	/*
	  Run length decoding for selectors, return number of selectors
	  decoded
	*/
	int run_length_decode(int *dest, uint8_t *source, int selector_bytes);

	
	};
