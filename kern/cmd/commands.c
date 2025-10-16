/*
 * commands.c
 *
 *  Created on: Oct 14, 2022
 *      Author: HP
 *      Contains the commands array & the command functions
 */

#include "commands.h"

#include <kern/trap/trap.h>
#include <kern/trap/fault_handler.h>
#include <kern/proc/user_environment.h>
#include <kern/proc/priority_manager.h>
#include <kern/tests/utilities.h>
#include "../cpu/sched.h"
#include "../disk/pagefile_manager.h"
#include "../mem/kheap.h"
#include "../mem/memory_manager.h"
#include "../tests/tst_handler.h"
#include "../tests/utilities.h"
#include "../cons/console.h"

//TODO:LAB2.Hands-on: declare start address variable of "My int array"

//====================================================================

//Array of commands. (initialized)
struct Command commands[] =
{
		//TODO: LAB2 Hands-on: add the commands here================


		//==========================================================

		//*******************************//
		/* COMMANDS WITH ZERO ARGUMENTS */
		//*******************************//
		{ "help", "Display this list of commands", command_help, 0 },
		{ "kernel_info", "Display information about the kernel", command_kernel_info, 0 },
		{ "ikb", "Lab3.Example: shows mapping info of KERNEL_BASE" ,command_kernel_base_info, 0},
		{ "dkb", "Lab3.Example: delete the mapping of KERNEL_BASE" ,command_del_kernel_base, 0},
		{ "meminfo", "display info about RAM", command_meminfo, 0},
		{"sched?", "print current scheduler algorithm", command_print_sch_method, 0},
		{"runall", "run all loaded programs", command_run_all, 0},
		{"printall", "print all loaded programs", command_print_all, 0},
		{"killall", "kill all environments in the system", command_kill_all, 0},
		{"fifo", "set replacement algorithm to FIFO", command_set_page_rep_FIFO, 0},
		{"clock", "set replacement algorithm to CLOCK", command_set_page_rep_CLOCK, 0},
		{"modclock", "set replacement algorithm to modified CLOCK", command_set_page_rep_ModifiedCLOCK, 0},
		{"optimal", "set replacement algorithm to OPTIMAL", command_set_page_rep_OPTIMAL, 0},
		{"rep?", "print current replacement algorithm", command_print_page_rep, 0},
		{"uhfirstfit", "set USER heap placement strategy to FIRST FIT", command_set_uheap_plac_FIRSTFIT, 0},
		{"uhbestfit", "set USER heap placement strategy to BEST FIT", command_set_uheap_plac_BESTFIT, 0},
		{"uhnextfit", "set USER heap placement strategy to NEXT FIT", command_set_uheap_plac_NEXTFIT, 0},
		{"uhworstfit", "set USER heap placement strategy to WORST FIT", command_set_uheap_plac_WORSTFIT, 0},
		{"uhcustomfit", "set USER heap placement strategy to CUSTOM FIT", command_set_uheap_plac_CUSTOMFIT, 0},
		{"uheap?", "print current USER heap placement strategy", command_print_uheap_plac, 0},
		{"khcontalloc", "set KERNEL heap placement strategy to CONTINUOUS ALLOCATION", command_set_kheap_plac_CONTALLOC, 0},
		{"khfirstfit", "set KERNEL heap placement strategy to FIRST FIT", command_set_kheap_plac_FIRSTFIT, 0},
		{"khbestfit", "set KERNEL heap placement strategy to BEST FIT", command_set_kheap_plac_BESTFIT, 0},
		{"khnextfit", "set KERNEL heap placement strategy to NEXT FIT", command_set_kheap_plac_NEXTFIT, 0},
		{"khworstfit", "set KERNEL heap placement strategy to WORST FIT", command_set_kheap_plac_WORSTFIT, 0},
		{"khcustomfit", "set KERNEL heap placement strategy to CUSTOM FIT", command_set_kheap_plac_CUSTOMFIT, 0},
		{"kheap?", "print current KERNEL heap placement strategy", command_print_kheap_plac, 0},
		{"nobuff", "disable buffering", command_disable_buffering, 0},
		{"buff", "enable buffering", command_enable_buffering, 0},
		{"nomodbuff", "disable modified buffer", command_disable_modified_buffer, 0},
		{"modbuff", "enable modified buffer", command_enable_modified_buffer, 0},
		{"modbufflength?", "get modified buffer length", command_get_modified_buffer_length, 0},
		{"cls", "clear screen", command_cls, 0},

		//*****************************//
		/* COMMANDS WITH ONE ARGUMENT */
		//*****************************//
		{ "rm", "Lab2.Example: reads one byte from specific physical location" ,command_readmem_k, 1},
		{ "gpt", "Lab3.Example: get/create page table at the given VA" ,command_get_page_table, 1},
		{ "nr", "Lab4.Example: show the number of references of the physical frame" ,command_nr, 1},
		{ "fp", "Lab4.Example: free one page in the user space at the given virtual address", command_fp, 1},
		{ "p2v", "Lab5.HandsOn: find virtual address that's corresponding to the given physical address", command_p2v, 1},
		{ "v2p", "Lab5.HandsOn: find physical address that's corresponding to the given virtual address", command_v2p, 1},
		{ "sm", "Lab5.HandsOn: display the mapping info for the given virtual address", command_shmp, 1},
		{ "ft", "Lab6.Example: free (remove) the page table at the given VA", command_ft, 1},
		{ "kill", "kill the given environment (by its ID) from the system", command_kill_program, 1},
		{ "schedRR", "switch the scheduler to RR with given quantum", command_sch_RR, 1},
		{"schedTest", "Used for turning on/off the scheduler test", command_sch_test, 1},
		{"lru", "set replacement algorithm to LRU", command_set_page_rep_LRU, 1},
		{"modbufflength", "set the length of the modified buffer", command_set_modified_buffer_length, 1},
		{ "setStarvThr", "set the the starvation threshold of priority scheduler", command_set_starve_thresh, 1},

		//******************************//
		/* COMMANDS WITH TWO ARGUMENTS */
		//******************************//
		{ "wm", "Lab2.Example: writes one byte to specific physical location" ,command_writemem_k, 2},
		{ "shr", "Lab3.Example: share one page on another" ,command_share_page, 2},
		{ "asp", "Lab5.HandsOn: allocate shared page with the given two virtual addresses" ,command_asp, 2},
		{ "cfp", "Lab5.HandsOn: count the number of free pages in the given range", command_cfp, 2},
		{ "rut", "remove a page table at the given VA from the given user environment ID", command_remove_table, 2},
		{ "schedBSD", "switch the scheduler to BSD with given # queues & quantum", command_sch_BSD, 2},
		{ "setPri", "set the priority of the given environment (by its ID)", command_set_priority, 2},
		{"nclock", "set replacement algorithm to Nth chance CLOCK (type=1: NORMAL Ver. type=2: MODIFIED Ver.", command_set_page_rep_nthCLOCK, 2},

		//********************************//
		/* COMMANDS WITH THREE ARGUMENTS */
		//********************************//
		{ "rub", "reads block of bytes from specific location in given environment" ,command_readuserblock, 3},
		{ "schedPRIRR", "switch the scheduler to PRIORITY RR with given #priorities, quantum and starvation threshold", command_sch_PRIRR, 3},

		//**************************************//
		/* COMMANDS WITH AT LEAST ONE ARGUMENT */
		//**************************************//
		{ "ap", "Lab4.Example: allocate one page [if not exists] in the user space and optionally initialize it by 0s", command_ap, -1},
		{ "sp", "Lab5.HandsOn: set the desired permission to a given virtual address page", command_scp, -1},
		{ "sr", "Lab5.HandsOn: shares the physical frames of the first virtual range with the 2nd virtual range", command_shrr, -1},
		{ "run", "runs a single user program", command_run_program, -1 },
		{ "wum", "writes one byte to specific location in kernel or a given environment" ,command_writeusermem, -1},
		{ "rum", "reads one byte from specific location in kernel or a given environment" ,command_readusermem, -1},
		{ "aup", "allocate a user page with the given r/w permission", command_allocuserpage, -1},
		{ "schedMLFQ", "switch the scheduler to MLFQ with given # queues & quantums", command_sch_MLFQ, -1},
		{"load", "load a single user program to mem with status = NEW", commnad_load_env, -1},
		{"tst", "run the given test", command_tst, -1},

};

//Number of commands = size of the array / size of command structure
uint32 NUM_OF_COMMANDS  = (sizeof(commands)/sizeof(struct Command));


/***** Implementations of basic kernel command prompt commands *****/

//print name and description of each command
int command_help(int number_of_arguments, char **arguments)
{

	int i;
	for (i = 0; i < NUM_OF_COMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].description);

	cprintf("-------------------\n");

	for (i = 0; i < NUM_USER_PROGS; i++)
		cprintf("run %s - %s [User Program]\n", ptr_UserPrograms[i].name, ptr_UserPrograms[i].desc);
	return 0;
}

//print information about kernel addresses and kernel size
int command_kernel_info(int number_of_arguments, char **arguments )
{
	extern char start_of_kernel[], end_of_kernel_code_section[], start_of_uninitialized_data_section[], end_of_kernel[];

	cprintf("Special kernel symbols:\n");
	cprintf("  Start Address of the kernel 			%08x (virt)  %08x (phys)\n", start_of_kernel, start_of_kernel - KERNEL_BASE);
	cprintf("  End address of kernel code  			%08x (virt)  %08x (phys)\n", end_of_kernel_code_section, end_of_kernel_code_section - KERNEL_BASE);
	cprintf("  Start addr. of uninitialized data section 	%08x (virt)  %08x (phys)\n", start_of_uninitialized_data_section, start_of_uninitialized_data_section - KERNEL_BASE);
	cprintf("  End address of the kernel   			%08x (virt)  %08x (phys)\n", end_of_kernel, end_of_kernel - KERNEL_BASE);
	cprintf("Kernel executable memory footprint: %d KB\n",
			(end_of_kernel-start_of_kernel+1023)/1024);
	return 0;
}

//*****************************************************************************************//
//************************************ LAB COMMANDS ***************************************//
//*****************************************************************************************//
//===========================================================================
//Lab2.Examples
//=============
int command_writemem_k(int number_of_arguments, char **arguments)
{
	//TODO: LAB2 Example: corresponding command name is "wm"

	unsigned char* address = (unsigned char*)strtol(arguments[1], NULL, 16)+KERNEL_BASE;
	int stringLen = strlen(arguments[2]);

	for(int i=0;i < stringLen; i++)
	{
		*address = arguments[2][i];
		address++;
	}
	return 0;
}

int command_readmem_k(int number_of_arguments, char **arguments)
{
	//TODO: LAB2 Example: corresponding command name is "rm"

	unsigned int address = strtol(arguments[1], NULL, 16);
	unsigned char *ptr = (unsigned char *)(address + KERNEL_BASE) ;
	//Read value at the given address
	cprintf("value at virtual address %x = %c\n", ptr, *ptr);
	return 0;
}

//===========================================================================
//Lab2.Hands.On
//=============
//TODO: LAB2 Hands-on: write the command function here

//===========================================================================
//Lab3.Examples
//=============
int command_get_page_table(int number_of_arguments, char **arguments)
{
	//TODO: LAB3 Example#1,2: fill this function. corresponding command name is "gpt"
	//Comment the following line
	panic("Function is not implemented yet!");

	return 0;
}

int command_kernel_base_info(int number_of_arguments, char **arguments)
{
	//TODO: LAB3 Example#3: fill this function. corresponding command name is "ikb"
	//Comment the following line
	panic("Function is not implemented yet!");

	return 0;
}

int command_del_kernel_base(int number_of_arguments, char **arguments)
{
	//TODO: LAB3 Example#4: fill this function. corresponding command name is "dkb"
	//Comment the following line
	panic("Function is not implemented yet!");

	return 0;
}

int command_share_page(int number_of_arguments, char **arguments)
{
	//TODO: LAB3 Example#5: fill this function. corresponding command name is "shr"
	//Comment the following line
	panic("Function is not implemented yet!");

	return 0;
}

//===========================================================================
//Lab4.Examples
//==============
//[1] Number of references on the given physical address
int command_nr(int number_of_arguments, char **arguments)
{
	//TODO: LAB4 Example#1: corresponding command name is "nr"

	uint32 pa  = strtol(arguments[1], NULL, 16);
	int num_of_ref = num_of_references(pa);
	cprintf("Num of ref's @ pa %x = %d\n", pa, num_of_ref);
	return 0;
}

//[2] Allocate Page
int command_ap(int number_of_arguments, char **arguments)
{
	//TODO: LAB4 Example#2: corresponding command name is "ap"

	uint32 va = strtol(arguments[1], NULL, 16);
	bool set_to_zero = 0;
	if (number_of_arguments == 3)
	{
		if (arguments[2][0] == '0')
			set_to_zero = 1;
	}
	int ret = alloc_page(ptr_page_directory, va, PERM_USER | PERM_WRITEABLE, set_to_zero);
	if (ret == 1)
	{
		cprintf("Page @ va %x already exists!\n", va);
	}
	else if (ret == E_NO_MEM)
	{
		cprintf("No enough memory!\n");
	}
	else
	{
		cprintf("New page is allocated @ va %x\n", va);
		if (set_to_zero)
			cprintf("and initialized by ZERO\n");
	}
	return 0 ;
}

//[3] Free Page: Un-map a single page at the given virtual address in the user space
int command_fp(int number_of_arguments, char **arguments)
{
	//TODO: LAB4 Example#3: corresponding command name is "fp"

	uint32 va = strtol(arguments[1], NULL, 16);
	unmap_frame(ptr_page_directory, va);

	return 0;
}

//===========================================================================
//Lab4.Hands-on
//==============
//Count Free Pages in Range
int command_cfp(int number_of_arguments, char **arguments)
{
	//TODO: LAB4 Hands-on: corresponding command name is "cfp"

	uint32* dir_ptr = clone_kern_dir();

	uint32 va1 = strtol(arguments[1], NULL, 16);
	uint32 va2 = strtol(arguments[2], NULL, 16);

	uint32 num_of_free = calculate_free_space(dir_ptr, va1, va2);

	cprintf("Num of free pages in va range [%x, %x) = %d\n", va1, va2, num_of_free);
	return 0;
}

//===========================================================================
//Lab5.Hands.On
//=============
int command_asp(int number_of_arguments, char **arguments)
{
	//TODO: LAB5 Hands-on: corresponding command name is "asp"

	uint32* dir_ptr = clone_kern_dir();
	uint32 va1 = strtol(arguments[1], NULL, 16);
	uint32 va2 = strtol(arguments[2], NULL, 16);
	int ret = alloc_shared_page(dir_ptr, va1, dir_ptr, va2, PERM_USER | PERM_WRITEABLE);
	if (ret == E_NO_MEM)
	{
		cprintf("No enough memory!\n");
	}
	return 0;
}

int command_shmp(int number_of_arguments, char **arguments)
{
	//TODO: LAB5 Hands-on: fill this function. corresponding command name is "sm"
	//Comment the following line
	panic("Function is not implemented yet!");

	return 0 ;
}

int command_scp(int number_of_arguments, char **arguments)
{
	//TODO: LAB5 Hands-on: fill this function. corresponding command name is "sp"
	//Comment the following line
	panic("Function is not implemented yet!");

	return 0 ;
}

int command_shrr(int number_of_arguments, char **arguments)
{
	//TODO: LAB5 Hands-on: fill this function. corresponding command name is "sr"
	//Comment the following line
	panic("Function is not implemented yet!");

	return 0;
}

int command_v2p(int number_of_arguments, char **arguments)
{
	//TODO: LAB5 Hands-on: corresponding command name is "v2p"

	uint32 va = strtol(arguments[1], NULL, 16);
	uint32 pa = virtual_to_physical(ptr_page_directory, va);
	if (pa == -1)
		cprintf("NOT FOUND\n");
	else
		cprintf("PA of VA %x = %x\n", va, pa);
	return 0;
}

int command_p2v(int number_of_arguments, char **arguments)
{
	//TODO: LAB5 Hands-on: corresponding command name is "p2v"

	uint32 pa = strtol(arguments[1], NULL, 16);
	uint32 va = physical_to_virtual(ptr_page_directory, pa);
	if (va == 0xFFFFFFFF)
		cprintf("NOT FOUND\n");
	else
		cprintf("VA of PA %x = %x\n", pa, va);
	return 0;
}

//===========================================================================
//Lab6.Examples
//=============
int command_ft(int number_of_arguments, char **arguments)
{
	//TODO: LAB6 Example: corresponding command name is "ft"

	uint32 va = strtol(arguments[1], NULL, 16);
	del_page_table(ptr_page_directory, va);

	return 0;
}


//*****************************************************************************************//
//***************************** UTILITY/TESING COMMANDS ***********************************//
//*****************************************************************************************//
int command_writeusermem(int number_of_arguments, char **arguments)
{
	//deal with the kernel page directory
	if (number_of_arguments == 3)
	{
		unsigned int address = strtol(arguments[1], NULL, 16);
		unsigned char *ptr = (unsigned char *)(address) ;

		*ptr = arguments[2][0];
	}
	//deal with a page directory of specific environment
	else if (number_of_arguments == 4)
	{
		int32 envId = strtol(arguments[1],NULL, 10);
		struct Env* env = NULL;
		envid2env(envId, &env, 0 );

		int address = strtol(arguments[2], NULL, 16);

		if(env == NULL) return 0;

		uint32 oldDir = rcr3();
		//lcr3((uint32) K_PHYSICAL_ADDRESS( env->env_pgdir));
		lcr3((uint32) (env->env_cr3));

		unsigned char *ptr = (unsigned char *)(address) ;

		//Write the given Character
		*ptr = arguments[3][0];
		lcr3(oldDir);
	}
	else
	{
		cprintf("wum command: invalid number of arguments\n") ;
	}
	return 0;
}


int command_readusermem(int number_of_arguments, char **arguments)
{
	//deal with the kernel page directory
	if (number_of_arguments == 2)
	{
		unsigned int address = strtol(arguments[1], NULL, 16);
		unsigned char *ptr = (unsigned char *)(address ) ;

		cprintf("value at address %x = %c\n", ptr, *ptr);
	}
	//deal with a page directory of specific environment
	else if (number_of_arguments == 3)
	{
		int32 envId = strtol(arguments[1],NULL, 10);
		struct Env* env = NULL;
		envid2env(envId, &env, 0 );

		int address = strtol(arguments[2], NULL, 16);

		if(env == NULL) return 0;

		uint32 oldDir = rcr3();
		//lcr3((uint32) K_PHYSICAL_ADDRESS( env->env_pgdir));
		lcr3((uint32)( env->env_cr3));

		unsigned char *ptr = (unsigned char *)(address) ;

		//Write the given Character
		cprintf("value at address %x = %c\n", address, *ptr);

		lcr3(oldDir);
	}
	else
	{
		cprintf("rum command: invalid number of arguments\n") ;
	}
	return 0;

}


int command_readuserblock(int number_of_arguments, char **arguments)
{
	int32 envId = strtol(arguments[1],NULL, 10);
	struct Env* env = NULL;
	envid2env(envId, &env, 0 );

	int address = strtol(arguments[2], NULL, 16);
	int nBytes = strtol(arguments[3], NULL, 10);

	unsigned char *ptr = (unsigned char *)(address) ;
	//Write the given Character

	if(env == NULL) return 0;

	uint32 oldDir = rcr3();
	//lcr3((uint32) K_PHYSICAL_ADDRESS( env->env_pgdir));
	lcr3((uint32)( env->env_cr3));

	int i;
	for(i = 0;i<nBytes; i++)
	{
		cprintf("%08x : %02x  %c\n", ptr, *ptr, *ptr);
		ptr++;
	}
	lcr3(oldDir);

	return 0;
}

int command_remove_table(int number_of_arguments, char **arguments)
{
	int32 envId = strtol(arguments[1],NULL, 10);
	struct Env* env = NULL;
	envid2env(envId, &env, 0 );
	if(env == 0) return 0;

	uint32 address = strtol(arguments[2], NULL, 16);
	unsigned char *va = (unsigned char *)(address) ;
	uint32 table_pa = env->env_page_directory[PDX(address)] & 0xFFFFF000;

	//remove the table
	if(USE_KHEAP && !CHECK_IF_KERNEL_ADDRESS(va))
	{
		kfree((void*)kheap_virtual_address(table_pa));
	}
	else
	{
		// get the physical address and FrameInfo of the page table
		struct FrameInfo *table_FrameInfo = to_frame_info(table_pa);
		// set references of the table frame to 0 then free it by adding
		// to the free frame list
		table_FrameInfo->references = 0;
		free_frame(table_FrameInfo);
	}

	// set the corresponding entry in the directory to 0
	uint32 dir_index = PDX(va);
	env->env_page_directory[dir_index] &= (~PERM_PRESENT);
	tlbflush();
	return 0;
}

int command_allocuserpage(int number_of_arguments, char **arguments)
{
	if (number_of_arguments < 3 || number_of_arguments > 4)
	{
		cprintf("aup command: invalid number of arguments\n") ;
		return 0;
	}
	int32 envId = strtol(arguments[1],NULL, 10);
	struct Env* env = NULL;

	envid2env(envId, &env, 0 );
	if(env == 0) return 0;

	uint32 va = strtol(arguments[2], NULL, 16);

	// Allocate a single frame from the free frame list
	struct FrameInfo * ptr_FrameInfo ;
	int ret = allocate_frame(&ptr_FrameInfo);
	if (ret == E_NO_MEM)
	{
		cprintf("ERROR: no enough memory\n");
		return 0;
	}

	if (number_of_arguments == 3)
	{
		// Map this frame to the given user virtual address with PERM_WRITEABLE
		map_frame(env->env_page_directory, ptr_FrameInfo, va, PERM_WRITEABLE | PERM_USER);
	}
	else if (number_of_arguments == 4)
	{
		// Map this frame to the given user virtual address with the given permission
		uint32 rw ;
		if (arguments[3][0] == 'r' || arguments[3][0] == 'R')
			rw = 0 ;
		else if (arguments[3][0] == 'w' || arguments[3][0] == 'W')
			rw = PERM_WRITEABLE ;
		else
		{
			cprintf("aup command: wrong permission (r/w)... will continue as writable\n") ;
			rw = PERM_WRITEABLE ;
		}

		map_frame(env->env_page_directory, ptr_FrameInfo, va, rw | PERM_USER);
	}
	return 0;
}


//*****************************************************************************************//
//***************************** PROJECT HELPERS COMMAND ***********************************//
//*****************************************************************************************//

int command_meminfo(int number_of_arguments, char **arguments)
{
	struct freeFramesCounters counters =calculate_available_frames();
	cprintf("Total available frames = %d\nFree Buffered = %d\nFree Not Buffered = %d\nModified = %d\n",
			counters.freeBuffered+ counters.freeNotBuffered+ counters.modified, counters.freeBuffered, counters.freeNotBuffered, counters.modified);

	cprintf("Num of calls for kheap_virtual_address [in last run] = %d\n", numOfKheapVACalls);

	return 0;
}

//2020
struct Env * CreateEnv(int number_of_arguments, char **arguments)
{
	struct Env* env;
	uint32 pageWSSize = __PWS_MAX_SIZE;		//arg#3 default
	uint32 LRUSecondListSize = 0;			//arg#4 default
	uint32 percent_WS_pages_to_remove = 0;	//arg#5 default
	int BSDSchedNiceVal = -100;				//arg#5 default
	int PRIRRSchedPriority = 0;				//arg#5 default

#if USE_KHEAP
	{
		switch (number_of_arguments)
		{
		case 5:
			if(!isPageReplacmentAlgorithmLRU(PG_REP_LRU_LISTS_APPROX))
			{
				cprintf("ERROR: Current Replacement is NOT LRU LISTS, invalid number of args\nUsage: <command> <prog_name> <page_WS_size> [<LRU_second_list_size>] [<BSD_Sched_Nice>]\naborting...\n");
				return NULL;
			}
			//percent_WS_pages_to_remove = strtol(arguments[4], NULL, 10);
			if (isSchedMethodBSD())
				BSDSchedNiceVal = strtol(arguments[4], NULL, 10);
			else if (isSchedMethodPRIRR())
				PRIRRSchedPriority = strtol(arguments[4], NULL, 10);

			LRUSecondListSize = strtol(arguments[3], NULL, 10);
			pageWSSize = strtol(arguments[2], NULL, 10);
			break;
		case 4:
			if(!isPageReplacmentAlgorithmLRU(PG_REP_LRU_LISTS_APPROX))
			{
				//percent_WS_pages_to_remove = strtol(arguments[3], NULL, 10);
				if (isSchedMethodBSD())
					BSDSchedNiceVal = strtol(arguments[3], NULL, 10);
				else if (isSchedMethodPRIRR())
					PRIRRSchedPriority = strtol(arguments[3], NULL, 10);			}
			else
			{
				LRUSecondListSize = strtol(arguments[3], NULL, 10);
			}
			pageWSSize = strtol(arguments[2], NULL, 10);
			break;
		case 3:
			if(isPageReplacmentAlgorithmLRU(PG_REP_LRU_LISTS_APPROX))
			{
				cprintf("ERROR: Current Replacement is LRU LISTS, Please specify a working set size in the 3rd arg and LRU second list size in the 4th arg, aborting.\n");
				return NULL;
			}
			pageWSSize = strtol(arguments[2], NULL, 10);
			break;
		default:
			cprintf("ERROR: invalid number of args\nUsage: <command> <prog_name> <page_WS_size> [<LRU_second_list_size>] [<DYN_LOC_SCOPE_percent_WS_to_remove>]\naborting...\n");
			return NULL;

			break;
		}
#if USE_KHEAP == 0
		if(pageWSSize > __PWS_MAX_SIZE)
		{
			cprintf("ERROR: size of WS must be less than or equal to %d... aborting", __PWS_MAX_SIZE);
			return NULL;
		}
#endif
		if(isPageReplacmentAlgorithmLRU(PG_REP_LRU_LISTS_APPROX))
		{
			if (LRUSecondListSize > pageWSSize - 1)
			{
				cprintf("ERROR: size of LRU second list can't equal/exceed the size of the page WS... aborting\n");
				return NULL;
			}
		}
		assert(percent_WS_pages_to_remove >= 0 && percent_WS_pages_to_remove <= 100);

			//assert(PRIRRSchedPriority >= 0 && PRIRRSchedPriority < num_of_ready_queues);
//		if (BSDSchedNiceVal != -100)
//		{
//			cprintf("nice value = %d\n", BSDSchedNiceVal);
//			assert(BSDSchedNiceVal >= -20 && BSDSchedNiceVal <= 20);
//		}
	}
#else
	{
		switch (number_of_arguments)
		{
		case 3:
			percent_WS_pages_to_remove = strtol(arguments[2], NULL, 10);
			break;
		case 2:
			break;
		default:
			cprintf("ERROR: invalid number of args\nUsage: <command> <prog_name> [<DYN_LOC_SCOPE_percent_WS_to_remove>]\naborting...\n");
			return NULL;

			break;
		}
		if(isPageReplacmentAlgorithmLRU(PG_REP_LRU_LISTS_APPROX))
		{
			LRUSecondListSize = __LRU_SNDLST_SIZE;
		}
	}
#endif
	assert(percent_WS_pages_to_remove >= 0 && percent_WS_pages_to_remove <= 100);
	env = env_create(arguments[1], pageWSSize, LRUSecondListSize, percent_WS_pages_to_remove);
	if (BSDSchedNiceVal != -100)
	{
		cprintf("nice value = %d\n", BSDSchedNiceVal);
		assert(BSDSchedNiceVal >= -20 && BSDSchedNiceVal <= 20);
		env_set_nice(env, BSDSchedNiceVal);
	}
	if (isSchedMethodPRIRR())
		env_set_priority(env->env_id, PRIRRSchedPriority);

	return env;
}

int command_run_program(int number_of_arguments, char **arguments)
{
	//[1] Create and initialize a new environment for the program to be run
	struct Env *env = CreateEnv(number_of_arguments, arguments);

	if(env == NULL) return 0;
	cprintf("\nEnvironment Id= %d\n",env->env_id);

	//[2] Place it in the NEW queue
	sched_new_env(env);

	numOfKheapVACalls = 0;

	//[3] Run the created environment by adding it to the "ready" queue then invoke the scheduler to execute it
	sched_run_env(env->env_id);

	return 0;
}

int command_kill_program(int number_of_arguments, char **arguments)
{
	int32 envId = strtol(arguments[1],NULL, 10);

	sched_kill_env(envId);

	return 0;
}

int commnad_load_env(int number_of_arguments, char **arguments)
{
	struct Env *env = CreateEnv(number_of_arguments, arguments);
	if (env == NULL)
		return 0 ;

	sched_new_env(env) ;

	cprintf("\nEnvironment Id= %d\n",env->env_id);
	return 0;
}

int command_run_all(int number_of_arguments, char **arguments)
{
	numOfKheapVACalls = 0;
	sched_run_all();

	return 0 ;
}

int command_print_all(int number_of_arguments, char **arguments)
{
	sched_print_all();

	return 0 ;
}

int command_kill_all(int number_of_arguments, char **arguments)
{
	sched_kill_all();

	return 0 ;
}

int command_set_page_rep_LRU(int number_of_arguments, char **arguments)
{
	if (number_of_arguments < 2)
	{
		cprintf("ERROR: please specify the LRU Approx Type (1: TimeStamp Approx, 2: Lists Approx), aborting...\n");
		return 0;
	}
	int LRU_TYPE = strtol(arguments[1], NULL, 10) ;
	if (LRU_TYPE == PG_REP_LRU_TIME_APPROX)
	{
		setPageReplacmentAlgorithmLRU(LRU_TYPE);
		cprintf("Page replacement algorithm is now LRU with TimeStamp approximation\n");
	}
	else if (LRU_TYPE == PG_REP_LRU_LISTS_APPROX)
	{
		setPageReplacmentAlgorithmLRU(LRU_TYPE);
		cprintf("Page replacement algorithm is now LRU with LISTS approximation\n");
	}
	else
	{
		cprintf("ERROR: Invalid LRU Approx Type (1: TimeStamp Approx, 2: Lists Approx), aborting...\n");
		return 0;
	}
	return 0;
}
//2021
int command_set_page_rep_nthCLOCK(int number_of_arguments, char **arguments)
{
	uint32 PageWSMaxSweeps = strtol(arguments[1], NULL, 10);
	uint8 type = strtol(arguments[2], NULL, 10);
	if (PageWSMaxSweeps <= 0)
	{
		cprintf("Invalid number of sweeps! it should be +ve.\n");
		return 0;
	}
	if (type == 1)		PageWSMaxSweeps = PageWSMaxSweeps * 1;
	else if (type == 2)	PageWSMaxSweeps = PageWSMaxSweeps * -1;
	else
	{
		cprintf("Invalid type!\n	type=1: NORMAL Ver. type=2: MODIFIED Ver.\n");
		return 0;
	}
	setPageReplacmentAlgorithmNchanceCLOCK(PageWSMaxSweeps);
	cprintf("Page replacement algorithm is now N chance CLOCK\n");
	return 0;
}
int command_set_page_rep_CLOCK(int number_of_arguments, char **arguments)
{
	setPageReplacmentAlgorithmCLOCK();
	cprintf("Page replacement algorithm is now CLOCK\n");
	return 0;
}

int command_set_page_rep_FIFO(int number_of_arguments, char **arguments)
{
	setPageReplacmentAlgorithmFIFO();
	cprintf("Page replacement algorithm is now FIFO\n");
	return 0;
}

int command_set_page_rep_ModifiedCLOCK(int number_of_arguments, char **arguments)
{
	setPageReplacmentAlgorithmModifiedCLOCK();
	cprintf("Page replacement algorithm is now Modified CLOCK\n");
	return 0;
}

int command_set_page_rep_OPTIMAL(int number_of_arguments, char **arguments)
{
	setPageReplacmentAlgorithmOPTIMAL();
	cprintf("Page replacement algorithm is now OPTIMAL\n");
	return 0;
}

/*2018*///BEGIN======================================================
int command_sch_RR(int number_of_arguments, char **arguments)
{
	uint8 quantum = strtol(arguments[1], NULL, 10);

	sched_init_RR(quantum);
	cprintf("Scheduler is now set to Round Robin with quantum %d ms\n", quantums[0]);
	return 0;
}
int command_sch_MLFQ(int number_of_arguments, char **arguments)
{
	uint8 numOfLevels = strtol(arguments[1], NULL, 10);
	uint8 quantumOfEachLevel[MAX_ARGUMENTS - 2] ;
	for (int i = 2 ; i < number_of_arguments ; i++)
	{
		quantumOfEachLevel[i-2] = strtol(arguments[i], NULL, 10);
	}

	sched_init_MLFQ(numOfLevels, quantumOfEachLevel);

	cprintf("Scheduler is now set to MLFQ with quantums: ");
	for (int i = 0 ; i < num_of_ready_queues; i++)
	{
		cprintf("%d   ", quantums[i]) ;
	}
	cprintf("\n");
	return 0;
}
int command_sch_BSD(int number_of_arguments, char **arguments)
{
	uint8 numOfLevels = strtol(arguments[1], NULL, 10);
	uint8 quantum = strtol(arguments[2], NULL, 10);

	sched_init_BSD(numOfLevels, quantum);

	cprintf("Scheduler is now set to BSD with %d levels & quantum = %d\n", numOfLevels, quantum);
	cprintf("\n");
	return 0;
}

int command_sch_PRIRR(int number_of_arguments, char **arguments)
{
	uint8 numOfLevels = strtol(arguments[1], NULL, 10);
	uint8 quantum = strtol(arguments[2], NULL, 10);
	uint32 starvThresh = strtol(arguments[3], NULL, 10);

	sched_init_PRIRR(numOfLevels, quantum, starvThresh);

	cprintf("Scheduler is now set to PRIORITY RR with %d priorities, quantum = %d and starvation thresh %d\n", numOfLevels, quantum, starvThresh);
	cprintf("\n");
	return 0;
}
int command_set_starve_thresh(int number_of_arguments, char **arguments)
{
	uint32 starvationThresh = strtol(arguments[1], NULL, 10);
	sched_set_starv_thresh(starvationThresh);
	return 0;
}
//*********************************************************************************//

int command_set_priority(int number_of_arguments, char **arguments)
{
	int32 envId = strtol(arguments[1],NULL, 10);
	int32 priority = strtol(arguments[2],NULL, 10);

	env_set_priority(envId, priority);

	return 0;
}
int command_print_sch_method(int number_of_arguments, char **arguments)
{
	if (isSchedMethodMLFQ())
	{
		cprintf("Current scheduler method is MLFQ with quantums: ");
		for (int i = 0 ; i < num_of_ready_queues; i++)
		{
			cprintf("%d   ", quantums[i]) ;
		}
		cprintf("\n");
	}
	else if (isSchedMethodRR())
	{
		cprintf("Current scheduler method is Round Robin with quantum %d ms\n", quantums[0]);
	}
	else if (isSchedMethodBSD())
	{
		cprintf("Scheduler is now set to BSD with %d levels & quantum = %d\n", num_of_ready_queues, quantums[0]);
	}
	else if (isSchedMethodPRIRR())
	{
		cprintf("Scheduler is now set to PRIORITY RR with %d priorities & quantum = %d\n", num_of_ready_queues, quantums[0]);
	}
	else
		cprintf("Current scheduler method is UNDEFINED\n");

	return 0;
}
int command_sch_test(int number_of_arguments, char **arguments)
{
	int status  = strtol(arguments[1], NULL, 10);
	chksch(status);
	if (status == 0)
		cprintf("Testing the scheduler is TURNED OFF\n");
	else if (status == 1)
		cprintf("Testing the scheduler is TURNED ON\n");
	return 0;
}

/*2018*///END======================================================


/*2015*///BEGIN======================================================
int command_print_page_rep(int number_of_arguments, char **arguments)
{
	if (isPageReplacmentAlgorithmCLOCK())
		cprintf("Page replacement algorithm is CLOCK\n");
	else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX))
		cprintf("Page replacement algorithm is LRU with TimeStamp approximation\n");
	else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_LISTS_APPROX))
		cprintf("Page replacement algorithm is LRU with LISTS approximation\n");
	else if (isPageReplacmentAlgorithmFIFO())
		cprintf("Page replacement algorithm is FIFO\n");
	else if (isPageReplacmentAlgorithmModifiedCLOCK())
		cprintf("Page replacement algorithm is Modified CLOCK\n");
	else if (isPageReplacmentAlgorithmOPTIMAL())
		cprintf("Page replacement algorithm is OPTIMAL\n");
	else if (isPageReplacmentAlgorithmNchanceCLOCK())
	{
		cprintf("Page replacement algorithm is Nth Chance CLOCK ");
		if (page_WS_max_sweeps > 0)			cprintf("[NORMAL ver]\n");
		else if (page_WS_max_sweeps < 0)	cprintf("[MODIFIED ver]\n");
	}
	else
		cprintf("Page replacement algorithm is UNDEFINED\n");

	return 0;
}


int command_set_uheap_plac_FIRSTFIT(int number_of_arguments, char **arguments)
{
	set_uheap_strategy(UHP_PLACE_FIRSTFIT);
	cprintf("User Heap placement strategy is now FIRST FIT\n");
	return 0;
}

int command_set_uheap_plac_BESTFIT(int number_of_arguments, char **arguments)
{
	set_uheap_strategy(UHP_PLACE_BESTFIT);
	cprintf("User Heap placement strategy is now BEST FIT\n");
	return 0;
}

int command_set_uheap_plac_NEXTFIT(int number_of_arguments, char **arguments)
{
	set_uheap_strategy(UHP_PLACE_NEXTFIT);
	cprintf("User Heap placement strategy is now NEXT FIT\n");
	return 0;
}
int command_set_uheap_plac_WORSTFIT(int number_of_arguments, char **arguments)
{
	set_uheap_strategy(UHP_PLACE_WORSTFIT);
	cprintf("User Heap placement strategy is now WORST FIT\n");
	return 0;
}
int command_set_uheap_plac_CUSTOMFIT(int number_of_arguments, char **arguments)
{
	set_uheap_strategy(UHP_PLACE_CUSTOMFIT);
	cprintf("User Heap placement strategy is now CUSTOM FIT\n");
	return 0;
}

int command_print_uheap_plac(int number_of_arguments, char **arguments)
{
	uint32 strategy = get_uheap_strategy();
	switch (strategy)
	{
	case UHP_PLACE_FIRSTFIT:
		cprintf("User Heap placement strategy is FIRST FIT\n");
		break;
	case UHP_PLACE_NEXTFIT:
		cprintf("User Heap placement strategy is NEXT FIT\n");
		break;
	case UHP_PLACE_BESTFIT:
		cprintf("User Heap placement strategy is BEST FIT\n");
		break;
	case UHP_PLACE_WORSTFIT:
		cprintf("User Heap placement strategy is WORST FIT\n");
		break;
	case UHP_PLACE_CUSTOMFIT:
		cprintf("User Heap placement strategy is CUSTOM FIT\n");
		break;
	default:
		cprintf("User Heap placement strategy is UNDEFINED\n");
	}
	return 0;
}

/*2015*///END======================================================

/*2017*///BEGIN======================================================

int command_set_kheap_plac_CONTALLOC(int number_of_arguments, char **arguments)
{
	set_kheap_strategy(KHP_PLACE_CONTALLOC);
	cprintf("Kernel Heap placement strategy is now FIRST FIT\n");
	return 0;
}

int command_set_kheap_plac_FIRSTFIT(int number_of_arguments, char **arguments)
{
	set_kheap_strategy(KHP_PLACE_FIRSTFIT);
	cprintf("Kernel Heap placement strategy is now FIRST FIT\n");
	return 0;
}

int command_set_kheap_plac_BESTFIT(int number_of_arguments, char **arguments)
{
	set_kheap_strategy(KHP_PLACE_BESTFIT);
	cprintf("Kernel Heap placement strategy is now BEST FIT\n");
	return 0;
}

int command_set_kheap_plac_NEXTFIT(int number_of_arguments, char **arguments)
{
	set_kheap_strategy(KHP_PLACE_NEXTFIT);
	cprintf("Kernel Heap placement strategy is now NEXT FIT\n");
	return 0;
}
int command_set_kheap_plac_WORSTFIT(int number_of_arguments, char **arguments)
{
	set_kheap_strategy(KHP_PLACE_WORSTFIT);
	cprintf("Kernel Heap placement strategy is now WORST FIT\n");
	return 0;
}
int command_set_kheap_plac_CUSTOMFIT(int number_of_arguments, char **arguments)
{
	set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
	cprintf("Kernel Heap placement strategy is now CUSTOM FIT\n");
	return 0;
}
int command_print_kheap_plac(int number_of_arguments, char **arguments)
{
	uint32 strategy = get_kheap_strategy();
	switch (strategy)
	{
	case KHP_PLACE_CONTALLOC:
		cprintf("Kernel Heap placement strategy is CONTINUOUS ALLOCATION\n");
		break;
	case KHP_PLACE_FIRSTFIT:
		cprintf("Kernel Heap placement strategy is FIRST FIT\n");
		break;
	case KHP_PLACE_NEXTFIT:
		cprintf("Kernel Heap placement strategy is NEXT FIT\n");
		break;
	case KHP_PLACE_BESTFIT:
		cprintf("Kernel Heap placement strategy is BEST FIT\n");
		break;
	case KHP_PLACE_WORSTFIT:
		cprintf("Kernel Heap placement strategy is WORST FIT\n");
		break;
	case KHP_PLACE_CUSTOMFIT:
		cprintf("Kernel Heap placement strategy is CUSTOM FIT\n");
		break;
	default:
		cprintf("Kernel Heap placement strategy is UNDEFINED\n");
	}

	return 0;
}

/*2017*///END======================================================

int command_disable_modified_buffer(int number_of_arguments, char **arguments)
{
	if(!isBufferingEnabled())
	{
		cprintf("Buffering is not enabled. Please enable buffering first.\n");
	}
	else
	{
		enableModifiedBuffer(0);
		cprintf("Modified Buffer is now DISABLED\n");
	}
	return 0;
}


int command_enable_modified_buffer(int number_of_arguments, char **arguments)
{
	if(!isBufferingEnabled())
	{
		cprintf("Buffering is not enabled. Please enable buffering first.\n");
	}
	else
	{
		enableModifiedBuffer(1);
		cprintf("Modified Buffer is now ENABLED\n");
	}
	return 0;
}

/*2016 ============================================================================*/

int command_disable_buffering(int number_of_arguments, char **arguments)
{
	enableBuffering(0);
	enableModifiedBuffer(0);
	cprintf("Buffering is now DISABLED\n");
	return 0;
}


int command_enable_buffering(int number_of_arguments, char **arguments)
{
	enableBuffering(1);
	enableModifiedBuffer(1);
	if(getModifiedBufferLength() == 0)
	{
		cprintf("Modified buffer enabled but with length = 0\n");
		char str[100];
		readline("Please enter the modified buff length = ", str);
		setModifiedBufferLength(strtol(str, NULL, 10));
		cprintf("Modified buffer length updated = %d\n", getModifiedBufferLength());
	}
	cprintf("Buffering is now ENABLED\n");
	return 0;
}

int command_set_modified_buffer_length(int number_of_arguments, char **arguments)
{
	if(!isBufferingEnabled())
	{
		cprintf("Buffering is not enabled. Please enable buffering to use the modified buffer.\n");
	}
	else if (!isModifiedBufferEnabled())
	{
		cprintf("Modified Buffering is not enabled. Please enable modified buffering.\n");
	}
	setModifiedBufferLength(strtol(arguments[1], NULL, 10));
	cprintf("Modified buffer length updated = %d\n", getModifiedBufferLength());
	return 0;
}

int command_get_modified_buffer_length(int number_of_arguments, char **arguments)
{
	if(!isBufferingEnabled())
	{
		cprintf("Buffering is not enabled. Please enable buffering to use the modified buffer.\n");
	}
	else if (!isModifiedBufferEnabled())
	{
		cprintf("Modified Buffering is not enabled. Please enable modified buffering.\n");
	}
	cprintf("Modified buffer length = %d\n", getModifiedBufferLength());
	return 0;
}

int command_tst(int number_of_arguments, char **arguments)
{
	return tst_handler(number_of_arguments, arguments);
}

// *************** This clear screen feature is implemented by *************
// ********* Abd-Alrahman Zedan From Team Frozen-Bytes - FCIS'24-25 ********
int command_cls(int number_of_arguments, char **arguments)
{
	clear_screen_buffer();
	return 0;
}
