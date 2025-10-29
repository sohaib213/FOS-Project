/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//==================================
// [1] GET PAGE VA:
//==================================
__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo)
{
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
bool is_initialized = 0;
void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #1 initialize_dynamic_allocator
	//Your code is here
	//Comment the following line
	//panic("initialize_dynamic_allocator() Not implemented yet");
	dynAllocStart = daStart;
	dynAllocEnd = daEnd;
	LIST_INIT(&freePagesList);

	for(int i = 0; i < 9; ++i){
		LIST_INIT(&freeBlockLists[i]);
	}

	for(int i = 0; i < DYN_ALLOC_MAX_SIZE/PAGE_SIZE; ++i){
		struct PageInfoElement* page = &pageBlockInfoArr[i];
		// no need this is by default implemented
		// page->block_size = 0;
		// page->num_of_free_blocks = 0;
		// page->prev_next_info.le_next = NULL;
		// page->prev_next_info.le_prev = NULL;
		LIST_INSERT_TAIL(&freePagesList,page);
	}

}

//===========================
// [2] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #2 get_block_size
	//Your code is here
	//Comment the following line
	//panic("get_block_size() Not implemented yet");

	uint32 v_add = (uint32) va;
	unsigned int index = (ROUNDDOWN(v_add,PAGE_SIZE) - dynAllocStart )/ PAGE_SIZE;
	uint32 b_size = pageBlockInfoArr[index].block_size;

	return b_size;
}

uint32 get_pageInfoArr_index(void* va){
	uint32 v_add = (uint32) va;
	unsigned int index = (ROUNDDOWN(v_add,PAGE_SIZE) - dynAllocStart )/ PAGE_SIZE;
	return index;
}

struct PageInfoElement* get_pageInfoArr_element_ptr(void* va){
	uint32 v_add = (uint32) va;
	unsigned int index = (ROUNDDOWN(v_add,PAGE_SIZE) - dynAllocStart )/ PAGE_SIZE;
	return &pageBlockInfoArr[index];
}

//===========================
// 3) ALLOCATE BLOCK:
//void *alloc_block(uint32 size)
//{
//	//==================================================================================
//	//DON'T CHANGE THESE LINES==========================================================
//	//==================================================================================
//	{
//		assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
//	}
//	//==================================================================================
//	//==================================================================================
//	// RETURN NULL IF SIZE==0
//	if (size == 0) return NULL;
//
//	// --- get nearest power-of-two block size >= size ---
//	uint32 nearst_size = (1 << LOG2_MIN_SIZE);
//	int index_of_nearst_size = 0; // index 0 corresponds to 1<<LOG2_MIN_SIZE
//	while (nearst_size < size && (nearst_size << 1) <= DYN_ALLOC_MAX_BLOCK_SIZE) {
//		nearst_size <<= 1;
//		index_of_nearst_size++;
//	}
//	// now nearst_size is the smallest power-of-two >= size
//	// index_of_nearst_size is the freeBlockLists index to use
//
//	uint32 numberOfFreeBlocks = LIST_SIZE(&freeBlockLists[index_of_nearst_size]);
//	uint32 numberOfFreePages = LIST_SIZE(&freePagesList);
//	struct BlockElement* needed_block = NULL;
//
//	// CASE 1: there is already a free block of this size
//	if (numberOfFreeBlocks > 0) {
//		cprintf("CASE 1\n");
//		needed_block = LIST_FIRST(&freeBlockLists[index_of_nearst_size]);
//		LIST_REMOVE(&freeBlockLists[index_of_nearst_size], needed_block);
//
//		// update page info (using index)
//		uint32 pageInfoArr_index = get_pageInfoArr_index((void*)needed_block);
//		if (pageInfoArr_index < NUM_PAGES) {
//			if (pageBlockInfoArr[pageInfoArr_index].num_of_free_blocks > 0)
//				pageBlockInfoArr[pageInfoArr_index].num_of_free_blocks--;
//			else
//				pageBlockInfoArr[pageInfoArr_index].num_of_free_blocks = 0; // safety
//		}
//		return (void*)needed_block;
//	}
//
//	// CASE 2: no free blocks of this size, but there is a free page to fill
//	else if (numberOfFreeBlocks == 0 && numberOfFreePages > 0) {
//		cprintf("CASE 2\n");
//		// take a free page
//		struct PageInfoElement* new_free_page = LIST_FIRST(&freePagesList);
//		LIST_REMOVE(&freePagesList, new_free_page);
//
//		uint32 VA = to_page_va(new_free_page);
//		void* ptr = (void*)VA;
//		int ret = get_page(ptr); // reserve physical frame
//
//		if (ret != 0) {
//			panic("CAN'T GET_PAGE OR ALLOC_PAGE IN ALLOC_BLOCK FUNCTION");
//		}
//
//		// set page metadata BEFORE inserting blocks
//		struct PageInfoElement* pageInfoElement = get_pageInfoArr_element_ptr((void*)VA);
//		pageInfoElement->block_size = nearst_size;
//		pageInfoElement->num_of_free_blocks = PAGE_SIZE / nearst_size; // initially all blocks free
//
//		// create blocks FOR THIS PAGE
//		// IMPORTANT: insert new page's blocks at HEAD so that next LIST_FIRST returns block from this page
//		uint32 blocks_per_page = PAGE_SIZE / nearst_size;
//		for (uint32 i = 0; i < blocks_per_page; i++) {
//			struct BlockElement* created_block = (struct BlockElement*)((uint8*)VA + (i * nearst_size));
//			LIST_INSERT_HEAD(&freeBlockLists[index_of_nearst_size], created_block);
//		}
//
//		// now allocate the first block from that list (which will be from the page we just populated)
//		needed_block = LIST_FIRST(&freeBlockLists[index_of_nearst_size]);
//		LIST_REMOVE(&freeBlockLists[index_of_nearst_size], needed_block);
//
//		// update pageInfo after actual removal
//		if (pageInfoElement->num_of_free_blocks > 0)
//			pageInfoElement->num_of_free_blocks--;
//		else
//			pageInfoElement->num_of_free_blocks = 0;
//
//		return (void*)needed_block;
//	}
//
//	// CASE 3: no free blocks of this size AND no free pages of any kind
//	else if (numberOfFreePages == 0) {
//		cprintf("CASE 3\n");
//		// try to get from larger block: request a block of double size and split it
//		if (nearst_size <= DYN_ALLOC_MAX_BLOCK_SIZE / 2) {
//			// request a block of next-higher size (recursive)
//			void* bigger = alloc_block(nearst_size * 2);
//			if (bigger == NULL) return NULL;
//
//			// split bigger block into two buddies of size = nearst_size
//			// bigger points to start of the big block
//			struct BlockElement* first_half = (struct BlockElement*)bigger;
//			struct BlockElement* second_half = (struct BlockElement*)((uint8*)bigger + nearst_size);
//
//			// insert both halves into the free list of current size
//			LIST_INSERT_HEAD(&freeBlockLists[index_of_nearst_size], second_half);
//			LIST_INSERT_HEAD(&freeBlockLists[index_of_nearst_size], first_half);
//
//			// update page info for the page that contains 'bigger'
//			uint32 pindex = get_pageInfoArr_index((void*)bigger);
//			if (pindex < NUM_PAGES) {
//				// bigger was taken from a page that previously had block_size = nearst_size*2
//				// after split, that page should have block_size = nearst_size (we are now using smaller blocks)
//				pageBlockInfoArr[pindex].block_size = nearst_size;
//				pageBlockInfoArr[pindex].num_of_free_blocks = (PAGE_SIZE / nearst_size); // both halves included
//			}
//
//			// now allocate from this size (should hit CASE1 now)
//			return alloc_block(size);
//		}
//		// cannot split further
//		return NULL;
//	}
//
//	return (void*)needed_block;
//}

//===========================
void *alloc_block(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #3 alloc_block
	//Your code is here

	// RETURN NULL IF SIZE==0
	if(size==0)return NULL;

	// cprintf("Size Needed = %d\n", size);
	// GET THE NEARST SIZE OF 2
	uint16 nearst_size;
	int index_of_nearst_size = 8;
	for(; index_of_nearst_size >= 0; index_of_nearst_size--)
	{
		nearst_size = 1 << (index_of_nearst_size + LOG2_MIN_SIZE);
		if(nearst_size == size)
			break;
		else if(nearst_size < size)
		{
			nearst_size *= 2;
			index_of_nearst_size++;
			break;
		}
	}
	if(index_of_nearst_size < 0)
		index_of_nearst_size = 0;
	// cprintf("nearst size = %d\n", nearst_size);

	// cprintf("index_of_nearst_size = %d\n", index_of_nearst_size);
	// uint32 index_of_nearst_size=log2(nearst_size)-LOG2_MIN_SIZE; // TO GET THE BOWER OF 2 AND MINUS 3 FROM IT TO GET THE BASE INDEX
	uint32 numberOfFreeBlocks=LIST_SIZE(&freeBlockLists[index_of_nearst_size]);
	uint32 numberOfFreePages=LIST_SIZE(&freePagesList);
	struct BlockElement* needed_block=NULL;

	// CASE 1
	if(numberOfFreeBlocks>0){
		 cprintf("CASE 1\n");
		// GET THE BLOCK AND SAVE IT THEN REMOVE IT FROM THE FREE BLOCK LIST
		needed_block=LIST_FIRST(&freeBlockLists[index_of_nearst_size]); //SAVE THE LAST ELEMENT IN PTR
		LIST_REMOVE(&freeBlockLists[index_of_nearst_size],needed_block);

		// FIRST WAY USING THE INDEX (UPDATE pageBlockInfoArr)
		uint32 pageInfoArr_index=get_pageInfoArr_index((void*)needed_block);
		pageBlockInfoArr[pageInfoArr_index].num_of_free_blocks=pageBlockInfoArr[pageInfoArr_index].num_of_free_blocks-1;

		// cprintf("66\n");
		//SECOND WAY USING THE PTR  (UPDATE pageBlockInfoArr)
//		struct PageInfoElement* pageInfoElement=get_pageInfoArr_element_ptr((void*)needed_block);
//		pageInfoElement->num_of_free_blocks--;

		// cprintf("77\n");

		return (void*)needed_block;

	}

	// CASE 2
	else if (numberOfFreeBlocks==0 && numberOfFreePages>0 ){
		cprintf("CASE 2\n");
		// TAKE FREE PAGE FROM (FREE PAGE LIST)
		struct PageInfoElement* new_free_page=LIST_FIRST(&freePagesList);
		LIST_REMOVE(&freePagesList,new_free_page);
		// GET THE VA OF THE TAKEN FREE PAGE IN THE VM
		uint32 VA=to_page_va(new_free_page);
		void* ptr=(void*)VA;

		int ret=get_page(ptr); // RESERVE PAGE IN TEH PHYSICAL MEM

		if(ret!=0){
			panic("CAN'T GET_PAGE OR ALLOC_PAGE IN ALLOC_BLOCK FUNCTION");
		}

		// INIT THE CORRESPONDING PageBlockInfoArr ELEMENT
		struct PageInfoElement* pageInfoElement=get_pageInfoArr_element_ptr((void*)VA);
		pageInfoElement->block_size=nearst_size;
		pageInfoElement->num_of_free_blocks=(PAGE_SIZE/nearst_size); // -1 for taken block

		// CREATE BLOCKS IN THE FREE BLOCK LIST
			for(int i = 0; i < PAGE_SIZE / nearst_size; i++){
				// cprintf("i = %d\n", i * nearst_size);
				struct BlockElement* created_block = (struct BlockElement*)((uint8*)VA + (i * nearst_size));// ADD THE OFFSET THAT REPRESTNT THE BLOCK SIZE OR NEARST_SIZE ON THE PAGE
				LIST_INSERT_HEAD(&freeBlockLists[index_of_nearst_size],created_block);
			}

		// GET THE BLOCK AND RETURN IT
		needed_block=LIST_FIRST(&freeBlockLists[index_of_nearst_size]);
		LIST_REMOVE(&freeBlockLists[index_of_nearst_size],needed_block);

		// UPDATE THE CORRESPONDING PageBlockInfoArr ELEMENT
		pageInfoElement->num_of_free_blocks--;// -1 for taken block

		return (void*)needed_block;

	}
	// CASE 3
	else if(numberOfFreePages==0){
		cprintf("CASE 3\n");
		if(nearst_size<=DYN_ALLOC_MAX_BLOCK_SIZE/2){
			needed_block=alloc_block(nearst_size*2);
		}
		return (void*)needed_block;
	}
	return (void*)needed_block;

	//Comment the following line
	//panic("alloc_block() Not implemented yet");

	//TODO: [PROJECT'25.BONUS#1] DYNAMIC ALLOCATOR - block if no free block
}

//===========================
// [4] FREE BLOCK:
//===========================
void free_block(void *va)
{

	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert((uint32)va >= dynAllocStart && (uint32)va < dynAllocEnd);
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #4 free_block
	//Your code is here
	uint32 correspond_block_size=get_block_size((void*)va);
	unsigned int index_of_block_size = 0;
	int size_for_test=correspond_block_size;
	while (size_for_test > 1) {
		size_for_test >>= 1;
		index_of_block_size++;
	}
	index_of_block_size-=LOG2_MIN_SIZE;
	// REMVOE THE BLOCK FROM THE FREE BLOCK LIST
	LIST_INSERT_HEAD(&freeBlockLists[index_of_block_size],(struct BlockElement*)va);

	// UPDATE PageBlockInfoArr
	struct PageInfoElement* pageInfoElement=get_pageInfoArr_element_ptr((void*)va);
	pageInfoElement->num_of_free_blocks++;
	uint32 max_free_blocks=(PAGE_SIZE/correspond_block_size);

	// CHECK IF FREE BLOCKS LED TO THE MAX FREE BLOCKS
	if(pageInfoElement->num_of_free_blocks==max_free_blocks){
		// DELETE ALL THE FREE BLOCKS RELATED TO THE FREED PAGE
		struct BlockElement* blockElement;
		LIST_FOREACH(blockElement, &freeBlockLists[index_of_block_size]) {
		   // GET THE CORR PAGE INFO AND COMPARE EVERY BLOCK IF IT SAME THE CORR PAGE INFO WITH US (DELETE IT)
			if(get_pageInfoArr_element_ptr((void*)blockElement)==pageInfoElement){
				LIST_REMOVE(&freeBlockLists[index_of_block_size],blockElement);
			}
		}
		// RETURN THE FRAME INTO THE PHYSICAL MEM AGAIN
		return_page((void*)pageInfoElement);
		// RETURN THE PAGE AGAIN IN THE FREE PAGE LIST
		LIST_INSERT_TAIL(&freePagesList,pageInfoElement);

	}


	//Comment the following line

	//panic("free_block() Not implemented yet");
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	panic("realloc_block() Not implemented yet");
}
