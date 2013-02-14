#include <amp.h>
#include <chrono>
#include <algorithm>
#include <conio.h>
#include "radix_sort.h"
#include <ppl.h>


int main()
{
	using namespace concurrency;
	accelerator default_device;
    wprintf(L"Using device : %s\n\n", default_device.get_description());
    if (default_device == accelerator(accelerator::direct3d_ref))
        printf("WARNING!! Running on very slow emulator! Only use this accelerator for debugging.\n\n");

	for(uint i = 0; i < 10; i ++)
	{
		uint num = (1<<(i+13));
		printf("Testing for %u elements: \n", num);

		std::vector<uint> data(num);

		for(uint i = 0; i < num; i ++)
		{
			data[i] = i;
		}
		std::random_shuffle(data.begin(), data.end());
		std::vector<uint> dataclone(data.begin(), data.end());


		auto start_fill = std::chrono::high_resolution_clock::now();
		array<uint> av(num, data.begin(), data.end());
		auto end_fill = std::chrono::high_resolution_clock::now();

		printf("Allocating %u random unsigned integers complete! Start GPU sort.\n", num);

		auto start_comp = std::chrono::high_resolution_clock::now();
		pal::radix_sort(av);
		av.accelerator_view.wait(); //Wait for the computation to finish
		auto end_comp = std::chrono::high_resolution_clock::now();

		auto start_collect = std::chrono::high_resolution_clock::now();
		data = av; //synchronise
		auto end_collect = std::chrono::high_resolution_clock::now();

		printf("GPU sort completed in %llu microseconds.\nData transfer: %llu microseconds, computation: %llu microseconds\n",
			std::chrono::duration_cast<std::chrono::microseconds> (end_collect-start_fill).count(), 
			std::chrono::duration_cast<std::chrono::microseconds> (end_fill-start_fill+end_collect-start_collect).count(),
			std::chrono::duration_cast<std::chrono::microseconds> (end_comp-start_comp).count());

		printf("Testing for correctness. Results are.. ");

		uint success = 1;
		for(uint i = 0; i < num; i ++)
		{
			if(data[i] != i) { success = 0; break;}
		}
		printf("%s\n", (success? "correct!" : "incorrect!"));

		data = dataclone;
		printf("Beginning CPU sorts for comparison.\n");
		start_comp = std::chrono::high_resolution_clock::now();
		std::sort(data.data(), data.data()+num);
		end_comp = std::chrono::high_resolution_clock::now();
		printf("CPU std::sort completed in %llu microseconds. \n", std::chrono::duration_cast<std::chrono::microseconds>(end_comp-start_comp).count());

		data = dataclone;
		start_comp = std::chrono::high_resolution_clock::now();
		//Note: the concurrency::parallel sorts are horribly slow if you give them vectors (i.e. parallel_radixsort(data.begin(), data.end())
		concurrency::parallel_radixsort(data.data(), data.data()+num);
		end_comp = std::chrono::high_resolution_clock::now();
		printf("CPU concurrency::parallel_sort completed in %llu microseconds. \n\n\n", std::chrono::duration_cast<std::chrono::microseconds>(end_comp-start_comp).count());

	}


	printf("Press any key to exit! \n");
	_getch();


}