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
		// cprintf("CASE 1\n");
		// GET THE BLOCK AND SAVE IT THEN REMOVE IT FROM THE FREE BLOCK LIST
		needed_block=LIST_FIRST(&freeBlockLists[index_of_nearst_size]); //SAVE THE LAST ELEMENT IN PTR
		LIST_REMOVE(&freeBlockLists[index_of_nearst_size],needed_block);

		// FIRST WAY USING THE INDEX (UPDATE pageBlockInfoArr)
		//uint32 pageInfoArr_index=get_pageInfoArr_index((void*)needed_block);
		//pageBlockInfoArr[pageInfoArr_index].num_of_free_blocks=pageBlockInfoArr[pageInfoArr_index].num_of_free_blocks-1;

		// cprintf("66\n");
		//SECOND WAY USING THE PTR  (UPDATE pageBlockInfoArr)
		struct PageInfoElement* pageInfoElement=get_pageInfoArr_element_ptr((void*)needed_block);
		pageInfoElement->num_of_free_blocks--;

		// cprintf("77\n");

		return (void*)needed_block;

	}

	// CASE 2
	else if (numberOfFreeBlocks==0){
		cprintf("CASE 2\n");
		// TAKE FREE PAGE FROM (FREE PAGE LIST)
		struct PageInfoElement* new_free_page=LIST_FIRST(&freePagesList);
		LIST_REMOVE(&freePagesList,new_free_page);
		// GET THE VA OF THE TAKEN FREE PAGE IN THE VM
		// cprintf("11\n");
		uint32 VA=to_page_va(new_free_page);
		void* ptr=(void*)VA;
		// cprintf("22\n");
		int ret=get_page(ptr); // RESERVE PAGE IN TEH PHYSICAL MEM
		// cprintf("33\n");
		// CREATE BLOCKS IN THE FREE BLOCK LIST WHEN (GET PAGE SUCCESS , RET==0)
		// cprintf("VA = %p\n", VA);

		if(ret==0){
			for(int i = 0; i < PAGE_SIZE / nearst_size; i++){
				// cprintf("i = %d\n", i * nearst_size);
				struct BlockElement* created_block = (struct BlockElement*)((uint32*)((uint8*)VA + (i * nearst_size)));// ADD THE OFFSET THAT REPRESTNT THE BLOCK SIZE OR NEARST_SIZE ON THE PAGE
				LIST_INSERT_TAIL(&freeBlockLists[index_of_nearst_size],created_block);
			}
		}else {
			panic("CAN'T GET_PAGE OR ALLOC_PAGE IN ALLOC_BLOCK FUNCTION");
		}
		// for(int i = 0; i < 8; i++)
		// {
		// 	cprintf("size: %d number = %d\n", (1 << (i + 3)), freeBlockLists[index_of_nearst_size].size);
		// }
		// cprintf("GET BLOCKS SUCCESSFULLY\n");
		// UPDATE THE CORRESPONDING PageBlockInfoArr ELEMENT
		struct PageInfoElement* pageInfoElement=get_pageInfoArr_element_ptr((void*)VA);
		pageInfoElement->block_size=nearst_size;
		pageInfoElement->num_of_free_blocks=(PAGE_SIZE/nearst_size)-1; // -1 for taken block

		// GET THE BLOCK AND RETURN IT
		needed_block=LIST_FIRST(&freeBlockLists[index_of_nearst_size]);
		LIST_REMOVE(&freeBlockLists[index_of_nearst_size],needed_block);

		return (void*)needed_block;

	}
	// CASE 3
	else if(numberOfFreePages==0){
		cprintf("CASE 3\n");
		if(nearst_size<=DYN_ALLOC_MAX_BLOCK_SIZE/2){
			needed_block=alloc_block(nearst_size*2);
		}
		else {
			panic("no free blocks and no free pages ");
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
	//Comment the following line
	panic("free_block() Not implemented yet");
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
