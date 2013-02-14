#include <amp.h>
#include <iterator>
#include "radix_sort.h"


void arr_fill(concurrency::array_view<uint> &dest, concurrency::array_view<uint>& src,  uint val) 
{ 
	parallel_for_each(dest.extent,[dest ,val, src](concurrency::index<1> idx)restrict(amp)
	{
		dest[idx] = ( (uint)idx[0] <src.get_extent().size())? src[idx]: val; 
	}); 
}

uint get_bits(uint x, uint numbits, uint bitoffset) restrict(amp)
{
	return  (x>>bitoffset) & ~(~0 <<numbits);
}

uint pow2(uint x) restrict(amp,cpu)
{
	return ( ((uint)1) << x);
}

uint tile_sum(uint x, concurrency::tiled_index<256> tidx) restrict(amp)
{
	using namespace concurrency;
	uint l_id = tidx.local[0];
	tile_static uint l_sums[256][2];
		
	l_sums[l_id][0] = x;
	tidx.barrier.wait();

	for(uint i = 0; i < 8; i ++)
	{
		if(l_id<  pow2(7-i))
		{
			uint w = (i+1)%2;
			uint r = i%2;

			l_sums[l_id][w] = l_sums[l_id*2][r] + l_sums[l_id*2 +1][r];
		}
		tidx.barrier.wait();
	}
	return l_sums[0][0];
		
}

uint tile_prefix_sum(uint x, concurrency::tiled_index<256> tidx,  uint& last_val ) restrict(amp)
{
	using namespace concurrency;
	uint l_id = tidx.local[0];
	tile_static uint l_prefix_sums[256][2];

	l_prefix_sums[l_id][0] = x;
	tidx.barrier.wait();

	for(uint i = 0; i < 8; i ++)
	{
		uint pow2i = pow2(i);

		uint w = (i+1)%2;
		uint r = i%2;
			
		l_prefix_sums[l_id][w] = (l_id >= pow2i)? ( l_prefix_sums[l_id][r] + l_prefix_sums[l_id - pow2i][r]) : l_prefix_sums[l_id][r] ;
		
		tidx.barrier.wait();
	}
	last_val = l_prefix_sums[255][0];
	
	uint retval = (l_id ==0)? 0: l_prefix_sums[l_id -1][0];
	return retval;
}

uint tile_prefix_sum(uint x, concurrency::tiled_index<256> tidx) restrict(amp)
{
	uint ll=0;
	return tile_prefix_sum(x, tidx, ll);
}


void calc_interm_sums(uint bitoffset, concurrency::array<uint> & interm_arr, 
					  concurrency::array<uint> & interm_sums, concurrency::array<uint> & interm_prefix_sums, uint num_tiles)
{
	using namespace concurrency;
	auto ext = extent<1>(num_tiles*256).tile<256>();

	parallel_for_each(ext , [=, &interm_sums, &interm_arr](tiled_index<256> tidx) restrict(amp)
	{
		uint inbound = ((uint)tidx.global[0]<interm_arr.get_extent().size());
		uint num = (inbound)? get_bits(interm_arr[tidx.global[0]], 2, bitoffset): get_bits(0xffffffff, 2, bitoffset);
		for(uint i = 0; i < 4; i ++)
		{
			uint to_sum = (num == i);
			uint sum = tile_sum(to_sum, tidx);

			if(tidx.local[0] == 0)
			{
				interm_sums[i*num_tiles + tidx.tile[0]] = sum;
			}
		}

	});
	
	uint numiter = (num_tiles/64) + ((num_tiles%64 == 0)? 0:1);
	ext = extent<1>(256).tile<256>();
	parallel_for_each(ext , [=, &interm_prefix_sums, &interm_sums](tiled_index<256> tidx) restrict(amp)
	{
		uint last_val0 = 0;
		uint last_val1 = 0;
	
		for(uint i = 0; i < numiter; i ++)
		{
			uint g_id = tidx.local[0] + i*256;
			uint num = (g_id<(num_tiles*4))? interm_sums[g_id]: 0;
			uint scan = tile_prefix_sum(num, tidx, last_val0);
			if(g_id<(num_tiles*4)) interm_prefix_sums[g_id] = scan + last_val1;

			last_val1 += last_val0;
		}

	});
}

void sort_step(uint bitoffset, concurrency::array<uint> & src, concurrency::array<uint> & dest,  
			   concurrency::array<uint> & interm_prefix_sums, uint num_tiles)
{
	using namespace concurrency;
	auto ext = extent<1>(num_tiles*256).tile<256>();

	parallel_for_each(ext , [=, &interm_prefix_sums, &src, &dest](tiled_index<256> tidx) restrict(amp)
	{
		uint inbounds = ((uint)tidx.global[0]<src.get_extent().size());
		uint element = (inbounds)? src[tidx.global[0]] : 0xffffffff;
		uint num = get_bits(element, 2,bitoffset);
		for(uint i = 0; i < 4; i ++)
		{
			uint scan = tile_prefix_sum((num == i), tidx) + interm_prefix_sums[i*num_tiles + tidx.tile[0]];
			if(num==i && inbounds) dest[scan] = element;
		}

	});
}

namespace pal
{
	void radix_sort(concurrency::array<uint>& arr)
	{
		using namespace concurrency;
		uint size = arr.get_extent().size();

		const uint num_tiles = (size/256) + ((size%256 == 0)? 0:1);

		array<uint> interm_arr(size);
		array<uint> interm_sums(num_tiles*4);
		array<uint> interm_prefix_sums(num_tiles*4);

		for(uint i = 0; i < 16; i ++)
		{
			array<uint>& src  = (i%2==0)? arr: interm_arr;
			array<uint>& dest = (i%2==0)? interm_arr: arr;

			uint bitoffset = i*2;
			calc_interm_sums(bitoffset, src, interm_sums, interm_prefix_sums, num_tiles);
			sort_step(bitoffset, src, dest, interm_prefix_sums, num_tiles);
		}
	}

	void radix_sort(uint* arr,  uint size)
	{
		radix_sort(concurrency::array<uint>(size, arr));
	}
}