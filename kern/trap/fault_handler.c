/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>
#include <kern/mem/kheap.h>
#include <inc/queue.h>

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE) {
	assert(
			LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE;
}
void setPageReplacmentAlgorithmCLOCK() {
	_PageRepAlgoType = PG_REP_CLOCK;
}
void setPageReplacmentAlgorithmFIFO() {
	_PageRepAlgoType = PG_REP_FIFO;
}
void setPageReplacmentAlgorithmModifiedCLOCK() {
	_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;
}
/*2018*/void setPageReplacmentAlgorithmDynamicLocal() {
	_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;
}
/*2021*/void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps) {
	_PageRepAlgoType = PG_REP_NchanceCLOCK;
	page_WS_max_sweeps = PageWSMaxSweeps;
}
/*2024*/void setFASTNchanceCLOCK(bool fast) {
	FASTNchanceCLOCK = fast;
}
;
/*2025*/void setPageReplacmentAlgorithmOPTIMAL() {
	_PageRepAlgoType = PG_REP_OPTIMAL;
}
;

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE) {
	return _PageRepAlgoType == LRU_TYPE ? 1 : 0;
}
uint32 isPageReplacmentAlgorithmCLOCK() {
	if (_PageRepAlgoType == PG_REP_CLOCK)
		return 1;
	return 0;
}
uint32 isPageReplacmentAlgorithmFIFO() {
	if (_PageRepAlgoType == PG_REP_FIFO)
		return 1;
	return 0;
}
uint32 isPageReplacmentAlgorithmModifiedCLOCK() {
	if (_PageRepAlgoType == PG_REP_MODIFIEDCLOCK)
		return 1;
	return 0;
}
/*2018*/uint32 isPageReplacmentAlgorithmDynamicLocal() {
	if (_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL)
		return 1;
	return 0;
}
/*2021*/uint32 isPageReplacmentAlgorithmNchanceCLOCK() {
	if (_PageRepAlgoType == PG_REP_NchanceCLOCK)
		return 1;
	return 0;
}
/*2021*/uint32 isPageReplacmentAlgorithmOPTIMAL() {
	if (_PageRepAlgoType == PG_REP_OPTIMAL)
		return 1;
	return 0;
}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt) {
	_EnableModifiedBuffer = enableIt;
}
uint8 isModifiedBufferEnabled() {
	return _EnableModifiedBuffer;
}

void enableBuffering(uint32 enableIt) {
	_EnableBuffering = enableIt;
}
uint8 isBufferingEnabled() {
	return _EnableBuffering;
}

void setModifiedBufferLength(uint32 length) {
	_ModifiedBufferLength = length;
}
uint32 getModifiedBufferLength() {
	return _ModifiedBufferLength;
}

//===============================
// FAULT HANDLERS
//===============================

//==================
// [0] INIT HANDLER:
//==================
void fault_handler_init() {
	//setPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX);
	//setPageReplacmentAlgorithmOPTIMAL();
	setPageReplacmentAlgorithmCLOCK();
	//setPageReplacmentAlgorithmModifiedCLOCK();
	enableBuffering(0);
	enableModifiedBuffer(0);
	setModifiedBufferLength(1000);
}
//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault = 0;
extern uint32 sys_calculate_free_frames();

struct Env* last_faulted_env = NULL;
void fault_handler(struct Trapframe *tf) {
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//cprintf("************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env) {
		num_repeated_fault++;
		if (num_repeated_fault == 3) {
			print_trapframe(tf);
			panic(
					"Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n",
					before_last_fault_va, before_last_eip, fault_va);
		}
	} else {
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32) tf->tf_eip;
	last_fault_va = fault_va;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap) {
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env
				&& fault_va
						>= (uint32) cur_env->kstack&& fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va
				>= (uint32) c->stack&& fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!",
					c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else {
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL) {
		cprintf("\nFaulted VA = %x\n", fault_va);
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ((faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT)
			!= PERM_PRESENT) {
		faulted_env->tableFaultsCounter++;
		table_fault_handler(faulted_env, fault_va);
	} else {
		if (userTrap) {
			/*============================================================================================*/
			//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #2 Check for invalid pointers
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			//your code is here
			int per = pt_get_page_permissions(faulted_env->env_page_directory,
					fault_va);

			bool present = per & PERM_PRESENT;
			bool write = per & PERM_WRITEABLE;
			bool user = per & PERM_UHPAGE;

			// cprintf("Debug: Entered userTrap handler for va=%p, perms=%p, err=%p\n", fault_va, per, tf->tf_err);

			// Check if fault address in kernel space
			if (fault_va >= USER_LIMIT) {
				// cprintf("Debug: Invalid access! fault_va in kernel space (va=%p)\n", fault_va);
				env_exit();
				return;
			}
			// Check if inside user heap but unmarked

			if (!user && fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) {
				// cprintf("Invalid pointer: unmapped page at %x\n", fault_va);
				env_exit();
				return;
			}

			// Check if it's a write fault but page is not writable
			if (present && !write) {
				// cprintf("Invalid pointer: write to read-only page at %x\n", fault_va);
				env_exit();
				return;
			}
			// cprintf("Debug: Passed all invalid pointer checks for va=%x\n", fault_va);
			/*============================================================================================*/
		}

		/* 2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory,
				fault_va);
		// cprintf("Debug: Checking access rights violation for va=%x, perms=%x\n", fault_va, perms);

		if (perms & PERM_PRESENT)
			panic(
					"Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n",
					fault_va);
		/*============================================================================================*/

		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter++;
		// cprintf("Debug: Normal page fault occurred. Total faults = %d\n", faulted_env->pageFaultsCounter);

//				cprintf("[%08s] user PAGE fault va %08x\n", faulted_env->prog_name, fault_va);
//				cprintf("\nPage working set BEFORE fault handler...\n");
//				env_page_ws_print(faulted_env);
		//int ffb = sys_calculate_free_frames();

		if (isBufferingEnabled()) {
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		} else {
			page_fault_handler(faulted_env, fault_va);
		}

		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(faulted_env);
		//		int ffa = sys_calculate_free_frames();
		//		cprintf("fault handling @%x: difference in free frames (after - before = %d)\n", fault_va, ffa - ffb);
	}

	/*************************************************************/
	//Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}

//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env * curenv, uint32 fault_va) {
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory,
				(uint32) fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}

//=========================
// [3] PAGE FAULT HANDLER:
//=========================
/* Calculate the number of page faults according th the OPTIMAL replacement strategy
 * Given:
 * 	1. Initial Working Set List (that the process started with)
 * 	2. Max Working Set Size
 * 	3. Page References List (contains the stream of referenced VAs till the process finished)
 *
 * 	IMPORTANT: This function SHOULD NOT change any of the given lists
 */
int get_optimal_num_faults(struct WS_List *initWorkingSet, int maxWSSize,
		struct PageRef_List *pageReferences) {
	//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #2 get_optimal_num_faults
	//Your code is here
	//Comment the following line
	//panic("get_optimal_num_faults() is not implemented yet...!!");

	    int num_faults = 0;

	    struct WS_List Active_ws;
	    LIST_INIT(&Active_ws);

	    struct WorkingSetElement *ele;
	    LIST_FOREACH(ele, initWorkingSet) {
	        struct WorkingSetElement *newElem =
	            (struct WorkingSetElement*)kmalloc(sizeof(struct WorkingSetElement));
	        newElem->virtual_address = ele->virtual_address;
	        LIST_INSERT_TAIL(&Active_ws, newElem);
	    }


	    int WSsSize = LIST_SIZE(&Active_ws);


	    struct PageRefElement *ref;
	    LIST_FOREACH(ref, pageReferences) {
	        uint32 refVA = ref->virtual_address;
	        bool page_inWS = 0;


	        struct WorkingSetElement *ws_eleem;
	        LIST_FOREACH(ws_eleem, &Active_ws) {
	            if (ws_eleem->virtual_address == refVA) {
	            	page_inWS = 1;
	                break;
	            }
	        }


	        if (!page_inWS) {
	            num_faults++;

	            if (WSsSize >= maxWSSize) {

	                struct WorkingSetElement *victim = NULL;
	                int max_distance = -1;

	                // For each page in current WS, find when it will be used next
	                LIST_FOREACH(ws_eleem, &Active_ws) {
	                    int distance = 0;
	                    bool future = 0;


	                    struct PageRefElement *futureRef = LIST_NEXT(ref);
	                    while (futureRef != NULL) {
	                        distance++;
	                        if (futureRef->virtual_address == ws_eleem->virtual_address) {
	                        	future = 1;
	                            break;
	                        }
	                        futureRef = LIST_NEXT(futureRef);
	                    }


	                    if (!future) {
	                        victim = ws_eleem;
	                        break;
	                    }

	                    if (distance > max_distance) {
	                    	max_distance = distance;
	                        victim = ws_eleem;
	                    }
	                }


	                if (victim != NULL) {
	                    LIST_REMOVE(&Active_ws, victim);
	                    kfree(victim);
	                    WSsSize--;
	                }
	            }

	            struct WorkingSetElement *new_page =
	                (struct WorkingSetElement*)kmalloc(sizeof(struct WorkingSetElement));
	            new_page->virtual_address = refVA;
	            LIST_INSERT_TAIL(&Active_ws, new_page);
	            WSsSize++;
	        }
	    }

	    // Clean
	    while (!LIST_EMPTY(&Active_ws)) {
	        struct WorkingSetElement *elem = LIST_FIRST(&Active_ws);
	        LIST_REMOVE(&Active_ws, elem);
	        kfree(elem);
	    }

	    return num_faults;
}

void page_fault_handler(struct Env * faulted_env, uint32 fault_va) {
#if USE_KHEAP
	if (isPageReplacmentAlgorithmOPTIMAL()) {
		//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #1 Optimal Reference Stream
		//Your code is here
		//Comment the following line
		// 	panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
		uint32 va = ROUNDDOWN(fault_va, PAGE_SIZE);
		uint32 *pt = NULL;
		cprintf("\n fault at va %p\n",va);
		cprintf("\n=== OPTIMAL FAULT #%d: VA = %p ===\n",
		LIST_SIZE(&(faulted_env->referenceStreamList)) + 1, va);



		//  if page is in memory
		struct FrameInfo *fi = get_frame_info(faulted_env->env_page_directory, va, &pt);
		if (fi == NULL) {
				cprintf(">>> Page NOT in RAM - allocating\n");
				// Page not in RAM - allocate it
				uint32 perms = PERM_PRESENT | PERM_USER | PERM_WRITEABLE;
				if (alloc_page(faulted_env->env_page_directory, va, perms, 1) != 0) {
						env_exit();
						return;
				}

			// Read from disk
			int ret = pf_read_env_page(faulted_env, (void*)va);
			cprintf(">>> pf_read_env_page returned: %d\n", ret);
			if (ret == E_PAGE_NOT_EXIST_IN_PF) {
					pt_set_page_permissions(faulted_env->env_page_directory, va, PERM_PRESENT, 0);
			}
		}
		//  page is already in Active WS
		cprintf(">>> Checking if 0x%x is in Active WS (size=%d)...\n",
						va, LIST_SIZE(&(faulted_env->ws_copy)));

		bool isInWorkingSet = 0;
		struct WorkingSetElement *e;
		LIST_FOREACH(e, &(faulted_env->ws_copy)) {
				if (e->virtual_address == va) {
						cprintf(">>> Page 0x%x FOUND in Active WS - returning\n", va);
						cprintf("=== END FAULT ===\n\n");
						isInWorkingSet = 1;
						break;
				}
		}
		if(!isInWorkingSet)
		{
			cprintf(">>> Page 0x%x NOT in Active WS\n", va);
	
			//  If WS is FULL
			if (LIST_SIZE(&(faulted_env->ws_copy)) == faulted_env->page_WS_max_size) {
					cprintf(">>> WS is FULL (size=%d, max=%d) - clearing all pages\n",
									LIST_SIZE(&(faulted_env->ws_copy)), faulted_env->page_WS_max_size);
				//add to reference
				struct WorkingSetElement *cur;
				LIST_FOREACH(cur, &(faulted_env->ws_copy)) {
						pt_set_page_permissions(faulted_env->env_page_directory,
																		cur->virtual_address, 0, PERM_PRESENT);
						// uint32* ptr_page_table ;
						// int ret = get_page_table(faulted_env->env_page_directory, cur->virtual_address, &ptr_page_table);
						// cprintf("PTE after modified of va = %p is %p \n", cur->virtual_address, ptr_page_table[PTX(cur->virtual_address)]);
				}
				LIST_INIT(&(faulted_env->ws_copy));
			}
			struct WorkingSetElement *new_elem = kmalloc(sizeof(struct WorkingSetElement));
			new_elem->virtual_address = va;
			LIST_INSERT_TAIL(&(faulted_env->ws_copy), new_elem);
			cprintf(">>> Added 0x%x to Active WS, new size = %d\n",
							va, LIST_SIZE(&(faulted_env->ws_copy)));
		}


		struct PageRefElement *ref = kmalloc(sizeof(struct PageRefElement));
		ref->virtual_address = va;
		LIST_INSERT_TAIL(&(faulted_env->referenceStreamList), ref);
		cprintf(">>> Added to ref stream, total refs = %d\n",
						LIST_SIZE(&(faulted_env->referenceStreamList)));

		pt_set_page_permissions(faulted_env->env_page_directory, va, PERM_PRESENT, 0);

		cprintf("PTE BEFORE LEAVE = %p\n", pt_get_page_permissions(faulted_env->env_page_directory, va));
		cprintf("=== END FAULT ===\n\n");
		return;
	}
		else
	{
//		cprintf("WORKING SET BEFORE placement:\n");
//		env_page_ws_print(faulted_env);
		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
		if (wsSize < (faulted_env->page_WS_max_size)) {
			//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #3 placement
			//Your code is here
			//Comment the following line
			// panic("page_fault_handler().PLACEMENT is not implemented yet...!!");
			uint32 permission = PERM_PRESENT | PERM_USER | PERM_WRITEABLE;

			// cprintf("Debug: Allocating frame for fault_va=%x\n", fault_va);
			// int allocResult = allocate_frame(&poin_frameinfo);

			// if (allocResult != 0 || poin_frameinfo == NULL)
			// {
			// 	cprintf("Debug: Failed to allocate frame! allocResult=%d\n", allocResult);
			// 	env_exit();
			// 	return;
			// }

			// cprintf("Debug: Mapping frame to VA=%p with perms=%x\n", fault_va, permission);
			// map_frame(faulted_env->env_page_directory, poin_frameinfo, fault_va, permission);
			int allocResult = alloc_page(faulted_env->env_page_directory,
					fault_va, permission, 0);
			if (allocResult != 0) {
				// cprintf("Debug: Failed to allocate frame! allocResult=%d\n", allocResult);
				env_exit();
				return;
			}
			// cprintf("Debug: Reading page from Page File for VA=%p\n", fault_va);
			int mkd = pf_read_env_page(faulted_env, (void *) fault_va);
			// cprintf("Debug: pf_read_env_page() returned %d\n", mkd);

			if (mkd == E_PAGE_NOT_EXIST_IN_PF) {
				bool is_heap = (fault_va >= USER_HEAP_START
						&& fault_va < USER_HEAP_MAX);
				bool is_stack = (fault_va >= USTACKBOTTOM
						&& fault_va < USTACKTOP);

				// cprintf("Debug: Page not found in Page File. is_heap=%d, is_stack=%d\n", is_heap, is_stack);

				if (!is_heap && !is_stack) {
					// cprintf("Debug: Fault VA not heap/stack -> Exiting env.\n");
					env_exit();
					return;
				}
			}

			if(faulted_env->page_last_WS_element != NULL)
			{
				struct WorkingSetElement *currentEl = LIST_FIRST(
						&(faulted_env->page_WS_list));
				while (currentEl != faulted_env->page_last_WS_element) {
					struct WorkingSetElement *tmp = LIST_NEXT(currentEl);
					LIST_REMOVE(&(faulted_env->page_WS_list), currentEl);
					LIST_INSERT_TAIL(&(faulted_env->page_WS_list), currentEl);
					currentEl = tmp;
				}
			}

			// cprintf("Debug: Creating WS element for VA=%x\n", fault_va);
			struct WorkingSetElement *new_element =
					env_page_ws_list_create_element(faulted_env, fault_va);

			if (new_element == NULL) {
				// cprintf("Debug: Failed to create WS element for VA=%x\n", fault_va);
				env_exit();
				return;
			}

			// cprintf("Debug: Inserting new WS element at tail. WS size before insert=%d\n", wsSize);
			LIST_INSERT_TAIL(&(faulted_env->page_WS_list), new_element);
			// faulted_env->page_last_WS_element = new_element;
			// cprintf("last element in ws = %p\n", faulted_env->page_last_WS_element);
			if (wsSize == faulted_env->page_WS_max_size - 1) {
				//cprintf("change last element\n");
				faulted_env->page_last_WS_element = LIST_FIRST(
						&(faulted_env->page_WS_list));
			}
			//cprintf("WORKING SET AFTER placement:\n");

			//env_page_ws_print(faulted_env);
		} else {
			if (isPageReplacmentAlgorithmCLOCK()) {
				//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #3 Clock Replacement
				//Your code is here
				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");

				struct WorkingSetElement* victim =
						faulted_env->page_last_WS_element;
				struct WorkingSetElement* startPtr = victim;

				//Normal Clock Replacement Algorithm
				do {
					int per = pt_get_page_permissions(
							faulted_env->env_page_directory,
							victim->virtual_address);
					int used = per & PERM_USED;

					//DEBUG iteration
					cprintf(" check va=%x used=%d\n", victim->virtual_address,
							used ? 1 : 0);

					if (used == 0) {

						//DEBUG hit
						cprintf(" HIT va=%x\n", victim->virtual_address);

						//Next element assignment
						struct WorkingSetElement* next = LOOP_LIST_NEXT(victim);
						if (next == NULL) {
							next = LIST_FIRST(&(faulted_env->page_WS_list));
						}

						// Print full WS before replacement
						cprintf("WS BEFORE REPLACE (lastWS va=%x): ",
								faulted_env->page_last_WS_element ?
										faulted_env->page_last_WS_element->virtual_address :
										0);

						struct WorkingSetElement* cur = LIST_FIRST(
								&(faulted_env->page_WS_list));
						while (cur != NULL) {
							cprintf("[%x] ", cur->virtual_address);
							cur = LIST_NEXT(cur);
						}
						uint32* ptr;
						struct FrameInfo* frame = get_frame_info(
						faulted_env->env_page_directory,
						victim->virtual_address, &ptr);
						pf_update_env_page(faulted_env, victim->virtual_address,
							frame);
						
						cprintf("\n");

						// Print victim info
						cprintf("REPLACE: victim elem=%p old_va=%x new_va=%x\n",
								victim, victim->virtual_address, fault_va);

						//Replacement
						uint32 permission = PERM_PRESENT | PERM_USER
								| PERM_WRITEABLE;
						unmap_frame(faulted_env->env_page_directory,
								victim->virtual_address);
						int allocResult = alloc_page(
								faulted_env->env_page_directory, fault_va,
								permission, 0);
						if (allocResult != 0) {
							env_exit();
							return;
						}
						int mkd = pf_read_env_page(faulted_env,
								(void *) fault_va);
						if (mkd == E_PAGE_NOT_EXIST_IN_PF) {
							bool is_heap = (fault_va >= USER_HEAP_START
									&& fault_va < USER_HEAP_MAX);
							bool is_stack = (fault_va >= USTACKBOTTOM
									&& fault_va < USTACKTOP);

							if (!is_heap && !is_stack) {
								env_exit();
								return;
							}
						}

						//Victim replacement and pagelastWSelement assignment
						victim->virtual_address = ROUNDDOWN(fault_va,
								PAGE_SIZE);
						victim->time_stamp = 0;
						victim->sweeps_counter = 0;
						faulted_env->page_last_WS_element = next;

						// DEBUG update
						cprintf("SET lastWS=%x\n", next->virtual_address);

						// Print WS after replacement
						cprintf("WS AFTER REPLACE (lastWS va=%x): ",
								faulted_env->page_last_WS_element->virtual_address);

						cur = LIST_FIRST(&(faulted_env->page_WS_list));
						while (cur != NULL) {
							cprintf("[%x] ", cur->virtual_address);
							cur = LIST_NEXT(cur);
						}
						cprintf("\n");

						break;
					} else {
						// DEBUG: show PTE before clear
						int before = pt_get_page_permissions(
								faulted_env->env_page_directory,
								victim->virtual_address);
						cprintf(
								" ABOUT TO CLEAR USED va=%x before_perm=%x used=%d\n",
								victim->virtual_address, before,
								(before & PERM_USED) ? 1 : 0);

						// Attempt to clear USED
						pt_set_page_permissions(faulted_env->env_page_directory,
								victim->virtual_address, 0, PERM_USED);

						// Read back immediately
						int after = pt_get_page_permissions(
								faulted_env->env_page_directory,
								victim->virtual_address);
						cprintf(" AFTER CLEAR va=%x after_perm=%x used=%d\n",
								victim->virtual_address, after,
								(after & PERM_USED) ? 1 : 0);

						// Extra: if not cleared, print some extra state for debugging
						if (after & PERM_USED) {
							cprintf(
									" CLEAR FAILED for va=%x — check pt_set_page_permissions implementation and env pd.\n",
									victim->virtual_address);
							//print env pd pointer
							cprintf(" env_page_directory=%x\n",
									faulted_env->env_page_directory);
						}

						// advance victim as before
						if (LOOP_LIST_NEXT(victim) == NULL) {
							victim = LIST_FIRST(&(faulted_env->page_WS_list));
						} else {
							victim = LIST_NEXT(victim);
						}
					}
				} while (victim != startPtr);

			} else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX)) {
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #2 LRU Aging Replacement
				//Your code is here
				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
				struct WorkingSetElement* currentWSelement = LIST_FIRST(
						&(faulted_env->page_WS_list));
				struct WorkingSetElement* victim = currentWSelement;

				if (currentWSelement != NULL) {
					unsigned int currentWS = currentWSelement->time_stamp;
					unsigned int minWS = currentWSelement->time_stamp;

					currentWSelement = LIST_NEXT(currentWSelement);
					while (currentWSelement != NULL) {
						currentWS = currentWSelement->time_stamp;
						if (currentWS < minWS) {
							minWS = currentWS;
							victim = currentWSelement;
						}
						currentWSelement = LIST_NEXT(currentWSelement);
					}
				}

				//Writing to the disc before replacement
				int pers = pt_get_page_permissions(
						faulted_env->env_page_directory,
						victim->virtual_address);
				if (pers & PERM_MODIFIED) {
					uint32* ptr;
					struct FrameInfo* frame = get_frame_info(
							faulted_env->env_page_directory,
							victim->virtual_address, &ptr);
					pf_update_env_page(faulted_env, victim->virtual_address,
							frame);
				}

				//Replacement
				uint32 permission = PERM_PRESENT | PERM_USER | PERM_WRITEABLE;
				unmap_frame(faulted_env->env_page_directory,
						victim->virtual_address);
				LIST_REMOVE(&(faulted_env->page_WS_list), victim);
				int allocResult = alloc_page(faulted_env->env_page_directory,
						fault_va, permission, 0);
				if (allocResult != 0) {
					env_exit();
					return;
				}
				int mkd = pf_read_env_page(faulted_env, (void *) fault_va);
				if (mkd == E_PAGE_NOT_EXIST_IN_PF) {
					bool is_heap = (fault_va >= USER_HEAP_START
							&& fault_va < USER_HEAP_MAX);
					bool is_stack = (fault_va >= USTACKBOTTOM
							&& fault_va < USTACKTOP);

					if (!is_heap && !is_stack) {
						env_exit();
						return;
					}
				}
				struct WorkingSetElement *new_element =
						env_page_ws_list_create_element(faulted_env, fault_va);
				if (new_element == NULL) {
					env_exit();
					return;
				}
				LIST_INSERT_TAIL(&(faulted_env->page_WS_list), new_element);
			} else if (isPageReplacmentAlgorithmModifiedCLOCK()) {
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #3 Modified Clock Replacement
				//Your code is here
				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");

				// DEBUG START
				//cprintf("MODCLOCK: START lastWS=%x\n",faulted_env->page_last_WS_element->virtual_address);
				// END DEBUG

				struct WorkingSetElement* victim =
						faulted_env->page_last_WS_element;
				struct WorkingSetElement* startPtr = victim;
				int flag = 0;
				int Break = 0;

				// DEBUG
				//cprintf("MODCLOCK: startPtr=%x victim=%x\n",startPtr->virtual_address, victim->virtual_address);

				while (1) {
					victim = startPtr;
					do {
						int per = pt_get_page_permissions(
								faulted_env->env_page_directory,
								victim->virtual_address);
						int used = per & PERM_USED;
						int modified = per & PERM_MODIFIED;

						// DEBUG iteration
						//cprintf("TRY1: check va=%x used=%d mod=%d\n",victim->virtual_address, used ? 1 : 0,modified ? 1 : 0);

						if (used == 0 && modified == 0) {
							flag = 1;

							// DEBUG hit
							//cprintf("TRY1: HIT va=%x\n",victim->virtual_address);

							//Next element assignment
							struct WorkingSetElement* next = LOOP_LIST_NEXT(
									victim);
							if (next == NULL) {
								next = LIST_FIRST(&(faulted_env->page_WS_list));
							}

							// Print full WS before replacement
							//cprintf("WS BEFORE REPLACE (lastWS va=%x): ",faulted_env->page_last_WS_element ? faulted_env->page_last_WS_element->virtual_address : 0);

//										struct WorkingSetElement* cur = LIST_FIRST(&(faulted_env->page_WS_list));
//										while (cur != NULL) {
//										    //cprintf("[%x] ", cur->virtual_address);
//										    cur = LIST_NEXT(cur);
//										}
							//cprintf("\n");

							// Print victim info
							//cprintf("REPLACE: victim elem=%p old_va=%x new_va=%x\n",victim, victim->virtual_address, fault_va);

							//Replacement
							uint32 permission = PERM_PRESENT | PERM_USER
									| PERM_WRITEABLE;
							unmap_frame(faulted_env->env_page_directory,
									victim->virtual_address);
							int allocResult = alloc_page(
									faulted_env->env_page_directory, fault_va,
									permission, 0);
							if (allocResult != 0) {
								env_exit();
								return;
							}
							int mkd = pf_read_env_page(faulted_env,
									(void *) fault_va);
							if (mkd == E_PAGE_NOT_EXIST_IN_PF) {
								bool is_heap = (fault_va >= USER_HEAP_START
										&& fault_va < USER_HEAP_MAX);
								bool is_stack = (fault_va >= USTACKBOTTOM
										&& fault_va < USTACKTOP);

								if (!is_heap && !is_stack) {
									env_exit();
									return;
								}
							}

							//Victim replacement and pagelastWSelement assignment
							victim->virtual_address = ROUNDDOWN(fault_va,
									PAGE_SIZE);
							victim->time_stamp = 0;
							victim->sweeps_counter = 0;
							faulted_env->page_last_WS_element = next;

							// DEBUG update
							//cprintf("SET lastWS=%x\n", next->virtual_address);

							// Print WS after replacement
//										cprintf("WS AFTER REPLACE (lastWS va=%x): ",
//										        faulted_env->page_last_WS_element->virtual_address);
//
//										cur = LIST_FIRST(&(faulted_env->page_WS_list));
//										while (cur != NULL) {
//										    cprintf("[%x] ", cur->virtual_address);
//										    cur = LIST_NEXT(cur);
//										}
//										cprintf("\n");

							Break = 1;
							break;
						} else {
							if (LOOP_LIST_NEXT(victim) == NULL) {
								victim = LIST_FIRST(
										&(faulted_env->page_WS_list));
							} else {
								victim = LIST_NEXT(victim);
							}
						}
					} while (victim != startPtr);

					if (Break == 1) {
						break;
					}

					//Restart victim pointer
					victim = startPtr;
					if (flag == 0) {

						// DEBUG
						//cprintf("TRY1: NO HIT, go to TRY2\n");

						//Normal Clock Replacement Algorithm
						do {
							int per = pt_get_page_permissions(
									faulted_env->env_page_directory,
									victim->virtual_address);
							int used = per & PERM_USED;
							int modified = per & PERM_MODIFIED;

							// DEBUG iteration
							//cprintf("TRY2: check va=%x used=%d mod=%d\n",victim->virtual_address, used ? 1 : 0,modified ? 1 : 0);

							if (used == 0) {

								// DEBUG hit
								//cprintf("TRY2: HIT va=%x\n",victim->virtual_address);

								//Next element assignment
								struct WorkingSetElement* next = LOOP_LIST_NEXT(
										victim);
								if (next == NULL) {
									next = LIST_FIRST(
											&(faulted_env->page_WS_list));
								}

								//Writing to the disc before replacement
								int pers = pt_get_page_permissions(
										faulted_env->env_page_directory,
										victim->virtual_address);
								uint32* ptr;
								struct FrameInfo* frame = get_frame_info(
										faulted_env->env_page_directory,
										victim->virtual_address, &ptr);
								pf_update_env_page(faulted_env,
										victim->virtual_address, frame);

								// Print full WS before replacement
								//cprintf("WS BEFORE REPLACE (lastWS va=%x): ",faulted_env->page_last_WS_element ? faulted_env->page_last_WS_element->virtual_address : 0);

//											struct WorkingSetElement* cur = LIST_FIRST(&(faulted_env->page_WS_list));
//											while (cur != NULL) {
//											    cprintf("[%x] ", cur->virtual_address);
//											    cur = LIST_NEXT(cur);
//											}
//											cprintf("\n");

								// Print victim info
//											cprintf("REPLACE: victim elem=%p old_va=%x new_va=%x\n",victim, victim->virtual_address, fault_va);

								//Replacement
								uint32 permission = PERM_PRESENT | PERM_USER
										| PERM_WRITEABLE;
								unmap_frame(faulted_env->env_page_directory,
										victim->virtual_address);
								int allocResult = alloc_page(
										faulted_env->env_page_directory,
										fault_va, permission, 0);
								if (allocResult != 0) {
									env_exit();
									return;
								}
								int mkd = pf_read_env_page(faulted_env,
										(void *) fault_va);
								if (mkd == E_PAGE_NOT_EXIST_IN_PF) {
									bool is_heap = (fault_va >= USER_HEAP_START
											&& fault_va < USER_HEAP_MAX);
									bool is_stack = (fault_va >= USTACKBOTTOM
											&& fault_va < USTACKTOP);

									if (!is_heap && !is_stack) {
										env_exit();
										return;
									}
								}

								//Victim replacement and pagelastWSelement assignment
								victim->virtual_address = ROUNDDOWN(fault_va,
										PAGE_SIZE);
								victim->time_stamp = 0;
								victim->sweeps_counter = 0;
								faulted_env->page_last_WS_element = next;

								// DEBUG update
								//cprintf("SET lastWS=%x\n",next->virtual_address);

								// Print WS after replacement
								//cprintf("WS AFTER REPLACE (lastWS va=%x): ",faulted_env->page_last_WS_element->virtual_address);

//											cur = LIST_FIRST(&(faulted_env->page_WS_list));
//											while (cur != NULL) {
//											    cprintf("[%x] ", cur->virtual_address);
//											    cur = LIST_NEXT(cur);
//											}
//											cprintf("\n");

								Break = 1;
								break;
							} else {
								// DEBUG: show PTE before clear
//								int before = pt_get_page_permissions(
//										faulted_env->env_page_directory,
//										victim->virtual_address);
								//cprintf("TRY2: ABOUT TO CLEAR USED va=%x before_perm=%x used=%d\n",victim->virtual_address, before,(before & PERM_USED) ? 1 : 0);

								// Attempt to clear USED
								pt_set_page_permissions(
										faulted_env->env_page_directory,
										victim->virtual_address, 0, PERM_USED);

								// Read back immediately
								int after = pt_get_page_permissions(
										faulted_env->env_page_directory,
										victim->virtual_address);
								//cprintf("TRY2: AFTER CLEAR va=%x after_perm=%x used=%d\n",victim->virtual_address, after,(after & PERM_USED) ? 1 : 0);

								// Extra: if not cleared, print some extra state for debugging
								//if (after & PERM_USED) {
								//cprintf("TRY2: CLEAR FAILED for va=%x — check pt_set_page_permissions implementation and env pd.\n",victim->virtual_address);
								// print env pd pointer
								//cprintf("TRY2: env_page_directory=%x\n",faulted_env->env_page_directory);
								//}

								// advance victim as before
								if (LOOP_LIST_NEXT(victim) == NULL) {
									victim = LIST_FIRST(
											&(faulted_env->page_WS_list));
								} else {
									victim = LIST_NEXT(victim);
								}
							}
						} while (victim != startPtr);

						if (Break == 1) {
							break;
						}
					}
				}
				// DEBUG END
				//cprintf("MODCLOCK: DONE\n");
			}
		}
	}
#endif
}
void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va) {
	panic("this function is not required...!!");
}
