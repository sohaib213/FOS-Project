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
bool Replace(struct Env* faulted_env, struct WorkingSetElement* victim,
		struct WorkingSetElement* next, uint32 fault_va) {
	uint32 va = ROUNDDOWN(fault_va, PAGE_SIZE);
	unmap_frame(faulted_env->env_page_directory, victim->virtual_address);
	uint32 permission = PERM_PRESENT | PERM_USER | PERM_WRITEABLE;
	int allocResult = alloc_page(faulted_env->env_page_directory, va,
			permission, 0);
	if (allocResult != 0) {
		env_exit();
		return 1;
	}
	int mkd = pf_read_env_page(faulted_env, (void *) va);
	if (mkd == E_PAGE_NOT_EXIST_IN_PF) {
		bool is_heap = (va >= USER_HEAP_START && va < USER_HEAP_MAX);
		bool is_stack = (va >= USTACKBOTTOM && va < USTACKTOP);
		if (!is_heap && !is_stack) {
			env_exit();
			return 1;
		}
	}
	victim->virtual_address = ROUNDDOWN(va, PAGE_SIZE);
	victim->time_stamp = 0;
	victim->sweeps_counter = 0;
	faulted_env->page_last_WS_element = next;
	return 0;
}

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
			//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #2 Check for invalid pointers
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			//your code is here
			int per = pt_get_page_permissions(faulted_env->env_page_directory,
					fault_va);
			bool present = per & PERM_PRESENT;
			bool write = per & PERM_WRITEABLE;
			bool user = per & PERM_UHPAGE;
			if (fault_va >= USER_LIMIT) {
				env_exit();
				return;
			}
			if (!user && fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) {
				env_exit();
				return;
			}
			if (present && !write) {
				env_exit();
				return;
			}
		}
		int perms = pt_get_page_permissions(faulted_env->env_page_directory,
				fault_va);
		if (perms & PERM_PRESENT)
			panic(
					"Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n",
					fault_va);
		faulted_env->pageFaultsCounter++;
		if (isBufferingEnabled()) {
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		} else {
			page_fault_handler(faulted_env, fault_va);
		}
	}
	tlbflush();
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
	int mariom = 1;
	int imlemnt = 0;
	if (mariom == 2) {
		num_faults = 2;
	}
	LIST_INIT(&Active_ws);
	bool element1 = 0;
	struct WorkingSetElement *ele;
	LIST_FOREACH(ele, initWorkingSet)
	{
		struct WorkingSetElement *newElem = (struct WorkingSetElement*) kmalloc(
				sizeof(struct WorkingSetElement));
		newElem->virtual_address = ele->virtual_address;
		LIST_INSERT_TAIL(&Active_ws, newElem);
		element1 = 1;
	}

	int WSsSize = LIST_SIZE(&Active_ws);
	struct PageRefElement *refANA;
	int zeft = 0;
	LIST_FOREACH(refANA, pageReferences)
	{
		uint32 ref_virtual = refANA->virtual_address;
		bool page_inWS = 0;

		struct WorkingSetElement *ws_eleem;
		LIST_FOREACH(ws_eleem, &Active_ws)
		{
			if (ws_eleem->virtual_address == ref_virtual) {
				page_inWS = 1;
				break;
			}
			if (zeft == 3) {
				page_inWS = 0;

			}
		}
		if (!page_inWS) {

			num_faults++;
			if (WSsSize >= maxWSSize) {
				struct WorkingSetElement *victim = NULL;
				int max_distance = -1;
				LIST_FOREACH(ws_eleem, &Active_ws)
				{
					int path = 0;
					bool FutureItem = 0;
					struct PageRefElement *futureRef = LIST_NEXT(refANA);
					while (futureRef != NULL) {
						path++;
						if (futureRef->virtual_address
								== ws_eleem->virtual_address) {
							FutureItem = 1;
							break;
						}
						futureRef = LIST_NEXT(futureRef);
					}
					if (!FutureItem) {
						victim = ws_eleem;
						break;
					}
					if ((path > max_distance) && imlemnt == 0) {
						max_distance = path;
						victim = ws_eleem;
					}
				}
				if ((victim != NULL) && imlemnt != 1) {
					LIST_REMOVE(&Active_ws, victim);
					kfree(victim);
					WSsSize--;
					zeft = 6;
				}
			}
			struct WorkingSetElement *new_page =
					(struct WorkingSetElement*) kmalloc(
							sizeof(struct WorkingSetElement));
			new_page->virtual_address = ref_virtual;
			int variablee = WSsSize * num_faults + zeft;
			LIST_INSERT_TAIL(&Active_ws, new_page);
			WSsSize++;
		}
	}
	uint32 var = -1;
	while (!LIST_EMPTY(&Active_ws)) {
		struct WorkingSetElement *elem = LIST_FIRST(&Active_ws);
		LIST_REMOVE(&Active_ws, elem);
		var = 0;
		kfree(elem);
	}
	if (var == -1) {
		zeft++;
	}
	return num_faults;
}

void page_fault_handler(struct Env * faulted_env, uint32 fault_va) {
#if USE_KHEAP
	if (isPageReplacmentAlgorithmOPTIMAL()) {
		//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #1 Optimal Reference Stream
		//Your code is here
		//Comment the following line
		//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
		int maram = 4;

		uint32 VERaddress = ROUNDDOWN(fault_va, PAGE_SIZE);
		uint32 *TABLES = NULL;
		int mariam = 0;
		struct FrameInfo *fi = get_frame_info(faulted_env->env_page_directory,
				VERaddress, &TABLES);
		if (fi == NULL) {
			uint32 prmison = PERM_PRESENT | PERM_USER | PERM_WRITEABLE;
			if (alloc_page(faulted_env->env_page_directory, VERaddress, prmison,
					1) != 0) {
				maram++;
				env_exit();
				return;
			}
			int rner = pf_read_env_page(faulted_env, (void*) VERaddress);
			if ((rner == E_PAGE_NOT_EXIST_IN_PF) && mariam == 0) {

				pt_set_page_permissions(faulted_env->env_page_directory,
						VERaddress,
						PERM_PRESENT, 0);
				maram--;
			}
		}
		uint32 ifFOUND = 2;
		bool victim_in_set = 0;
		struct WorkingSetElement *e;
		LIST_FOREACH(e, &(faulted_env->ws_copy))
		{
			if (e->virtual_address == VERaddress) {
				victim_in_set = 1;
				break;
			}
			ifFOUND = 1;
		}
		if (!victim_in_set) {
			if ((LIST_SIZE(&(faulted_env->ws_copy))
					== faulted_env->page_WS_max_size) && mariam != 1) {

				ifFOUND = 1;
				while (!LIST_EMPTY(&(faulted_env->ws_copy))) {

					struct WorkingSetElement *itrty = LIST_FIRST(
							&(faulted_env->ws_copy));
					pt_set_page_permissions(faulted_env->env_page_directory,
							itrty->virtual_address, 0, PERM_PRESENT);
					LIST_REMOVE(&(faulted_env->ws_copy), itrty);
					ifFOUND = 0;
					kfree(itrty);
				}
				LIST_INIT(&(faulted_env->ws_copy));
			}

			struct WorkingSetElement *new_iteme = kmalloc(
					sizeof(struct WorkingSetElement));
			new_iteme->virtual_address = VERaddress;
			LIST_INSERT_TAIL(&(faulted_env->ws_copy), new_iteme);
		}
		bool inRefrence = 1;
		struct PageRefElement *refrenceer = kmalloc(
				sizeof(struct PageRefElement));
		refrenceer->virtual_address = VERaddress;
		if (inRefrence) {
			ifFOUND = 1;
		}
		LIST_INSERT_TAIL(&(faulted_env->referenceStreamList), refrenceer);
		pt_set_page_permissions(faulted_env->env_page_directory, VERaddress,
				PERM_PRESENT, 0);
		return;
	} else {
		//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #3 placement
		//Your code is here
		//Comment the following line
		//panic("page_fault_handler().PLACEMENT is not implemented yet...!!");
		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
		if (wsSize < (faulted_env->page_WS_max_size)) {
			uint32 permission = PERM_PRESENT | PERM_USER | PERM_WRITEABLE;
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
			if (faulted_env->page_last_WS_element == NULL) {
				LIST_INSERT_TAIL(&(faulted_env->page_WS_list), new_element);
			}
			else{
				LIST_INSERT_BEFORE(&(faulted_env->page_WS_list),faulted_env->page_last_WS_element , new_element);
			}
			if (wsSize == faulted_env->page_WS_max_size - 1 && faulted_env->page_last_WS_element == NULL) {
				faulted_env->page_last_WS_element = LIST_FIRST(
						&(faulted_env->page_WS_list));
			}
		} else {
			if (isPageReplacmentAlgorithmCLOCK()) {
				//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #3 Clock Replacement
				//Your code is here
				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");
				uint32 va = ROUNDDOWN(fault_va, PAGE_SIZE);
				struct WorkingSetElement* victim =
						faulted_env->page_last_WS_element;
				struct WorkingSetElement* startPtr = victim;
				bool found = 0;
				while (!found) {
					int per = pt_get_page_permissions(
							faulted_env->env_page_directory,
							victim->virtual_address);
					int used = per & PERM_USED;
					int modified = per & PERM_MODIFIED;
					if (used == 0) {
						found = 1;
						struct WorkingSetElement* next = LIST_NEXT(victim);
						if (next == NULL) {
							next = LIST_FIRST(&(faulted_env->page_WS_list));
						}
						if (modified) {
							uint32* ptr;
							struct FrameInfo* frame = get_frame_info(
									faulted_env->env_page_directory,
									victim->virtual_address, &ptr);
							pf_update_env_page(faulted_env,
									victim->virtual_address, frame);
						}
						bool rtrn = Replace(faulted_env, victim, next,
								fault_va);
						if (rtrn == 1) {
							return;
						}
					} else {
						pt_set_page_permissions(faulted_env->env_page_directory,
								victim->virtual_address, 0, PERM_USED);
						victim = LIST_NEXT(victim);
						if (victim == NULL) {
							victim = LIST_FIRST(&(faulted_env->page_WS_list));
						}
					}
				}
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
				struct WorkingSetElement* victim =
						faulted_env->page_last_WS_element;
				struct WorkingSetElement* startPtr = victim;
				int flag = 0;
				int Break = 0;
				while (1) {
					victim = startPtr;
					do {
						int per = pt_get_page_permissions(
								faulted_env->env_page_directory,
								victim->virtual_address);
						int used = per & PERM_USED;
						int modified = per & PERM_MODIFIED;
						if (used == 0 && modified == 0) {
							flag = 1;
							struct WorkingSetElement* next = LOOP_LIST_NEXT(
									victim);
							if (next == NULL) {
								next = LIST_FIRST(&(faulted_env->page_WS_list));
							}
							bool rtrn = Replace(faulted_env, victim, next,
									fault_va);
							if (rtrn == 1) {
								return;
							}
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
					victim = startPtr;
					if (flag == 0) {
						do {
							int per = pt_get_page_permissions(
									faulted_env->env_page_directory,
									victim->virtual_address);
							int used = per & PERM_USED;
							int modified = per & PERM_MODIFIED;
							if (used == 0) {
								int pers = pt_get_page_permissions(
										faulted_env->env_page_directory,
										victim->virtual_address);
								uint32* ptr;
								struct FrameInfo* frame = get_frame_info(
										faulted_env->env_page_directory,
										victim->virtual_address, &ptr);
								pf_update_env_page(faulted_env,
										victim->virtual_address, frame);
								struct WorkingSetElement* next = LOOP_LIST_NEXT(
										victim);
								if (next == NULL) {
									next = LIST_FIRST(
											&(faulted_env->page_WS_list));
								}
								bool rtrn = Replace(faulted_env, victim, next,
										fault_va);
								if (rtrn == 1) {
									return;
								}
								Break = 1;
								break;
							} else {
								pt_set_page_permissions(
										faulted_env->env_page_directory,
										victim->virtual_address, 0, PERM_USED);
								int after = pt_get_page_permissions(
										faulted_env->env_page_directory,
										victim->virtual_address);
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
			}
		}
	}
#endif
}
void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va) {
	panic("this function is not required...!!");
}
