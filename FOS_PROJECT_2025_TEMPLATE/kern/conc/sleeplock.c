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
//khtwat el Pseudocode  1-acquire spinlock(&gurad)
//                      2- while ( the lck== busy)
//    	                3- go to sleep
//		                4- ahgezz el lock lya ana bqa
//		                5- release -spinlock(& guard)

{



	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #4 SLEEP LOCK - acquire_sleeplock
	//Your code is here
//	//  lk bta3 sleep accsess el lk bta3 el spin ely gwah 3shan haga  shared mhtaga tt7me
	acquire_kspinlock(&(lk->lk));


//	tol m lck busy nam w lmaa tss7aa akhod el lck lnfsy w lma akhls aft7 el lck
	while(lk->locked==1){

		sleep(&(lk->chan),&(lk->lk));

	                    }

	lk->locked=1;
	release_kspinlock(&(lk->lk));





	//Comment the following line
//	panic("acquire_sleeplock() is not implemented yet...!!");




}

void release_sleeplock(struct sleeplock *lk)
{
//	1-acquiree spin lck
//	2-lw hd kan  mstny bqaa w hsluu sleep hnady wakeup all blocked process
//	3- lock =free 8ery ystkhdmuu
//	4-release spinlock


	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #5 SLEEP LOCK - release_sleeplock
	//Your code is here
//  lk bta3 sleep accsess el lk bta3 el spin ely gwah 3shan haga  shared mhtaga tt7me
	acquire_kspinlock(&(lk->lk));
// bs7e koll elly kanoo mstneen el lk yfdaa w hasolhum sleep 3shan bnady el wakeup all''
	wakeup_all(&(lk->chan));

//	bft7 el lck bqa ely yakhduuu
	lk->locked=0;

	release_kspinlock(&(lk->lk));




//	//Comment the following line
//	panic("release_sleeplock() is not implemented yet...!!");
}

int holding_sleeplock(struct sleeplock *lk)
{
	int r;
	acquire_kspinlock(&(lk->lk));
	r = lk->locked && (lk->pid == get_cpu_proc()->env_id);
	release_kspinlock(&(lk->lk));
	return r;
}



