#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_kspinlock(&AllShares.shareslock, "shares lock");
	//init_sleeplock(&AllShares.sharessleeplock, "shares sleep lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//=========================
// [2] Find Share Object:
//=========================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* find_share(int32 ownerID, char* name)
{
#if USE_KHEAP
	struct Share * ret = NULL;
	bool wasHeld = holding_kspinlock(&(AllShares.shareslock));
	if (!wasHeld)
	{
		acquire_kspinlock(&(AllShares.shareslock));
	}
	{
		struct Share * shr ;
		LIST_FOREACH(shr, &(AllShares.shares_list))
		{
			//cprintf("shared var name = %s compared with %s\n", name, shr->name);
			if(shr->ownerID == ownerID && strcmp(name, shr->name)==0)
			{
				//cprintf("%s found\n", name);
				ret = shr;
				break;
			}
		}
	}
	if (!wasHeld)
	{
		release_kspinlock(&(AllShares.shareslock));
	}
	return ret;
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [3] Get Size of Share Object:
//==============================
int size_of_shared_object(int32 ownerID, char* shareName)
{
	// This function should return the size of the given shared object
	// RETURN:
	//	a) If found, return size of shared object
	//	b) Else, return E_SHARED_MEM_NOT_EXISTS
	//
	struct Share* ptr_share = find_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;

	return 0;
}
//===========================================================


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=====================================
// [1] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
struct Share* alloc_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #1 alloc_share
	//Your code is here
	//Comment the following line
	//panic("alloc_share() is not implemented yet...!!");

	struct Share *shared_obj_check = find_share(ownerID,shareName);
	if(shared_obj_check == NULL)
		return NULL; //object already allocated


	struct Share *shared_obj =(struct Share *)kmalloc(sizeof(struct Share));

	if(shared_obj == NULL)
		return NULL;
	
	unsigned int masked_va = (unsigned int)shared_obj;	
	shared_obj->ID = (int32)(masked_va & 0x7FFFFFFF);
	shared_obj->ownerID = ownerID;
	shared_obj->size = size;
	shared_obj->references = 1;
	shared_obj->isWritable = isWritable;

	int i = 0;
	for(i = 0; i < 63 && shareName[i] != '\0'; ++i){
		shared_obj->name[i] = shareName[i]; 
	}
	shared_obj->name[i] = '\0';

	unsigned int pages =(unsigned int) (ROUNDUP(size,PAGE_SIZE) / PAGE_SIZE);

	shared_obj->framesStorage = (struct FrameInfo**)kmalloc(pages*sizeof(struct FrameInfo*));
	if(shared_obj->framesStorage == NULL){
		kfree(shared_obj);
		return NULL;
	}
	for(int j = 0; j < pages; ++j){
		shared_obj->framesStorage[j] = NULL;
	}
	
	return shared_obj;
}


//=========================
// [4] Create Share Object:
//=========================
int create_shared_object(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #3 create_shared_object
	//Your code is here
	//Comment the following line
	//panic("create_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment
	uint32 	*page_direc = myenv->env_page_directory;
	// This function should create the shared object at the given virtual address with the given size
	// and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_EXISTS if the shared object already exists
	//	c) E_NO_SHARE if failed to create a shared object

	bool lock_already_held = holding_kspinlock(&AllShares.shareslock);
	if (!lock_already_held)
	{
		acquire_kspinlock(&AllShares.shareslock);
	}

	struct Share *shared_obj_check = find_share(ownerID,shareName);
	if(shared_obj_check != NULL){
		if(!lock_already_held)
			release_kspinlock(&AllShares.shareslock);
		return E_SHARED_MEM_EXISTS;
	}


	struct Share *shared_obj = alloc_share(ownerID,shareName,size,isWritable);
	
	if(shared_obj == NULL){
		if(!lock_already_held)
			release_kspinlock(&AllShares.shareslock);
		return E_NO_SHARE;	
	}

	LIST_INSERT_HEAD(&AllShares.shares_list,shared_obj);

	unsigned int pages =(unsigned int) (ROUNDUP(size,PAGE_SIZE) / PAGE_SIZE);
	uint32 va = (uint32) virtual_address;

	for(int i=0; i < pages; i++){
		struct FrameInfo *frame = NULL;
		int ret = allocate_frame(&frame);

		if(ret != 0 || frame == NULL){
			if(!lock_already_held)
				release_kspinlock(&AllShares.shareslock);
			return E_NO_SHARE;	
		}

		shared_obj->framesStorage[i] = frame;
		int ret2 = map_frame(page_direc,frame,va,isWritable);
		
		if(ret2 != 0){
			free_frame(frame);

			for(int j=0; j<i; ++j){
				unmap_frame(page_direc,va);
				va += PAGE_SIZE;
			}
			LIST_REMOVE(&AllShares.shares_list,shared_obj);
			kfree(shared_obj->framesStorage);
			kfree(shared_obj);

			if(!lock_already_held)
				release_kspinlock(&AllShares.shareslock);
			return E_NO_SHARE;
		}
		va += PAGE_SIZE;
	}

	if(!lock_already_held)
			release_kspinlock(&AllShares.shareslock);

	return shared_obj->ID;
}


//======================
// [5] Get Share Object:
//======================
int get_shared_object(int32 ownerID, char* shareName, void* virtual_address)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #5 get_shared_object
	//Your code is here
	//Comment the following line
	//panic("get_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment
	uint32 	*page_direc = myenv->env_page_directory;
	// 	This function should share the required object in the heap of the current environment
	//	starting from the given virtual_address with the specified permissions of the object: read_only/writable
	// 	and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

	bool lock_already_held = holding_kspinlock(&AllShares.shareslock);
	if (!lock_already_held)
	{
		acquire_kspinlock(&AllShares.shareslock);
	}

	struct Share *shared_obj = find_share(ownerID,shareName);
	if(shared_obj == NULL){
		if(!lock_already_held)
			release_kspinlock(&AllShares.shareslock);
		return E_SHARED_MEM_NOT_EXISTS;
	}

	uint32 va = (uint32) virtual_address;
	unsigned int pages =(unsigned int) (ROUNDUP(shared_obj->size,PAGE_SIZE) / PAGE_SIZE);
	for(uint32 i=0; i<pages; ++i){
		int ret = map_frame(page_direc,shared_obj->framesStorage[i],va,shared_obj->isWritable);
		va += PAGE_SIZE;
	}
	shared_obj->references++;

	if(!lock_already_held)
		release_kspinlock(&AllShares.shareslock);
	return shared_obj->ID;
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//
//=========================
// [1] Delete Share Object:
//=========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself
void free_share(struct Share* ptrShare)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - free_share
	//Your code is here
	//Comment the following line
	panic("free_share() is not implemented yet...!!");
}


//=========================
// [2] Free Share Object:
//=========================
int delete_shared_object(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - delete_shared_object
	//Your code is here
	//Comment the following line
	panic("delete_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment

	// This function should free (delete) the shared object from the User Heapof the current environment
	// If this is the last shared env, then the "frames_store" should be cleared and the shared object should be deleted
	// RETURN:
	//	a) 0 if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

	// Steps:
	//	1) Get the shared object from the "shares" array (use get_share_object_ID())
	//	2) Unmap it from the current environment "myenv"
	//	3) If one or more table becomes empty, remove it
	//	4) Update references
	//	5) If this is the last share, delete the share object (use free_share())
	//	6) Flush the cache "tlbflush()"

}
