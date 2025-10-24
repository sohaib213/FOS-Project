#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"

uint32 programmsSizes[KHP_PAGES_NUMBER];
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 kheap_init [GIVEN]
//Remember to initialize locks (if any)
void kheap_init()
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		initialize_dynamic_allocator(KERNEL_HEAP_START, KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE);
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
		kheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		kheapPageAllocBreak = kheapPageAllocStart;
	}
	//==================================================================================
	//==================================================================================
	memset(programmsSizes, 0, KHP_PAGES_NUMBER * sizeof(uint32));

}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = alloc_page(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE), PERM_WRITEABLE, 1);
	if (ret < 0)
		panic("get_page() in kern: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	unmap_frame(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE));
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================
void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #1 kmalloc
	//Your code is here
	//Comment the following line
	// kpanic_into_prompt("kmalloc() is not implemented yet...!!");

	bool lock_already_held = holding_kspinlock(&MemFrameLists.mfllock);

	if (!lock_already_held)
	{
		acquire_kspinlock(&MemFrameLists.mfllock);
	}

	if(size > (KERNEL_HEAP_MAX - kheapPageAllocStart))
	{
		return NULL;
	}

	if(size < DYN_ALLOC_MAX_BLOCK_SIZE)
	{
		int* a = alloc_block(size);
	}else
	{

		unsigned int maxGap = 0, maxGapAddress;
		unsigned int pagesNeeded = ROUNDUP((uint32)size, PAGE_SIZE) / PAGE_SIZE;
		int currentPagesNumber = 0;
		unsigned int resultAddress = 0;
		unsigned int startAdressOfLastFreePages = kheapPageAllocStart;


		for(uint32 currentAddress = kheapPageAllocStart; currentAddress < kheapPageAllocBreak; currentAddress += PAGE_SIZE)
		{

			uint32* ptr_table;
			struct FrameInfo* ptr_fi = get_frame_info(ptr_page_directory, currentAddress, &ptr_table);
			if (ptr_fi != NULL) {
				startAdressOfLastFreePages = currentAddress + PAGE_SIZE;
				if(currentPagesNumber == pagesNeeded)
				{
					resultAddress = currentAddress;
					break;
				}
				if(currentPagesNumber > maxGap)
				{

					maxGap = currentPagesNumber;
					maxGapAddress = currentAddress;
					currentPagesNumber = 0;
				}
			}
			currentPagesNumber++;
		}
		if(resultAddress == 0)
		{

			if(maxGap >= pagesNeeded)
			{
				resultAddress = maxGapAddress;
			}

			else{
				if(KERNEL_HEAP_MAX - startAdressOfLastFreePages < size)
				{
					return NULL;
				}

				kheapPageAllocBreak = startAdressOfLastFreePages + size;
				resultAddress = startAdressOfLastFreePages;
			}
		}
		// link result address with pages
		for(uint32 currentAddress = resultAddress; currentAddress < resultAddress + size; currentAddress += PAGE_SIZE)
		{
			int ret = alloc_page(ptr_page_directory, currentAddress, PERM_PRESENT | PERM_WRITEABLE, 0);
			if(ret != 0)
			{
				return NULL;
			}
		}

		if (!lock_already_held)
		{
			release_kspinlock(&MemFrameLists.mfllock);
		}
		programmsSizes[(resultAddress - KERNEL_HEAP_START) / PAGE_SIZE] = size;
		return (void*)resultAddress;
	}


	//TODO: [PROJECT'25.BONUS#3] FAST PAGE ALLOCATOR
	return (void*) 22;
}

//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void kfree(void* virtual_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #2 kfree
	//Your code is here
	//Comment the following line
	panic("kfree() is not implemented yet...!!");
}

//=================================
// [3] FIND VA OF GIVEN PA:
//=================================
unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #3 kheap_virtual_address
	//Your code is here
	//Comment the following line
	// panic("kheap_virtual_address() is not implemented yet...!!");

	unsigned int offset = physical_address & 0X00000FFF;
	physical_address = physical_address & 0xFFFFF000;

	struct FrameInfo* ptr_fi = to_frame_info(physical_address);
	if(ptr_fi -> references == 0)
		return 0;
	return ptr_fi->virtual_address | offset;
	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================
unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #4 kheap_physical_address
	//Your code is here
	//Comment the following line
	// panic("kheap_physical_address() is not implemented yet...!!");
	uint32* ptr_table ;
	uint32 ret =  get_page_table(ptr_page_directory, virtual_address, ptr_table) ;
	if((*ptr_table) != 0)
	{
		uint32 index_page_table = PTX(virtual_address);
		uint32 page_table_entry = ptr_table[index_page_table];

		if( ((page_table_entry & ~0xFFF) != 0) || ((page_table_entry & (PERM_PRESENT|PERM_BUFFERED)) != 0))
		{
			return EXTRACT_ADDRESS ( page_table_entry );
		}
	}
	return 0;
	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

extern __inline__ uint32 get_block_size(void *va);

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	panic("krealloc() is not implemented yet...!!");
}
