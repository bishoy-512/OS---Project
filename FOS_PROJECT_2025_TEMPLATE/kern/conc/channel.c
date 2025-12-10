/*
 * channel.c
 *
 *  Created on: Sep 22, 2024
 *      Author: HP
 */
#include "channel.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <inc/string.h>
#include <inc/disk.h>

//===============================
// 1) INITIALIZE THE CHANNEL:
//===============================
// initialize its lock & queue
void init_channel(struct Channel *chan, char *name)
{
	strcpy(chan->name, name);
	init_queue(&(chan->queue));
}

//===============================
// 2) SLEEP ON A GIVEN CHANNEL:
//===============================
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// Ref: xv6-x86 OS code


void sleep(struct Channel *chan, struct kspinlock* lk){
//	bgeb el process ely sh8ala 3leha dlwqty el cpu
struct Env * mycurrent_proccess=get_cpu_proc();
//bahmee el queue 3shan admn m7dsh yhot fe nfs el sanya
acquire_kspinlock(&ProcessQueues.qlock);

//akhdt el process 7ttha blocked
mycurrent_proccess->env_status=ENV_BLOCKED;
//b3den n7otha fe queue bta3na
enqueue(&chan->queue,mycurrent_proccess);
//aft7 el lck bta3y 3shan process 8erha tsht8l
release_kspinlock(lk);
sched();

release_kspinlock(&ProcessQueues.qlock);
//bft7 el lck b3d m as7a 3shan admnn en ely ydkhol my7slsh qflaa ll sysytem kolo
acquire_kspinlock(lk);







	    }
//==================================================
// 3) WAKEUP ONE BLOCKED PROCESS ON A GIVEN CHANNEL:
//==================================================
// Wake up ONE process sleeping on chan.
// The qlock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes
void wakeup_one(struct Channel *chan)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #2 CHANNEL - wakeup_one
	//Your code is here
	struct Env* firstonewakeup;
	acquire_kspinlock(&ProcessQueues.qlock);
//	3rft bs vriable size 3shan  a3rf 3dd el process ely gwaa w cheack lw fe process ys7eha
	int size=queue_size(&chan->queue);
	if(size==0){
//  lw tl3 size=0 aft77 el lck 3shan myqflsh el system kolo (deadlock)
		release_kspinlock(&ProcessQueues.qlock);

	}
	else
//		8er kdhh bakhod mn el queue awl process  kant nayma (fifo)
{firstonewakeup=dequeue(&chan->queue);
//     b7otha readyy bqa  3shan trg3 tkml mn el mkan el wqft 3nduu
	firstonewakeup->env_status=ENV_READY;
//	function bdkhlhaly 3la tool fe ready queue
	sched_insert_ready(firstonewakeup);
//	rg3t ft7t wl lck b3d ma el process tkhlss
	release_kspinlock(&ProcessQueues.qlock);
}









	//Comment the following line
	//panic("wakeup_one() is not implemented yet...!!");
}

//====================================================
// 4) WAKEUP ALL BLOCKED PROCESSES ON A GIVEN CHANNEL:
//====================================================
// Wake up all processes sleeping on chan.
// The queues lock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes

void wakeup_all(struct Channel *chan)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #3 CHANNEL - wakeup_all
	//Your code is here
	struct Env* firstonewakeup;
//	3mlt lck 3shan admn en m7dsh y7ott ay haga zayda w ana b3ml el loop
		acquire_kspinlock(&ProcessQueues.qlock);
		int size=queue_size(&chan->queue);
//hna el loop btmshy btakhod kol ely fe queue w tkhlehum ready
		while (size>0){
			firstonewakeup=dequeue(&chan->queue);
			firstonewakeup->env_status=ENV_READY;
			sched_insert_ready(firstonewakeup);
			size--;



		}
//		hna nft7 el lck 3shann ttmly tany bqaa b3 m ash7y kolo
		release_kspinlock(&ProcessQueues.qlock);

	//Comment the following line
	//panic("wakeup_all() is not implemented yet...!!");
}



