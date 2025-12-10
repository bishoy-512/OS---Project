// Kernel-level Semaphore

#include "inc/types.h"
#include "inc/x86.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/environment_definitions.h"
#include "inc/assert.h"
#include "inc/string.h"
#include "ksemaphore.h"
#include "channel.h"
#include "../cpu/cpu.h"
#include "../proc/user_environment.h"

void init_ksemaphore(struct ksemaphore *ksem, int value, char *name)
{
	init_channel(&(ksem->chan), "ksemaphore channel");
	init_kspinlock(&(ksem->lk), "lock of ksemaphore");
	strcpy(ksem->name, name);
	ksem->count = value;
}


void wait_ksemaphore(struct ksemaphore *ksem)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #6 SEMAPHORE - wait_ksemaphore
	//Your code is here

//	el count hna variable shared lazm yt7mme acquire w relese
//	 hnqss el count lw lsaa postive ybqaa fady w tdkhol lw negative m3nah en hd mwgod a a3mllha sleep


	 acquire_kspinlock(&(ksem->lk));
	ksem->count--;
	if(	ksem->count<0){
		sleep(&(ksem->chan),&(ksem->lk));

	}

	release_kspinlock(&(ksem->lk));





	//Comment the following line
//	panic("wait_ksemaphore() is not implemented yet...!!");

}

void signal_ksemaphore(struct ksemaphore *ksem)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #7 SEMAPHORE - signal_ksemaphore
	//Your code is here
//	 fe el signal bqaa  bhawel  afdy mkan wahed fa bcall el wakeupone
//	b3mluu locks zy fo2 3sha dh shared brduu
	acquire_kspinlock(&(ksem->lk));
	ksem->count++;

	if(ksem->count<=0)
	{
		 wakeup_one(&(ksem->chan));
	}






	release_kspinlock(&(ksem->lk));







	//Comment the following line
//	panic("signal_ksemaphore() is not implemented yet...!!");

}


