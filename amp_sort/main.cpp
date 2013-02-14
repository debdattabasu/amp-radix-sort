#include <amp.h>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <conio.h>
#include "radix_sort.h"
#include <ppl.h>



int main()
{
	using namespace concurrency;
	accelerator default_device;
    std::wcout << "Using device : " << default_device.get_description() << std::endl<<std::endl;
    if (default_device == accelerator(accelerator::direct3d_ref))
        std::wcout << "WARNING!! Running on very slow emulator! Only use this accelerator for debugging." << std::endl<<std::endl;

	for(uint i = 0; i < 10; i ++)
	{
		uint num = (1<<(i+13));
		std::cout<<"Testing for "<<num<<" elements: \n"<<std::endl;
		std::cout<<"Allocating "<< num <<" random unsigned integers."<< std::endl; 

		uint *data = new uint[num];
		for(uint i = 0; i < num; i ++)
		{
			data[i] = i;
		}
		std::random_shuffle(data, data+num);
		array_view<uint> av(num, data);
		std::cout<<"Allocating "<< num <<" random unsigned integers complete! Begin GPU data transfer."<< std::endl; 

		auto start = std::chrono::high_resolution_clock::now();
		av.synchronize();
		auto end = std::chrono::high_resolution_clock::now();

		std::cout<<"GPU data transfer complete in "<<std::chrono::duration_cast<std::chrono::microseconds>(end-start).count()<<
			" microseconds! Begin GPU sort." <<std::endl;

		start = std::chrono::high_resolution_clock::now();
		pal::radix_sort(av);
		end = std::chrono::high_resolution_clock::now();
		std::cout<<"GPU sort complete in "<<std::chrono::duration_cast<std::chrono::microseconds>(end-start).count()<<
			" microseconds! Begin test for correctness." <<std::endl;

		av.synchronize();
		uint success = 1;
		for(uint i = 0; i < num; i ++)
		{
			if(data[i] != i) { success = 0; break;}
		}
		std::cout<<"Test for correctness complete! "<<"Test was a "<<(success? "success!" : "failure!")<<"\n";
		std::random_shuffle(data, data+num);
		std::cout<<"Beginning std::sort for comparison.\n";
		start = std::chrono::high_resolution_clock::now();
		std::sort(data, data+num);
		end = std::chrono::high_resolution_clock::now();
		std::cout<<"std::sort complete in "<<std::chrono::duration_cast<std::chrono::microseconds>(end-start).count()<<
			" microseconds! \n";

		std::cout<<"Beginning concurrency::parallel_radixsort for comparison.\n";
		start = std::chrono::high_resolution_clock::now();
		concurrency::parallel_radixsort(data, data+num);
		end = std::chrono::high_resolution_clock::now();
		std::cout<<"concurrency::parallel_radixsort sort complete in "<<std::chrono::duration_cast<std::chrono::microseconds>(end-start).count()<<
			" microseconds! \n\n\n";



		delete[] data;

	}

	
	std::cout<<"Press any key to exit! \n";
	_getch();
	

}