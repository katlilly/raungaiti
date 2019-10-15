#include <cstdint>
#include <stdio.h>
#include <immintrin.h>
#include <math.h>
#include "pack512.h"

/*
  Find the column widths for the optimal packing of dgaps into a
  16-int vector. Write those widths to the int array "selectors"
  and return the number of selectors generated.
*/
int Pack512::generate_selectors(int *selectors, int *dgaps, int *end)
	{
	int length = end - dgaps;
	int current = 0; // index of first element in a column
	int column = 0; // index of selector

	while (current < length)
		{
		/* get width of next column */
		int column_width = 0;
		for (int row = 0; row < 16; row++)
			if (current + row < length)
				column_width |= dgaps[current + row];
				
		column_width = get_bitwidth(column_width);
		current += 16;

		/* write out this column width to selector array */
		selectors[column++] = column_width;
		}
	
	return column;
	}

/*
  Pack a postings list (dgaps) using those selectors. Write
  compressed data to "payload"
*/
Pack512::listrecord Pack512::avx_optimal_pack(int *payload, int *selectors, int num_selectors, int *raw, int *end)
	{
	listrecord list;
	list.payload_bytes = 0;
	list.dgaps_compressed = 0;
	list.num_selectors = num_selectors;

	while (raw < end)
		{
		wordrecord word = encode_one_word(payload, selectors, num_selectors, raw, end);
		payload += 16;
		selectors += word.n_columns;
		num_selectors -= word.n_columns;
		raw += word.n_compressed;
		list.dgaps_compressed += word.n_compressed;
		list.payload_bytes += 64;
		}
	
	return list;
	}

/*
  Pack a postings list (dgaps) using those selectors. Write compressed
  data to "payload" and run length encoded selectors into
  "compressed_selectors"
*/
Pack512::listrecord Pack512::avx_compress(int *payload, uint8_t *compressed_selectors, int *selectors, int num_selectors, int *raw, int *end)
	{
	listrecord list;
	list.payload_bytes = 0;
	list.dgaps_compressed = 0;
	list.num_selectors = num_selectors;
	int *start_selectors = selectors;
	int ns = num_selectors;

	while (raw < end)
		{
		wordrecord word = encode_one_word(payload, selectors, num_selectors, raw, end);
		payload += 16;
		selectors += word.n_columns;
		num_selectors -= word.n_columns;
		raw += word.n_compressed;
		list.dgaps_compressed += word.n_compressed;
		list.payload_bytes += 64;
		}

	list.selector_bytes = run_length_encode(compressed_selectors, start_selectors, ns);
	
	return list;
	}

/*
  Decompress a postings list. Currently using a non-compressed list
  of selectors
*/
int Pack512::avx_unpack_list(int *decoded, int *selectors, int num_selectors, int *payload, int to_decompress)
	{
	int num_decompressed = 0;
	wordrecord word;
	word.n_compressed = 0;
	word.n_columns = 0;
	
	while (num_selectors)
		{
		word = decode_one_word(decoded, selectors, num_selectors, payload, to_decompress);
		decoded += word.n_compressed;
		selectors += word.n_columns;
		num_selectors -= word.n_columns;
		payload += 16;
		to_decompress -= word.n_compressed;
		num_decompressed += word.n_compressed;
		}

	return num_decompressed;
	}

/* 
	Decompress both the payload and the selectors
*/
int Pack512::decompress(int *decoded, uint8_t *compressed_selectors, int selector_bytes, int *payload, int dgaps_to_decompress)
	{
	int *selectors = new int[dgaps_to_decompress];
	int num_selectors = run_length_decode(selectors, compressed_selectors, selector_bytes);
	// int num_selectors = 0;
	// for (int i = 0; i < selector_bytes; i++)
	// 	{
	// 	int value = (compressed_selectors[i] & 248) >> 3;
	// 	int repeats = compressed_selectors[i] & 7;
	// 	selectors[num_selectors++] = value;
	// 	for (int j = 0; j < repeats; j++)
	// 		selectors[num_selectors++] = value;
	// 	}
		


	int num_decompressed = 0;

	
	delete [] selectors;
	return num_decompressed;
	}


/* 
	efficiently calculate the number of bits needed for the binary
	representation. Copied from here:
	https://github.com/torvalds/linux/blob/master/include/asm-generic/bitops/fls.h
*/
int Pack512::get_bitwidth(uint x)
	{
	int r = 32;

	if (!x)
		return 1;
	if (!(x & 0xffff0000u))
		{
		x <<= 16;
		r -= 16;
		}
	if (!(x & 0xff000000u))
		{
		x <<= 8;
		r -= 8;
		}
	if (!(x & 0xf0000000u))
		{
		x <<= 4;
		r -= 4;
		}
	if (!(x & 0xc0000000u))
		{
		x <<= 2;
		r -= 2;
		}
	if (!(x & 0x80000000u))
		{
		x <<= 1;
		r -= 1;
		}
	return r;
	}

/*
  Pack one 512-bit word
*/
Pack512::wordrecord Pack512::encode_one_word(int *payload, int *selectors, int num_selectors, int *raw, int *end)
	{
	wordrecord result;
	int length = end - raw;

	__m512i compressedword = _mm512_setzero_epi32();
	__m512i indexvector = _mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
	__m512i columnvector;

	/*
	  Find how many columns to pack in this word
	*/
	int sum = 0;
	int num_columns;
	for (num_columns = 0; num_columns < 32 && num_columns < num_selectors; num_columns++)
		{
		sum += selectors[num_columns];
		if (sum > 32)
			break;
		}

	/*
	  Pack data using those columns
	*/
	int bitsused = 0;
	for (int i = 0; i < num_columns; i++)
		{
		// gather next 16 ints into a 512bit register
		columnvector = _mm512_i32gather_epi32(indexvector, raw, 4);

		// left shift input to correct "column"
		const uint8_t shift = bitsused; 
		columnvector = _mm512_slli_epi32(columnvector, shift);

		// pack this column of 16 dgaps into compressed 512bit word
		compressedword = _mm512_or_epi32(compressedword, columnvector);
		raw += 16;
		bitsused += selectors[i];
		}

	/*
	  Write compressed data to memory as 32bit ints
	*/
	_mm512_i32scatter_epi32(payload, indexvector, compressedword, 4);
	
	/*
	  Find the number of real dgaps that were compressed
	*/
	result.n_compressed = 16 * num_columns;
	if (result.n_compressed > length)
		result.n_compressed = length;
	
	result.n_columns = num_columns;

	return result;
	}

/*
  Decompress one 512-bit word
*/
Pack512::wordrecord Pack512::decode_one_word(int *decoded, int *selectors, int num_selectors, int *payload, int length)
	{
	int dgaps_decompressed = 0;
	
	/*
	  Load 512 bits of compressed data into a register
	*/
	__m512i indexvector = _mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
	__m512i compressed_word = _mm512_i32gather_epi32(indexvector, payload, 4);
	
	/* 
		Declare a 512 bit vector for decoding into
	*/
	__m512i decomp_vector;

	/*
	  Find how many columns will fit in this word
	*/
	int sum = 0;
	int num_columns;
	for (num_columns = 0; num_columns < 32 && num_columns < num_selectors; num_columns++)
		{
		sum += selectors[num_columns];
		if (sum > 32)
			break;
		}

	/* 
		Decompress 512 bits of encoded data
	*/
	for (int i = 0; i < num_columns; i++)
		{
		// get selector and create bitmask vector
		int width = selectors[i];
		int mask = pow(2, width) - 1;
		__m512i mask_vector = _mm512_set1_epi32(mask);
		
		// get 16 dgaps by ANDing mask with compressed word
		decomp_vector = _mm512_and_epi32(compressed_word, mask_vector);

		// write those 16 numbers to int array of decoded dgaps
		_mm512_i32scatter_epi32(decoded, indexvector, decomp_vector, 4);

		// right shift the remaining data in the compressed word
		compressed_word = _mm512_srli_epi32(compressed_word, width);

		dgaps_decompressed += 16;
		decoded += 16;
		}

	wordrecord result;
	result.n_columns = num_columns;
	if (dgaps_decompressed > length)
		result.n_compressed = length;
	else
		result.n_compressed = dgaps_decompressed;

	return result;
	}


int Pack512::run_length_encode(uint8_t *dest, int *source, int length)
	{
	int count = 0;
	int runlength = 0;
	for (int index = 0; index < length; )
		{
		int current = source[index];
		if (current == source[index + 1])
			{
			// keep looking end for of run
			runlength++;
			index++;
			}
		else
			{
			// have found the end of a run, now write it out
			index++;
			while(runlength > 7)
				{
				dest[count] = 0;
				dest[count] |= 7;
				dest[count] |= current << 3;
				runlength -= 8;
				count++;
				}
			dest[count] = 0;
			dest[count] |= runlength;
			dest[count] |= current << 3;
			count++;
			current = source[index];
			runlength = 0;
			}
		}
	return count; // return number of bytes written
	}

int Pack512::run_length_decode(int *decompressed, uint8_t *encoded, int length)
	{
	int count = 0;
	for (int i = 0; i < length; i++)
		{
		int value = (encoded[i] & 248) >> 3;
		int repeats = encoded[i] & 7;
		decompressed[count++] = value;
		for (int j = 0; j < repeats; j++)
			decompressed[count++] = value;
		}
	return count;  // return number of column widths decompressed
	}
