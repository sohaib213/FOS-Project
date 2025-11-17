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
		uint32* a = alloc_block(size);
		if(a == NULL)
			return NULL;
		else
			return (void *)a;
	}else
	{

		size = ROUNDUP(size, PAGE_SIZE);
		unsigned int maxSize = 0, maxSizeAddress;
		unsigned int resultAddress = 0, lastAddress = 0;

		int i = 0;
		// cprintf("Before \n");
		// cprintf("kheapPageAllocBreak = %p \n", kheapPageAllocBreak);
		
		for(uint32 currentAddress = kheapPageAllocStart; currentAddress < kheapPageAllocBreak;)
		{
			// cprintf("currentAddress = %p\n", currentAddress);
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
		// cprintf("After \n");
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
		// cprintf("Result address = %p\n", resultAddress);
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
	if(ptr_fi -> virtual_address == 0)
		return 0;
	return ptr_fi->virtual_address | offset;
	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================
unsigned int kheap_physical_address(unsigned int virtual_address)
{
	// cprintf("virtual address = %p\n", virtual_address);
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #4 kheap_physical_address
	//Your code is here
	//Comment the following line
	// panic("kheap_physical_address() is not implemented yet...!!");
	uint32* ptr_table ;
	uint32 ret =  get_page_table(ptr_page_directory, virtual_address, &ptr_table) ;
	if(ptr_table != NULL)
	{
		// cprintf("THERE IS TABLE\n");
		uint32 index_page_table = PTX(virtual_address);
		uint32 page_table_entry = ptr_table[index_page_table];
		uint32 offset = virtual_address & 0xFFF;

		if((EXTRACT_ADDRESS(page_table_entry) != 0) || ((page_table_entry & (PERM_PRESENT|PERM_BUFFERED)) != 0))
		{
			// cprintf("DID = %p\n", virtual_address);
			return EXTRACT_ADDRESS ( page_table_entry ) | offset;
		}
	}
	// cprintf("virtual address failed = %p\n", virtual_address);
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
	// cprintf("kheapPageAllocBreak from krealloc = %p \n", kheapPageAllocBreak);

	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	// panic("krealloc() is not implemented yet...!!");
	bool lock_already_held = holding_kspinlock(&MemFrameLists.mfllock);

	if (!lock_already_held)
	{
		acquire_kspinlock(&MemFrameLists.mfllock);
	}
	// cprintf("Size = %d\n", new_size);
	if(virtual_address == NULL)
	{
		return kmalloc(new_size);
	}
	
	if(new_size == 0)
	{
		kfree(virtual_address);
		return NULL;
	}
	
	uint32 va = (uint32)virtual_address;
	// new size is block
	if(new_size <= DYN_ALLOC_MAX_BLOCK_SIZE)
	{
		uint32* vaResult = alloc_block(new_size);
		if(vaResult == NULL)
		{
			return NULL;
		}
		// old size was block
		if(va >= dynAllocStart && va < dynAllocEnd)
		{
			// cprintf("new size = %d\n", new_size);
			// cprintf("block size = %d\n\n", get_block_size(vaResult));
			memcpy((void *)vaResult, (const void*)va, get_block_size(virtual_address));
			free_block(virtual_address);
		}
		else
		{
			memcpy((void *)vaResult, (const void*)va, pagesInfo[getPagesInfoIndex(va)].size);
			kfree(virtual_address);
		}
		return (void *) vaResult;
	}

	new_size = ROUNDUP(new_size, PAGE_SIZE);
	// old size was block
	if(va >= dynAllocStart && va <= dynAllocEnd)
	{
		void * vaRes = kmalloc(new_size);
		if(vaRes == NULL)
		{
			return virtual_address;
		}
		memcpy((void *)vaRes, (const void*)va, get_block_size(virtual_address));
		free_block(virtual_address);
		return vaRes;
	}

	va = ROUNDDOWN(va, PAGE_SIZE);
	struct pageInfo *oldProgramm = &pagesInfo[getPagesInfoIndex(va)];
	if(oldProgramm->size == 0)
	{
		panic("There is no old process to reallocate");
	}
	if(new_size == oldProgramm->size)
	{
		return virtual_address;
	}
	else if(new_size > oldProgramm->size)
	{
		// cprintf("Here 1\n");
		// alloc more frames
		uint32 sizeNeeded = new_size - oldProgramm->size;
		uint32 sizeNeededWillNotBeChanged = sizeNeeded;
		
		struct pageInfo *before = NULL, *after = NULL;
		// cprintf("va before = %p\n", va);
		// cprintf("kheapPageAllocStart = %p\n", kheapPageAllocStart);

		if(va != kheapPageAllocStart)
		{
			// cprintf("ENTERED\n"); 
			before = &pagesInfo[getPagesInfoIndex(oldProgramm->prevPageStartAddress)];
		}
		if(va + oldProgramm->size != kheapPageAllocBreak)
			after = &pagesInfo[getPagesInfoIndex(oldProgramm->size + va)];


		uint8 beforeCondition = 0;
		if(before != NULL)
		{
			if(!before->isBlocked)
			{
				if(before->size > sizeNeeded)
				{
					//split node and take upper space for current node
					sizeNeeded = 0;
					beforeCondition = 1;
				}
				else
				{
					
					cprintf("before address = %p\n", oldProgramm->prevPageStartAddress);
					cprintf("before index = %d\n", getPagesInfoIndex(oldProgramm->prevPageStartAddress));
					cprintf("va = %p\n", va);

					cprintf("before size = %d\n", before->size);
					cprintf("current size = %d\n", oldProgramm->size);
					sizeNeeded -= before->size;
					// merge the before node to current one
					beforeCondition = 2;
				}
			}
		}
		
		uint8 afterCondition = 0;
		uint32 sizeToAllocAfter = 0;
		if(after != NULL && sizeNeeded != 0)
		{
			if(!after->isBlocked)
			{
				sizeToAllocAfter = sizeNeeded;
				if(after->size > sizeNeeded)
				{
					// split the after node and take the bottom space
					afterCondition = 1;
					sizeNeeded = 0;
				}
				else if(after->size == sizeNeeded){
					// merge with node
					afterCondition = 2;
					sizeNeeded = 0;
				}
			}
		}
		// cprintf("Here 4\n");
		
		if(after == NULL)
		{
			if(KERNEL_HEAP_MAX - kheapPageAllocBreak >= sizeNeeded)
			{
				// cprintf("Here 111\n");

				kheapPageAllocBreak += sizeNeeded;
				sizeToAllocAfter = sizeNeeded;
				sizeNeeded = 0;
				after = &pagesInfo[getPagesInfoIndex(kheapPageAllocBreak)];
				after->size = sizeNeeded;
				afterCondition = 2;
			}
		}
		
		// cprintf("Here 5\n");

		if(sizeNeeded == 0)
		{
			// cprintf("Here 6\n");

			bool a;
			uint32 startAddress = va;
			if(beforeCondition == 1 || beforeCondition == 2)
			{
				// cprintf("Here 22\n");
				if(beforeCondition == 1)
				{
				// cprintf("Here 33\n");

					struct pageInfo* pageOnSplit = &pagesInfo[(getPagesInfoIndex(va - sizeNeededWillNotBeChanged))];
					startAddress = va - sizeNeededWillNotBeChanged;
					before->size -= sizeNeededWillNotBeChanged;
					pageOnSplit->prevPageStartAddress = oldProgramm->prevPageStartAddress;
					pageOnSplit->size = new_size;
					if(after != NULL)
					{
						after -> prevPageStartAddress = startAddress;
					}
			// cprintf("Here 7\n");

				}
				else
				{
				// cprintf("Here 44\n");

					startAddress = oldProgramm -> prevPageStartAddress;
					before->size += oldProgramm->size;
					before->isBlocked = 1;
					if(after != NULL)
					{
						after -> prevPageStartAddress = oldProgramm->prevPageStartAddress;
					}

				}

				// cprintf("Memory copied\n");
				a = allocFrames(startAddress, va);
				memcpy((void *)startAddress, (const void*)va, oldProgramm->size);

				oldProgramm->isBlocked = 0;
				oldProgramm->size = 0;
				oldProgramm->prevPageStartAddress = 0;
				oldProgramm = before;
				if(!a)
				{
					return NULL;
				}
			}

			if(afterCondition == 1 || afterCondition == 2)
			{
				// cprintf("Here 9\n");

				struct pageInfo* newAfter;
				cprintf("old Programm size before merge = %d\n", oldProgramm->size);
				oldProgramm->size += sizeToAllocAfter;
				cprintf("old Programm size after merge = %d\n", oldProgramm->size);


				if(afterCondition == 1)
				{
					newAfter = &pagesInfo[getPagesInfoIndex(startAddress + oldProgramm->size)];
					newAfter->size = after->size - sizeToAllocAfter;
					newAfter->prevPageStartAddress = startAddress;
				}
				else
				{
					if(startAddress + oldProgramm->size != kheapPageAllocBreak)
					{
						newAfter = &pagesInfo[getPagesInfoIndex(startAddress + oldProgramm->size)];
						newAfter->prevPageStartAddress = startAddress;
					}
				}
				a = allocFrames(startAddress + oldProgramm->size - sizeToAllocAfter, startAddress + oldProgramm->size);
				if(!a)
				{
					// unmap frames alloced before oldPrgramm
					if(beforeCondition == 1 || beforeCondition == 2)
					{
						for(uint32 currentAddress = startAddress; currentAddress < va; currentAddress += PAGE_SIZE)
						{
							unmap_frame(ptr_page_directory, currentAddress);
						}
					}
					return NULL;
				}
				after->size = 0;
				after->prevPageStartAddress = 0;
			}

			return (void *)startAddress;
		}else{
			void* vaResult = kmalloc(new_size);
			memcpy(vaResult, (const void*)va, oldProgramm->size);

			if(vaResult == NULL)
				return NULL;
			kfree(virtual_address);
			return vaResult;
		}
	}
	else{
		// free Space From The programm
		uint32 sizeToDelete = oldProgramm->size - new_size;
		// cprintf("sizeToDelete = %d\n", sizeToDelete);
		struct pageInfo *splitPoint;
		splitPoint = &pagesInfo[getPagesInfoIndex(va + new_size)];
	
		struct pageInfo *after = &pagesInfo[getPagesInfoIndex(va + oldProgramm->size)];
		if(after->isBlocked){
			splitPoint->size = sizeToDelete;
			splitPoint->prevPageStartAddress = va;
			after->prevPageStartAddress = va + new_size;
		}
		else
		{
			if(va + oldProgramm->size == kheapPageAllocBreak)
			{
				cprintf("break before decreament = %p\n", kheapPageAllocBreak);
				kheapPageAllocBreak -= sizeToDelete;
				cprintf("break after decreament = %p\n", kheapPageAllocBreak);

			}
			else
			{
				splitPoint->size = sizeToDelete + after->size;
				struct pageInfo *afterAfter = &pagesInfo[getPagesInfoIndex(va + oldProgramm->size + after->size)];
				afterAfter ->prevPageStartAddress = va + new_size;
				after->size = 0;
				after->prevPageStartAddress = 0;
			}
		}
	
		for(uint32 currentAddress = va + new_size; currentAddress < va + oldProgramm->size; currentAddress += PAGE_SIZE){
			unmap_frame(ptr_page_directory, currentAddress);
		}
	
		oldProgramm->size -= sizeToDelete;
		return (void *)va;
	}

	if (!lock_already_held)
	{
		release_kspinlock(&MemFrameLists.mfllock);
	}

		// cprintf("Here 8\n");
	return NULL;
}

bool allocFrames(uint32 start, uint32 end){
	for(uint32 currentAddress = start; currentAddress < end; currentAddress += PAGE_SIZE)
	{

		int ret = alloc_page(ptr_page_directory, currentAddress, PERM_WRITEABLE, 1);
		if(ret != 0)
		{
			for(uint32 currentAddress2 = start; currentAddress2 < currentAddress; currentAddress2 += PAGE_SIZE)
			{
				unmap_frame(ptr_page_directory, currentAddress2);
			}
			return 0;
		}
	}
	return 1;
}

uint32 getPagesInfoIndex(uint32 address){
	return (address - kheapPageAllocStart) / PAGE_SIZE;
}
