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
	int current = 0;  // index of first element in a column
	int column = 0;   // index of selector

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
  Pack a postings list "dgaps" using the generated selectors. Write compressed
  data to "payload" and run length encoded selectors into "compressed_selectors"
*/
Pack512::listrecord Pack512::avx_compress(int *payload, uint8_t *compressed_selectors, int *selectors, int num_selectors, int *raw, int *end)
	{
	listrecord list;
	list.payload_bytes = 0;
	list.dgaps_compressed = 0;
	list.num_selectors = num_selectors;

	/* 
		Record the original values of these so can use them for
		compressing selectors after packing payload 
	 */
	int *start_selectors = selectors;
	int ns = num_selectors;

	/*
	  Pack the payloads
	 */
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

	/*
	  Compress the selectors
	 */
	if (list.payload_bytes > 7800) // should recursively compress selectors. note problem with overwriting list record
		list.selector_bytes = run_length_encode(compressed_selectors, start_selectors, ns);
	else 
		list.selector_bytes = run_length_encode(compressed_selectors, start_selectors, ns);

	return list;
	}

/* 
	Decompress both the payload and the selectors. dgaps for the
	current postings list are written to "decoded". Compressed data is
	in "payload", compressed selectors are in "compressed_selectors".
	Return number of dgaps decompressed.
*/
int Pack512::decompress(int *decoded, uint8_t *compressed_selectors, int selector_bytes, int *payload, int dgaps_to_decompress)
	{
	/*
	  Decompress the selectors
	 */
	int *decompressed_selectors = new int[dgaps_to_decompress];
	int num_selectors = run_length_decode(decompressed_selectors, compressed_selectors, selector_bytes);

	/*
	  Unpack the payload
	 */
	int num_decompressed = avx_unpack_list(decoded, decompressed_selectors, num_selectors, payload, dgaps_to_decompress);
	
	delete [] decompressed_selectors;

	return num_decompressed;
	}

/*
  Pack a postings list (dgaps) using those selectors. Write compressed
  data to "payload".  There is no compression of selectors here.
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
  Decompress a postings list. This function uses a non-compressed list
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
		/* get selector and create bitmask vector */
		int width = selectors[i];
		int mask = pow(2, width) - 1;
		__m512i mask_vector = _mm512_set1_epi32(mask);
		
		/* get 16 dgaps by ANDing mask with compressed word */
		decomp_vector = _mm512_and_epi32(compressed_word, mask_vector);

		/* write those 16 numbers to int array of decoded dgaps */
		_mm512_i32scatter_epi32(decoded, indexvector, decomp_vector, 4);

		/* right shift the remaining data in the compressed word */
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

/*
  Run length compression for selectors, return number of bytes used to
  compressed selectors
 */
int Pack512::run_length_encode(uint8_t *dest, int *source, int length)
	{
	int index, current, runlength;
	int bytes_used = 0;

	index = 0;
	while (index < length)
		{
		current = source[index];
		runlength = 1;

		while (current == source[index + runlength] && index + runlength < length)
			runlength++;

		int repeats = runlength - 1;
		dest[bytes_used++] = current;
		dest[bytes_used++] = repeats;
		if (repeats > 255)
			exit(printf("run length can't fit in a byte\n"));
		index += runlength;
		}

	return bytes_used;
	}

/*
  Decoding of run length encoded selectors, return number of selectors
  decompressed
 */
int Pack512::run_length_decode(int *dest, uint8_t *source, int length)
	{
	int index = 0;
	int decoded_length = 0;

	while (index < length)
		{
		int value = source[index];
		int runlength = source[index + 1] + 1;

		for (int i = 0; i < runlength; i++)
			dest[decoded_length + i] = value;

		decoded_length += runlength;
		index += 2;
		}
	
	return decoded_length;
	}

/* 
	Efficiently calculate the number of bits needed for the binary
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
