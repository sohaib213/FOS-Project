#include <inc/lib.h>

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE USER HEAP:
//==============================================
int __firstTimeFlag = 1;
void uheap_init()
{
	if(__firstTimeFlag)
	{
		initialize_dynamic_allocator(USER_HEAP_START, USER_HEAP_START + DYN_ALLOC_MAX_SIZE);
		uheapPlaceStrategy = sys_get_uheap_strategy();
		uheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		uheapPageAllocBreak = uheapPageAllocStart;

		__firstTimeFlag = 0;
	}
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = __sys_allocate_page(ROUNDDOWN(va, PAGE_SIZE), PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
	if (ret < 0)
		panic("get_page() in user: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	int ret = __sys_unmap_frame(ROUNDDOWN((uint32)va, PAGE_SIZE));
	if (ret < 0)
		panic("return_page() in user: failed to return a page to the kernel");
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

struct pageUserHeapInfo pagesInfo[UHP_PAGES_AREA_NUMBER];

//=================================
// [1] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size)
{
	// cprintf("size = %d\n", size);
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'25.IM#2] USER HEAP - #1 malloc
	//Your code is here
	//Comment the following line
	// panic("malloc() is not implemented yet...!!");

	if(ROUNDUP(size, PAGE_SIZE) > (USER_HEAP_MAX - uheapPageAllocStart))
	{
		return NULL;
	}
	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE)
	{
		// cprintf("size = %d\n", size);
		uint32* a = alloc_block(size);
		// cprintf("return add = %p\n\n", (uint32)a);


		// cprintf("va from alloc block = %p\n", (uint32)a);
		if(a == NULL)
			return NULL;
		else if ((uint32) a == ROUNDDOWN((uint32)a, PAGE_SIZE))
		{
			sys_allocate_user_mem((uint32) a, size);
		}
		return (void *)a;
	}else{
		size = ROUNDUP(size, PAGE_SIZE);
		uint32 maxSize = 0, maxSizeAddress;
		uint32 resultAddress = 0, lastAddress = 0;

		
		for(uint32 currentAddress = uheapPageAllocStart; currentAddress < uheapPageAllocBreak;)
		{
			struct pageUserHeapInfo *currentPageInfo = &pagesInfo[getPagesInfoIndex(currentAddress)];
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
				struct pageUserHeapInfo *maxSizePage = &pagesInfo[getPagesInfoIndex(maxSizeAddress)], *nextPage, *splitAddress;
				splitAddress = &pagesInfo[getPagesInfoIndex(maxSizeAddress + size)];
				nextPage = &pagesInfo[getPagesInfoIndex(maxSizeAddress + maxSizePage->size)];
	
				splitAddress->size = maxSizePage->size - size;
				splitAddress->prevPageStartAddress = maxSizeAddress;
	
				nextPage->prevPageStartAddress = maxSizeAddress + size;
	
				maxSizePage->isBlocked = 1;
				maxSizePage->size = size;
			}
			else{
				if(USER_HEAP_MAX - uheapPageAllocBreak < size)
				{
					return NULL;
				}
	
				resultAddress = uheapPageAllocBreak;
				struct pageUserHeapInfo *page = &pagesInfo[getPagesInfoIndex(resultAddress)];
				page->isBlocked = 1;
				page->size = size;
				page->prevPageStartAddress = lastAddress;
	
				uheapPageAllocBreak += size;
			}
		}
		// cprintf("resultAddress = %p\n", resultAddress);
		sys_allocate_user_mem(resultAddress, size);
		return (void*)resultAddress;
	}

	return NULL;
}

//=================================
// [2] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{
	//TODO: [PROJECT'25.IM#2] USER HEAP - #3 free
	//Your code is here
	//Comment the following line
	// panic("free() is not implemented yet...!!");
	if(virtual_address == NULL)
	{
		panic("kfree() error: NULL pointer passed!");
	}


	uint32 va = (uint32)virtual_address;

	if(va >= USER_HEAP_START && va < dynAllocEnd)
	{
			// This is a small block allocation
			free_block(virtual_address);
			return;
	}

	// ========================================
	// Case 2: Large Block (Page Allocator)
	// ========================================
  if(va >= uheapPageAllocStart && va < USER_HEAP_MAX)
	{

		uint32 block_start = ROUNDDOWN(va, PAGE_SIZE);

		struct pageUserHeapInfo *pageToDelete = &pagesInfo[getPagesInfoIndex(block_start)];

		// If size is 0, this block was not allocated
		if(pageToDelete->size == 0 || !pageToDelete->isBlocked)
		{
				panic("kfree() error: trying to free unallocated block!");
		}

		// // Calculate the number of pages to free
		// uint32 block_end = block_start + pageToDelete->size;

		// // Free all frames for this block
		// for(uint32 addr = block_start; addr < block_end; addr += PAGE_SIZE)
		// {
		// 	unmap_frame(ptr_page_directory, addr);
		// }
		// cprintf("block start = %p,   size = %d\n", (uint32) block_start, ROUNDUP(pageToDelete->size, PAGE_SIZE));
		sys_free_user_mem((uint32) block_start, ROUNDUP(pageToDelete->size, PAGE_SIZE));


		bool foundBeforePageEmpty = 0;
		uint32 sizeDeleted = pageToDelete->size;
		struct pageUserHeapInfo *pageBeforeDeleted;
		if(block_start != uheapPageAllocStart) // check if there a free block before it
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

		struct pageUserHeapInfo *pageAfterDeleted;
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
				struct pageUserHeapInfo *pageAfterAfter;
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
			uheapPageAllocBreak = block_start;
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
}

//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
	//Your code is here
	//Comment the following line
	//panic("smalloc() is not implemented yet...!!");


	size = ROUNDUP(size, PAGE_SIZE);
	uint32 maxSize = 0, maxSizeAddress;
	uint32 resultAddress = 0, lastAddress = 0;
	
	for(uint32 currentAddress = uheapPageAllocStart; currentAddress < uheapPageAllocBreak;)
	{
		struct pageUserHeapInfo *currentPageInfo = &pagesInfo[getPagesInfoIndex(currentAddress)];
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
			struct pageUserHeapInfo *maxSizePage = &pagesInfo[getPagesInfoIndex(maxSizeAddress)], *nextPage, *splitAddress;
			splitAddress = &pagesInfo[getPagesInfoIndex(maxSizeAddress + size)];
			nextPage = &pagesInfo[getPagesInfoIndex(maxSizeAddress + maxSizePage->size)];

			splitAddress->size = maxSizePage->size - size;
			splitAddress->prevPageStartAddress = maxSizeAddress;

			nextPage->prevPageStartAddress = maxSizeAddress + size;

			maxSizePage->isBlocked = 1;
			maxSizePage->size = size;
		}
		else{
			if(USER_HEAP_MAX - uheapPageAllocBreak < size)
			{
				return NULL;
			}

			resultAddress = uheapPageAllocBreak;
			struct pageUserHeapInfo *page = &pagesInfo[getPagesInfoIndex(resultAddress)];
			page->isBlocked = 1;
			page->size = size;
			page->prevPageStartAddress = lastAddress;

			uheapPageAllocBreak += size;
		}
	}

	int id = sys_create_shared_object(sharedVarName, size, isWritable, (void*)resultAddress);
	//panic("calleddddddddddddddddddddddddddddddddd");
	
	if(id != E_NO_SHARE && id != E_SHARED_MEM_EXISTS){
		return (void *)resultAddress;
	}

	return NULL;
}

//========================================
// [4] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #4 sget
	//Your code is here
	//Comment the following line
	//panic("sget() is not implemented yet...!!");
	//panic("dah b2a 48aiiiiiiiiiiiiiiiiiiiii");
	int size = sys_size_of_shared_object(ownerEnvID,sharedVarName);
	if(size == E_SHARED_MEM_NOT_EXISTS || size == 0){
		return NULL;
	}

	size = ROUNDUP(size, PAGE_SIZE);
	uint32 maxSize = 0, maxSizeAddress;
	uint32 resultAddress = 0, lastAddress = 0;
	
	for(uint32 currentAddress = uheapPageAllocStart; currentAddress < uheapPageAllocBreak;)
	{
		struct pageUserHeapInfo *currentPageInfo = &pagesInfo[getPagesInfoIndex(currentAddress)];
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
			struct pageUserHeapInfo *maxSizePage = &pagesInfo[getPagesInfoIndex(maxSizeAddress)], *nextPage, *splitAddress;
			splitAddress = &pagesInfo[getPagesInfoIndex(maxSizeAddress + size)];
			nextPage = &pagesInfo[getPagesInfoIndex(maxSizeAddress + maxSizePage->size)];

			splitAddress->size = maxSizePage->size - size;
			splitAddress->prevPageStartAddress = maxSizeAddress;

			nextPage->prevPageStartAddress = maxSizeAddress + size;

			maxSizePage->isBlocked = 1;
			maxSizePage->size = size;
		}
		else{
			if(USER_HEAP_MAX - uheapPageAllocBreak < size)
			{
				return NULL;
			}

			resultAddress = uheapPageAllocBreak;
			struct pageUserHeapInfo *page = &pagesInfo[getPagesInfoIndex(resultAddress)];
			page->isBlocked = 1;
			page->size = size;
			page->prevPageStartAddress = lastAddress;

			uheapPageAllocBreak += size;
		}
	}
	//panic("callingggggggggggggggg");
	int id = sys_get_shared_object(ownerEnvID,sharedVarName,(void*)resultAddress);
	if(id != E_SHARED_MEM_NOT_EXISTS){
		return (void *)resultAddress;
	}

	return NULL;
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	panic("realloc() is not implemented yet...!!");
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
uint32 getPagesInfoIndex(uint32 address){
	return (address - uheapPageAllocStart) / PAGE_SIZE;
}