#include <stdio.h>
#include <cstdint>


int bits_used;


void write(uint32_t value, uint32_t size)
	{
	;//printf("write %d using %d bits\n", value, size);
	}


uint32_t get_bitwidth(uint32_t x)
	{
	uint32_t r = 32;

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


void encode(uint32_t *raw, uint32_t length, uint32_t lo, uint32_t hi)
	{
	if (length < 1) return;
	uint32_t index = length / 2;
	uint32_t value = raw[index];
	uint32_t value_to_write = value - lo - index;
	uint32_t size_to_write = get_bitwidth(hi - lo - length + 1);

	for (int i = 0; i < length; i++)
		printf("%d, ", raw[i]);
	printf("\nvalue: %d, write: %d using %d bits\n\n", value, value_to_write, size_to_write);
	bits_used += size_to_write;
	write(value_to_write, size_to_write);
	
	// for the recursive calls, hi and lo are bounds, not values, so use whole length
	encode(raw, index, lo, value - 1);
	encode(raw + index + 1, length - index - 1, value + 1, hi);
	}

int main(void)
	{
	uint32_t length = 12;
	uint32_t data[length] = {3, 4, 7, 13, 14, 15, 21, 25, 36, 38, 54, 62};

	bits_used = 0;
	
	// the first time we call this we already know the largest value, so length--
	encode(data, length - 1, 0, data[length - 1]);

	printf("used %d bits to encode %d ints\n", bits_used, length);

	return 0;
	}
