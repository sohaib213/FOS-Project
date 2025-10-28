#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"

struct pageInfo pagesInfo[KHP_PAGES_AREA_NUMBER];
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
	// memset(programmsSizes, 0, KHP_PAGES_AREA_NUMBER * sizeof(uint32));

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

	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE)
	{
		int* a = alloc_block(size);
	}else
	{

		size = ROUNDUP(size, PAGE_SIZE);
		unsigned int maxSize = 0, maxSizeAddress;
		unsigned int resultAddress = 0, lastAddress = 0;

		int i = 0;
		for(uint32 currentAddress = kheapPageAllocStart; currentAddress < kheapPageAllocBreak;)
		{
			i++;
			struct pageInfo *currentPageInfo = &pagesInfo[getPagesInfoIndex(currentAddress)];
			if (!currentPageInfo->isBlocked) {
				if(size == currentPageInfo->size) // CUSTOM FIT FOUND
				{
					resultAddress = currentAddress;
					currentPageInfo->isBlocked = 1;
					break;
				}
				if(currentPageInfo->size > size && currentPageInfo->size > maxSize)
				{
					maxSize = currentPageInfo->size;
					maxSizeAddress = currentAddress;
				}
			}
			lastAddress = currentAddress;
			currentAddress += currentPageInfo->size;
		}
		if(resultAddress == 0)
		{
			if(maxSize != 0)
			{
				resultAddress = maxSizeAddress;
				struct pageInfo *maxSizePage = &pagesInfo[getPagesInfoIndex(maxSizeAddress)], *nextPage, *splitAddress;
				splitAddress = &pagesInfo[getPagesInfoIndex(maxSizeAddress + size)];
				nextPage = &pagesInfo[getPagesInfoIndex(maxSizeAddress + maxSizePage->size)];
	
				splitAddress->size = maxSizePage->size - size;
				splitAddress->prevPageStartAddress = maxSizeAddress;
	
				nextPage->prevPageStartAddress = maxSizeAddress + size;
	
				maxSizePage->isBlocked = 1;
				maxSizePage->size = size;
			}
			else{
				if(KERNEL_HEAP_MAX - kheapPageAllocBreak < size)
				{
					return NULL;
				}
	
				resultAddress = kheapPageAllocBreak;
				struct pageInfo *page = &pagesInfo[getPagesInfoIndex(resultAddress)];
				page->isBlocked = 1;
				page->size = size;
				page->prevPageStartAddress = lastAddress;
	
				kheapPageAllocBreak += size;
			}
		}
		// link result address with pages
		bool a = allocFrames(resultAddress, resultAddress + size);
		if(!a)
		{
			return NULL;
		}


		if (!lock_already_held)
		{
			release_kspinlock(&MemFrameLists.mfllock);
		}
		return (void*)resultAddress;
	}
	
	if (!lock_already_held)
	{
		release_kspinlock(&MemFrameLists.mfllock);
	}
	return NULL;
	//TODO: [PROJECT'25.BONUS#3] FAST PAGE ALLOCATOR
}

//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void kfree(void* virtual_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #2 kfree
	//Your code is here
	//Comment the following line
	// panic("kfree() is not implemented yet...!!");

		
	bool lock_already_held = holding_kspinlock(&MemFrameLists.mfllock);

	if (!lock_already_held)
	{
			acquire_kspinlock(&MemFrameLists.mfllock);
	}

	if(virtual_address == NULL)
	{
			if (!lock_already_held)
			{
				release_kspinlock(&MemFrameLists.mfllock);
			}
			panic("kfree() error: NULL pointer passed!");
	}


	uint32 va = (uint32)virtual_address;

	// ========================================
	// Case 1: Small Block (Dynamic Allocator)
	// ========================================
	if(va >= KERNEL_HEAP_START && va < dynAllocEnd)
	{
			// This is a small block allocation
			free_block(virtual_address);

			if (!lock_already_held)
			{
					release_kspinlock(&MemFrameLists.mfllock);
			}
			return;
	}

	// ========================================
	// Case 2: Large Block (Page Allocator)
	// ========================================
  if(va >= kheapPageAllocStart && va < KERNEL_HEAP_MAX)
	{

		uint32 block_start = ROUNDDOWN(va, PAGE_SIZE);

		struct pageInfo *pageToDelete = &pagesInfo[getPagesInfoIndex(block_start)];

		// If size is 0, this block was not allocated
		if(pageToDelete->size == 0 || !pageToDelete->isBlocked)
		{
				if (!lock_already_held)
						release_kspinlock(&MemFrameLists.mfllock);
				panic("kfree() error: trying to free unallocated block!");
		}

		// Calculate the number of pages to free
		uint32 block_end = block_start + pageToDelete->size;

		// Free all frames for this block
		for(uint32 addr = block_start; addr < block_end; addr += PAGE_SIZE)
		{
			unmap_frame(ptr_page_directory, addr);
		}

		bool foundBeforePageEmpty = 0;
		uint32 sizeDeleted = pageToDelete->size;
		struct pageInfo *pageBeforeDeleted;
		if(block_start != kheapPageAllocStart) // check if there a free block before it
		{
			pageBeforeDeleted = &pagesInfo[getPagesInfoIndex(pageToDelete->prevPageStartAddress)];
			if(!pageBeforeDeleted -> isBlocked)
			{
				pageBeforeDeleted->size += sizeDeleted;
				block_start = pageToDelete->prevPageStartAddress;
				pageToDelete->size = 0;
				foundBeforePageEmpty = 1;
			}
		}

		struct pageInfo *pageAfterDeleted;
		if(foundBeforePageEmpty)
		{
			pageAfterDeleted = &pagesInfo[getPagesInfoIndex(block_start + pageBeforeDeleted->size)];
		}
		else
		{
			pageAfterDeleted = &pagesInfo[getPagesInfoIndex(block_start + pageToDelete->size)];
		}
		if(pageAfterDeleted -> size != 0) // chek if the page deleted is not before break directly
		{
			if(pageAfterDeleted->isBlocked)
			{
				pageAfterDeleted->prevPageStartAddress = block_start;
			}
			else
			{
				struct pageInfo *pageAfterAfter;
				if(foundBeforePageEmpty)
				{
					pageBeforeDeleted->size += pageAfterDeleted->size;
					pagesInfo[getPagesInfoIndex(block_start + pageBeforeDeleted->size)].prevPageStartAddress = block_start;

				}
				else
				{
					pageToDelete->size += pageAfterDeleted->size;
					pagesInfo[getPagesInfoIndex(block_start + pageToDelete->size)].prevPageStartAddress = block_start;
				}
				pageAfterDeleted->size = 0;
			}
		}else{
			kheapPageAllocBreak = block_start;
			if(foundBeforePageEmpty)
			{
				pageBeforeDeleted->size = 0;
			}
			else
			{
				pageToDelete->size = 0;
			}
		}
		
		pageToDelete->isBlocked = 0;
	}
	else{
		panic("kfree() error: invalid virtual address!");
	}
	// If we reach here, the address is invalid
	if (!lock_already_held)
	{
		release_kspinlock(&MemFrameLists.mfllock);
	}
	return;
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
	uint32 ret =  get_page_table(ptr_page_directory, virtual_address, &ptr_table) ;
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
	// bool lock_already_held = holding_kspinlock(&MemFrameLists.mfllock);

	// if (!lock_already_held)
	// {
	// 	acquire_kspinlock(&MemFrameLists.mfllock);
	// }
	// if(virtual_address == NULL)
	// {
	// 	return (void *)kmalloc(new_size);
	// }
	
	// if(new_size == 0)
	// {
	// 	kfree(virtual_address);
	// 	return virtual_address;
	// }

	// if(new_size < DYN_ALLOC_MAX_BLOCK_SIZE)
	// {
	// 	// handle blocks
	// }

	// uint32 oldSize = programmsSizes[((uint32) - kheapPageAllocStart) / PAGE_SIZE];
	// if(oldSize == 0)
	// {
	// 	panic("There is no old process to reallocate");
	// }
	// if(new_size == oldSize)
	// {
	// 	return virtual_address;
	// }

	// if(new_size > oldSize)
	// {
	// 	oldSize = ROUNDUP(oldSize, PAGE_SIZE);
	// 	// alloc more frames
	// 	uint32 pagesNeeded = ROUNDUP((new_size - oldSize), PAGE_SIZE) / PAGE_SIZE;
	// 	uint32 pagesUpoveBreak = 0;
	// 	uint32 addressOfTheStartFreePagesBelow = ROUNDDOWN((uint32)virtual_address, PAGE_SIZE);
	// 	uint32 addressOfTheEndFreePagesAbove = addressOfTheStartFreePagesBelow + oldSize;

	// 	while(addressOfTheStartFreePagesBelow > kheapPageAllocStart)
	// 	{
	// 		addressOfTheStartFreePagesBelow -= PAGE_SIZE;
	// 		uint32* ptr_table;
	// 		struct FrameInfo* ptr_fi = get_frame_info(ptr_page_directory, addressOfTheStartFreePagesBelow, &ptr_table);
	// 		if (ptr_fi == NULL) {
	// 			pagesNeeded--;
	// 			if(pagesNeeded == 0)
	// 			{
	// 				break;
	// 			}
	// 		}
	// 		else{
	// 			addressOfTheStartFreePagesBelow += PAGE_SIZE;
	// 			break;
	// 		}
	// 	}
	// 	bool reachedToBreak = 0;
	// 	if(pagesNeeded != 0)
	// 	{
	// 		while(1)
	// 		{
	// 			if(addressOfTheEndFreePagesAbove == kheapPageAllocBreak)
	// 			{
	// 				reachedToBreak = 1;
	// 				break;
	// 			}
				
	// 			uint32* ptr_table;
	// 			struct FrameInfo* ptr_fi = get_frame_info(ptr_page_directory, addressOfTheEndFreePagesAbove, &ptr_table);
	// 			if (ptr_fi == NULL) {
	// 				pagesNeeded--;
	// 				addressOfTheEndFreePagesAbove += PAGE_SIZE;
	// 				if(pagesNeeded == 0)
	// 				{
	// 					break;
	// 				}
	// 			}
	// 			else{
	// 				break;
	// 			}
	// 		}
	// 	}
	// 	if(pagesNeeded != 0 && reachedToBreak)
	// 	{
	// 		uint32 pagesBetweenBreakAndHeapMax = (KERNEL_HEAP_MAX - kheapPageAllocBreak) / PAGE_SIZE;
	// 		if(pagesBetweenBreakAndHeapMax >= pagesNeeded)
	// 		{
	// 			kheapPageAllocBreak += pagesNeeded * PAGE_SIZE;
	// 			addressOfTheEndFreePagesAbove = kheapPageAllocBreak;
	// 			pagesNeeded = 0;
	// 		}
	// 	}

	// 	if(pagesNeeded == 0)
	// 	{
	// 		bool a = allocFrames(addressOfTheStartFreePagesBelow, ROUNDDOWN((uint32)virtual_address, PAGE_SIZE));
	// 		if(!a)
	// 		{
	// 			return NULL;
	// 		}
	// 		a = allocFrames(ROUNDDOWN((uint32)virtual_address, PAGE_SIZE) + oldSize, addressOfTheEndFreePagesAbove);
	// 		if(!a)
	// 		{
	// 			//free upove linked frames
	// 			return NULL;
	// 		}
	// 	}else{
	// 		// free old linked frames
	// 		// will replace the prgramm in another valid place
	// 		return kmalloc(new_size);
	// 	}
	// }

	// // free frames


	// if (!lock_already_held)
	// {
	// 	release_kspinlock(&MemFrameLists.mfllock);
	// }

	// return NULL;
}

bool allocFrames(uint32 start, uint32 end){
	for(uint32 currentAddress = start; currentAddress < end; currentAddress += PAGE_SIZE)
	{
		int ret = alloc_page(ptr_page_directory, currentAddress, PERM_PRESENT | PERM_WRITEABLE, 0);
		if(ret != 0)
		{
			// free recent allocated frames
			return 0;
		}
	}
	return 1;
}

uint32 getPagesInfoIndex(uint32 address){
	return (address - kheapPageAllocStart) / PAGE_SIZE;
}
