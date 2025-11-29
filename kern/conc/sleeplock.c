// Sleeping locks

#include "inc/types.h"
#include "inc/x86.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/environment_definitions.h"
#include "inc/assert.h"
#include "inc/string.h"
#include "sleeplock.h"
#include "channel.h"
#include "../cpu/cpu.h"
#include "../proc/user_environment.h"



void init_sleeplock(struct sleeplock *lk, char *name)
{
	init_channel(&(lk->chan), "sleep lock channel");
	char prefix[30] = "lock of sleeplock - ";
	char guardName[30+NAMELEN];
	strcconcat(prefix, name, guardName);
	init_kspinlock(&(lk->lk), guardName);
	strcpy(lk->name, name);
	lk->locked = 0;
	lk->pid = 0;
}

void acquire_sleeplock(struct sleeplock *lk)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #4 SLEEP LOCK - acquire_sleeplock
	//Your code is here
	//Comment the following line
	//panic("acquire_sleeplock() is not implemented yet...!!");
	cprintf("a1.\n");
//	init_sleeplock(lk,"sleep lock initialized in acquire fun");


	cprintf("a2.\n");
	acquire_kspinlock(&(lk->lk)); // guard
	cprintf("a3.\n");
	while(lk->locked){ // mylock   |-> modify in the code here
		cprintf("a4\n");
		sleep(&(lk->chan),&(lk->lk));
		cprintf("a5\n");
	}
	cprintf("a6.\n");
	lk->locked=1;
	cprintf("a7.\n");
	lk->pid = get_cpu_proc()->env_id;
	cprintf("a8.\n");
	release_kspinlock(&(lk->lk)); // guard
	cprintf("a9.\n");


}

//void release_sleeplock(struct sleeplock *lk)
//{
//	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #5 SLEEP LOCK - release_sleeplock
//	//Your code is here
//	//Comment the following line
//	//panic("release_sleeplock() is not implemented yet...!!");
//	cprintf("a9a\n");
////	init_sleeplock(lk,"sleep lock initialized in release fun");
//	cprintf("a10a\n");
//	acquire_kspinlock(&(lk->lk));
//	cprintf("a11\n");
//	struct Channel new_chan=lk->chan;
//	int que_size=queue_size(&(new_chan.queue));
//	cprintf("a12\n");
//	if(que_size){
//		cprintf("a13\n");
//		wakeup_all(&(lk->chan));
//		cprintf("a14\n");
//	}
//	lk->locked=0;
//	cprintf("a15\n");
//	lk->pid = 0;
//	cprintf("a15-ii\n");
//	release_kspinlock(&(lk->lk));
//	cprintf("a16\n");
//}
void release_sleeplock(struct sleeplock *lk)
{
    acquire_kspinlock(&(lk->lk));            // guard
    cprintf("r1\n");


    if(queue_size(&(lk->chan.queue))){
		wakeup_all(&(lk->chan));
		cprintf("r3\n");
    }
    cprintf("r3-i\n");

    lk->locked = 0;

    cprintf("r1\n");

    lk->pid = 0;

    cprintf("r2\n");

    release_kspinlock(&(lk->lk));

    cprintf("r4\n");
}


int holding_sleeplock(struct sleeplock *lk)
{
	int r;
	acquire_kspinlock(&(lk->lk));
	r = lk->locked && (lk->pid == get_cpu_proc()->env_id);
	release_kspinlock(&(lk->lk));
	return r;
}



