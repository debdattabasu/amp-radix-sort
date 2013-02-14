# pragma once
typedef unsigned int uint;
#include <amp.h>

namespace pal
{
	void radix_sort(uint* start,  uint num);
	void radix_sort(concurrency::array<uint>& arr);
}

