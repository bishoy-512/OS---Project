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

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE ;
}
void setPageReplacmentAlgorithmCLOCK(){_PageRepAlgoType = PG_REP_CLOCK;}
void setPageReplacmentAlgorithmFIFO(){_PageRepAlgoType = PG_REP_FIFO;}
void setPageReplacmentAlgorithmModifiedCLOCK(){_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;}
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal(){_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;}
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps){_PageRepAlgoType = PG_REP_NchanceCLOCK;  page_WS_max_sweeps = PageWSMaxSweeps;}
/*2024*/ void setFASTNchanceCLOCK(bool fast){ FASTNchanceCLOCK = fast; };
/*2025*/ void setPageReplacmentAlgorithmOPTIMAL(){ _PageRepAlgoType = PG_REP_OPTIMAL; };

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE){return _PageRepAlgoType == LRU_TYPE ? 1 : 0;}
uint32 isPageReplacmentAlgorithmCLOCK(){if(_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0;}
uint32 isPageReplacmentAlgorithmFIFO(){if(_PageRepAlgoType == PG_REP_FIFO) return 1; return 0;}
uint32 isPageReplacmentAlgorithmModifiedCLOCK(){if(_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0;}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal(){if(_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK(){if(_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmOPTIMAL(){if(_PageRepAlgoType == PG_REP_OPTIMAL) return 1; return 0;}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt){_EnableModifiedBuffer = enableIt;}
uint8 isModifiedBufferEnabled(){  return _EnableModifiedBuffer ; }

void enableBuffering(uint32 enableIt){_EnableBuffering = enableIt;}
uint8 isBufferingEnabled(){  return _EnableBuffering ; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length;}
uint32 getModifiedBufferLength() { return _ModifiedBufferLength;}

//===============================
// FAULT HANDLERS
//===============================

//==================
// [0] INIT HANDLER:
//==================
void fault_handler_init()
{
	//setPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX);
	//setPageReplacmentAlgorithmOPTIMAL();
	setPageReplacmentAlgorithmCLOCK();
	//setPageReplacmentAlgorithmModifiedCLOCK();
	enableBuffering(0);
	enableModifiedBuffer(0) ;
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
int8 num_repeated_fault  = 0;
extern uint32 sys_calculate_free_frames() ;

struct Env* last_faulted_env = NULL;
void fault_handler(struct Trapframe *tf)
{
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//cprintf("************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env)
	{
		num_repeated_fault++ ;
		if (num_repeated_fault == 3)
		{
			print_trapframe(tf);
			panic("Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n", before_last_fault_va, before_last_eip, fault_va);
		}
	}
	else
	{
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32)tf->tf_eip;
	last_fault_va = fault_va ;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap)
	{
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env && fault_va >= (uint32)cur_env->kstack && fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va >= (uint32)c->stack && fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!", c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else
	{
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL)
	{
		cprintf("\nFaulted VA = %x\n", fault_va);
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ( (faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT) != PERM_PRESENT)
	{
		faulted_env->tableFaultsCounter ++ ;
		table_fault_handler(faulted_env, fault_va);
	}
	else
	{
		if (userTrap)
		{
					//============================================================================================/
					//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #2 Check for invalid pointers
					//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
					//your code is here
			//CASE 1 : If fault address not in user space (kernel space)
			if (fault_va >= KERNEL_BASE)
				env_exit();
			uint32 perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
			uint32 uhpage = perms & PERM_UHPAGE;
			uint32 present = perms & PERM_PRESENT;
			uint32 writeable = perms & PERM_WRITEABLE;
			uint32 user = perms & PERM_USER;

			//CASE 2 : UHPAGE = 0
			if ((fault_va >= USER_HEAP_START) && (fault_va < USER_HEAP_MAX) && !(uhpage))
				env_exit();
			//CASE 3 : IF READ ONLY PAGE
			if (present && !(writeable) )
				env_exit();
			if(present && !user)
				env_exit();
			/*============================================================================================*/
		}

		/*2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
		if (perms & PERM_PRESENT)
			panic("Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n", fault_va) ;
		/*============================================================================================*/


		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter ++ ;

//				cprintf("[%08s] user PAGE fault va %08x\n", faulted_env->prog_name, fault_va);
//				cprintf("\nPage working set BEFORE fault handler...\n");
//				env_page_ws_print(faulted_env);
		//int ffb = sys_calculate_free_frames();

		if(isBufferingEnabled())
		{
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		}
		else
		{
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
void table_fault_handler(struct Env * curenv, uint32 fault_va)
{
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
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
int get_optimal_num_faults(struct WS_List *initWorkingSet, int maxWSSize, struct PageRef_List *pageReferences)
{
	//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #2 get_optimal_num_faults
	//Your code is here
	//Comment the following line
	//panic("get_optimal_num_faults() is not implemented yet...!!");

	//copy VAs from initWorkingSet
	uint32 Active_WS[maxWSSize];
	uint32 ActiveSize = 0;
	struct WorkingSetElement* cur_wsElem = LIST_FIRST(initWorkingSet);

	LIST_FOREACH(cur_wsElem,initWorkingSet){
		if(ActiveSize<maxWSSize && cur_wsElem!=NULL){
			Active_WS[ActiveSize] = ROUNDDOWN(cur_wsElem->virtual_address,PAGE_SIZE);
			ActiveSize++;
		}
		else
			break;
	}

	//loop on the pageRefrences to trace

	struct PageRefElement* cur_ref = NULL;
	int Faults = 0;
	LIST_FOREACH(cur_ref,pageReferences){
		cur_wsElem = NULL;
		bool exist = 0;

		//check if the current Reference is in the Active_WS
		for(int i =0; i<ActiveSize; i++){
			if(Active_WS[i]==ROUNDDOWN(cur_ref->virtual_address,PAGE_SIZE)){
				exist = 1;
				break;
			}
		}
		if(exist){
			//do no thing
			continue;
		}
		else{
			//if not exist in the WS add it to Active_WS
			Faults++;

			if(ActiveSize<maxWSSize){
				//insert cur_ref in the Active_WS
				Active_WS[ActiveSize]=ROUNDDOWN(cur_ref->virtual_address,PAGE_SIZE);
				ActiveSize++;
			}

			else{
				int fartherDis = -1;
				int victim;

				//loop on the WS to get the victim
				for(int i =0; i<ActiveSize; i++){
					int dis = 0;
					bool found = 0;
					struct PageRefElement* scanner;
					//loop on the pageRefrences from the current WS_element->va
					//to calculate the farther Reference
					for(scanner = cur_ref->prev_next_info.le_next;scanner!=NULL;scanner=scanner->prev_next_info.le_next){
						dis ++;
						if(Active_WS[i]==ROUNDDOWN(scanner->virtual_address, PAGE_SIZE)){
							found=1;
							break;
						}
					}
					if(found){
						if(dis>fartherDis){
							fartherDis=dis;
							victim=i;
						}
					}
					else{
						//if not in the pageReference
						victim=i;
						break;
					}
				}
					//insert cur_ref in the Active_WS
					Active_WS[victim]=ROUNDDOWN(cur_ref->virtual_address,PAGE_SIZE);
			}
		}
	}
	return Faults;
}


void page_fault_handler(struct Env * faulted_env, uint32 fault_va)
{
#if USE_KHEAP
	if (isPageReplacmentAlgorithmOPTIMAL())
		{
			//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #1 Optimal Reference Stream
			//Your code is here
			//Comment the following line
			//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");


			//1- Keep track of the Active WS
			struct WorkingSetElement* current=LIST_FIRST(&(faulted_env->page_WS_list));
			if(LIST_SIZE(&(faulted_env->ActiveList))==0  && LIST_SIZE(&(faulted_env->page_WS_list))!=0){
				LIST_FOREACH(current,&(faulted_env->page_WS_list)){
					if(current==NULL)
						break;
					else{
						struct WorkingSetElement* copy = env_page_ws_list_create_element(faulted_env,current->virtual_address);
						LIST_INSERT_TAIL(&(faulted_env->ActiveList),copy);
					}
				}
			}

			//2-If faulted page not in memory, read it from disk
			//Else, just set its present bit

			/*if in MEM*/
			uint32 *ptr_page_table = NULL;
			uint32 va =  ROUNDDOWN(fault_va,PAGE_SIZE);
			struct FrameInfo* ptr_frame_info = get_frame_info(faulted_env->env_page_directory,va, &ptr_page_table);
			if(ptr_frame_info!=NULL){
				pt_set_page_permissions(faulted_env->env_page_directory, ROUNDDOWN(fault_va,PAGE_SIZE), PERM_PRESENT, 0);
			}

			/*if it is the first time*/
			else{
				struct FrameInfo *ptr_frame = NULL;
				int ret = allocate_frame(&ptr_frame);
				if (ret != 0) {
					panic("page_fault_handler: no free frames!");
				}

				ret = map_frame(faulted_env->env_page_directory, ptr_frame,
								ROUNDDOWN(fault_va, PAGE_SIZE),
								PERM_USER | PERM_WRITEABLE | PERM_PRESENT);
				if (ret != 0) {
					panic("page_fault_handler: mapping failed!");
				}

				ret = pf_read_env_page(faulted_env, (void*) ROUNDDOWN(fault_va, PAGE_SIZE));

				if (ret == E_PAGE_NOT_EXIST_IN_PF) {
					if ((fault_va >= USTACKBOTTOM && fault_va < USTACKTOP)
							|| (fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX)) {
						//nothing to do, stack/heap created on demand
					} else {
						unmap_frame(faulted_env->env_page_directory, ROUNDDOWN(fault_va, PAGE_SIZE));
						env_exit();
						return;
					}
				}
			}

			//3-If the faulted page in the Active WS, do nothing
			//Else, if Active WS is FULL, reset present & delete all its pages

			//check is the fault_va is in the ActiveList or not
			//if exist do no thing
			//if not, create WorkingSetElement with fault_va
			bool exist = 0;
			current=LIST_FIRST(&(faulted_env->ActiveList));

			LIST_FOREACH(current,&(faulted_env->ActiveList)){
				if(ROUNDDOWN(current->virtual_address , PAGE_SIZE)==ROUNDDOWN(fault_va,PAGE_SIZE)){
					exist=1;
					break;
				}
			}

			if (exist){
				//do no thing
			}
			else if(!exist){

				//to add NEW WS ELEMENT we should check the size to ActiveList
				//if < MAXSIZE INSERT_TAIL
				//else reset present bit & remove all elements in ActiveList

				if(LIST_SIZE(&(faulted_env->ActiveList))>faulted_env->page_WS_max_size
						|| LIST_SIZE(&(faulted_env->ActiveList))==faulted_env->page_WS_max_size)
				{
					current = LIST_FIRST(&(faulted_env->ActiveList));
					while(current!=NULL){
						struct WorkingSetElement* NEXT = current->prev_next_info.le_next;
						uint32 va = ROUNDDOWN(current->virtual_address,PAGE_SIZE);
						pt_set_page_permissions(faulted_env->env_page_directory, va, 0, PERM_PRESENT);

						LIST_REMOVE(&(faulted_env->ActiveList), current);

						current = NEXT;
					}
				}
			}

			//	[4] Add the faulted page to the Active WS
			struct WorkingSetElement* newWSElem = env_page_ws_list_create_element(faulted_env,fault_va);
			if(newWSElem!=NULL)
				LIST_INSERT_TAIL(&(faulted_env->ActiveList),newWSElem);

			//	[5] Add faulted page to the end of the reference stream list
			struct PageRefElement* newRef = env_page_pageRef_list_create_element(faulted_env,fault_va);
			if(newRef!=NULL)
				LIST_INSERT_TAIL(&(faulted_env->referenceStreamList),newRef);

		}
	else
	{
		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
		if(wsSize < (faulted_env->page_WS_max_size))
		{
		    //TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #3 placement
		    //Your code is here
		    //Comment the following line
		    //panic("page_fault_handler().PLACEMENT is not implemented yet...!!");

		    struct FrameInfo *ptr_frame = NULL;
		    int ret = allocate_frame(&ptr_frame);
		    if (ret != 0) {
		        panic("page_fault_handler: no free frames!");
		    }

		    ret = map_frame(faulted_env->env_page_directory, ptr_frame,
		                    ROUNDDOWN(fault_va, PAGE_SIZE),
		                    PERM_USER | PERM_WRITEABLE | PERM_PRESENT);
		    if (ret != 0) {
		        panic("page_fault_handler: mapping failed!");
		    }

		    ret = pf_read_env_page(faulted_env, (void*) ROUNDDOWN(fault_va, PAGE_SIZE));

		    if (ret == E_PAGE_NOT_EXIST_IN_PF) {
		        if ((fault_va >= USTACKBOTTOM && fault_va < USTACKTOP) || (fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX)) {
		            //nothing to do, stack/heap created on demand
		        } else {
		            unmap_frame(faulted_env->env_page_directory, ROUNDDOWN(fault_va, PAGE_SIZE));
		            env_exit();
		            return;
		        }
		    }

		    struct WorkingSetElement *new_elem = env_page_ws_list_create_element(faulted_env, ROUNDDOWN(fault_va, PAGE_SIZE));
		    if (faulted_env->page_last_WS_element == NULL){
				LIST_INSERT_TAIL(&(faulted_env->page_WS_list),new_elem);
				if(LIST_SIZE(&(faulted_env->page_WS_list)) == faulted_env->page_WS_max_size)
					faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
				else
					faulted_env->page_last_WS_element = NULL;
		    }
		    else
		        LIST_INSERT_BEFORE(&(faulted_env->page_WS_list),faulted_env->page_last_WS_element,new_elem);
		}

		else
		{
			if (isPageReplacmentAlgorithmCLOCK()) {

				struct WorkingSetElement* ptr;
				struct WorkingSetElement* victim = NULL;

				int max_size = faulted_env->page_WS_max_size;
				int size = LIST_SIZE(&(faulted_env->page_WS_list));

				if(max_size == size) {
					while(1) {
						ptr = faulted_env->page_last_WS_element;
						if (!ptr) {
							break;
						}
						uint32 va = (uint32)ptr->virtual_address;
						int perms = pt_get_page_permissions(faulted_env->env_page_directory, va);

						//check the used bit is 0
						//if 0 -> victim
						//if 1 -> set it by 0
						if (!(perms & PERM_USED)) {
							victim = ptr;
							break;
						} else {
							pt_set_page_permissions(faulted_env->env_page_directory, va, 0, PERM_USED);
							faulted_env->page_last_WS_element = LIST_NEXT(faulted_env->page_last_WS_element);
							if (faulted_env->page_last_WS_element == NULL)
								faulted_env->page_last_WS_element = LIST_FIRST(&faulted_env->page_WS_list);
						}
					}

					//replace the victim with the new_elem
					if (!victim) {
						cprintf("DEBUG: WARNING - no victim found (unexpected)\n");
					} else {
						uint32 victim_va = victim->virtual_address;

						struct WorkingSetElement *victim_next = LIST_NEXT(victim);

						//if it need to be updated before removing
						uint32 perms = pt_get_page_permissions(faulted_env->env_page_directory, victim_va);
						if (perms & PERM_MODIFIED) {
							uint32 *pt = NULL;
							struct FrameInfo *victim_frame = get_frame_info(faulted_env->env_page_directory, victim_va, &pt);
							if (victim_frame) {
								pf_update_env_page(faulted_env, victim_va, victim_frame);
							}
						}

						//remove & unmap the victim
						env_page_ws_invalidate(faulted_env, victim_va);

						//create & insert the new_elem
						struct FrameInfo* new_frame = NULL;
						allocate_frame(&new_frame);

						uint32 permissions = PERM_USER | PERM_WRITEABLE | PERM_PRESENT | PERM_USED;
						if(fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX)
							permissions |= PERM_UHPAGE;

						map_frame(faulted_env->env_page_directory, new_frame, ROUNDDOWN(fault_va, PAGE_SIZE), permissions);

						int ret = pf_read_env_page(faulted_env, (void*)ROUNDDOWN(fault_va, PAGE_SIZE));
						if (ret == E_PAGE_NOT_EXIST_IN_PF) {
							if ((fault_va >= USTACKBOTTOM && fault_va < USTACKTOP) ||
								(fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX)) {
							} else {
								unmap_frame(faulted_env->env_page_directory, ROUNDDOWN(fault_va, PAGE_SIZE));
								env_exit();
								return;
							}
						}

						struct WorkingSetElement *new_elem = env_page_ws_list_create_element(faulted_env, ROUNDDOWN(fault_va, PAGE_SIZE));
						if(!new_elem){
							panic("CLOCK: env_page_ws_list_create_element failed");
						}

						if (victim_next != NULL) {
							LIST_INSERT_BEFORE(&(faulted_env->page_WS_list), victim_next, new_elem);
						} else {
							LIST_INSERT_TAIL(&(faulted_env->page_WS_list), new_elem);
						}

						faulted_env->page_last_WS_element = victim_next;
						if (faulted_env->page_last_WS_element == NULL) {
							faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
						}

					}
				}
			}
			else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX)) {
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #2 LRU Aging Replacement
				//Your code is here
				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");

				struct WorkingSetElement* ptr;
				struct WorkingSetElement* victim = NULL;
				uint32 min_time = 0xFFFFFFFF;

				//find victim with minimum timestamp
				//make sure any real timestamp will be smaller
				LIST_FOREACH(ptr, &(faulted_env->page_WS_list))
				{
					if(ptr->time_stamp < min_time) {
						min_time = ptr->time_stamp;
						victim = ptr;
					}
				}

				uint32 victim_va = victim->virtual_address;
				uint32 victim_permissions = pt_get_page_permissions(faulted_env->env_page_directory, victim_va);

				//write back if modified
				if(victim_permissions & PERM_MODIFIED) {
					uint32 *ptr_page_table = NULL;
					struct FrameInfo* victim_frame = get_frame_info(faulted_env->env_page_directory, victim_va, &ptr_page_table);
					pf_update_env_page(faulted_env, victim_va, victim_frame);
				}

				env_page_ws_invalidate(faulted_env,victim_va);


				//allocate and map new frame
				struct FrameInfo* new_frame = NULL;
				allocate_frame(&new_frame);

				//determine permissions based on address
				uint32 permissions = PERM_USER | PERM_WRITEABLE | PERM_PRESENT;
				if(fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX)
				{
					permissions |= PERM_UHPAGE;
				}

				map_frame(faulted_env->env_page_directory, new_frame,ROUNDDOWN(fault_va, PAGE_SIZE), permissions);

				//read page
				int ret = pf_read_env_page(faulted_env, (void*)ROUNDDOWN(fault_va, PAGE_SIZE));
				if(ret == E_PAGE_NOT_EXIST_IN_PF)
				{
					if((fault_va >= USTACKBOTTOM && fault_va < USTACKTOP) ||
					   (fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX))
					{
						//Stack/Heap --> already mapped
					}
				    else {
				        // Invalid page
				        unmap_frame(faulted_env->env_page_directory, ROUNDDOWN(fault_va, PAGE_SIZE));
				        env_exit();
				        return;
				    }
				}
				//creating new ws elememt
				struct WorkingSetElement *new_elem = env_page_ws_list_create_element(faulted_env,ROUNDDOWN(fault_va, PAGE_SIZE));

				//timestamp updated by update_WS_time_stamps()
				LIST_INSERT_TAIL(&(faulted_env->page_WS_list),new_elem);

			}
			else if (isPageReplacmentAlgorithmModifiedCLOCK()) {
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II
				//get max ws and current size
				int max_size = faulted_env->page_WS_max_size;
				int size = LIST_SIZE(&faulted_env->page_WS_list);

				//only perform replacement if ws full
				if(max_size == size){
					struct WorkingSetElement* ptr;
					struct WorkingSetElement* victim = NULL;

					while(1){
						// FIRST PASS: used=0 && modified=0
						//the best
						//not recently used && not dirty
						for(int i = 0; i < max_size; i++) {
							ptr = faulted_env->page_last_WS_element;
							uint32 va = (uint32)ptr->virtual_address;
							int perms = pt_get_page_permissions(faulted_env->env_page_directory, va);


							if(!(perms & PERM_USED) && !(perms & PERM_MODIFIED)) {
								victim = ptr;

								faulted_env->page_last_WS_element = LIST_NEXT(faulted_env->page_last_WS_element);
								if(faulted_env->page_last_WS_element == NULL)
									faulted_env->page_last_WS_element = LIST_FIRST(&faulted_env->page_WS_list);

								break;
							}
							//give page second chance --> move clk hand forward
							faulted_env->page_last_WS_element = LIST_NEXT(faulted_env->page_last_WS_element);

							//wrap around to beginning if at end
							if(faulted_env->page_last_WS_element == NULL)
								faulted_env->page_last_WS_element = LIST_FIRST(&faulted_env->page_WS_list);
						}

						if (victim != NULL) break;

						// SECOND PASS: used=0 (ignore modified)
						//accept any page that hasn't been recently used
						//even if dirty
						for(int i = 0; i < max_size; i++) {
							ptr = faulted_env->page_last_WS_element;
							uint32 va = (uint32)ptr->virtual_address;
							int perms = pt_get_page_permissions(faulted_env->env_page_directory, va);


							if(!(perms & PERM_USED)) {
								victim = ptr;

								faulted_env->page_last_WS_element = LIST_NEXT(faulted_env->page_last_WS_element);
								if(faulted_env->page_last_WS_element == NULL)
									faulted_env->page_last_WS_element = LIST_FIRST(&faulted_env->page_WS_list);
								break;
							} else {
								//page used --> clear used bit (give it a chance)
								pt_set_page_permissions(faulted_env->env_page_directory, va, 0, PERM_USED);
							}
							//move clock hand forward
							faulted_env->page_last_WS_element = LIST_NEXT(faulted_env->page_last_WS_element);
							if(faulted_env->page_last_WS_element == NULL)
								faulted_env->page_last_WS_element = LIST_FIRST(&faulted_env->page_WS_list);
						}

						if(victim != NULL) break;
						//if no victim found after 2 passes, loop continues (shouldn't happen)
					}

					uint32 victim_va = victim->virtual_address;

					int victim_perms = pt_get_page_permissions(faulted_env->env_page_directory, victim_va);

					//if victim page was modified --> save it to disk
					if(victim_perms & PERM_MODIFIED) {
						uint32 *ptr_pt = NULL;
						struct FrameInfo* victim_frame = get_frame_info(faulted_env->env_page_directory, victim_va, &ptr_pt);

						//verify page table entry exists before writing back
						if(ptr_pt != NULL){
							//write el modified page content to page file
							int ret = pf_update_env_page(faulted_env, victim_va, victim_frame);
						}
					}
					//remove victim from ws && clear mapping
					env_page_ws_invalidate(faulted_env, victim_va);

					struct FrameInfo* new_frame = NULL;
					allocate_frame(&new_frame);

					//prepare permission bits for new page
					uint32 permissions = PERM_USER | PERM_WRITEABLE | PERM_PRESENT;
					if(fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX)
						permissions |= PERM_UHPAGE;

					map_frame(faulted_env->env_page_directory, new_frame,ROUNDDOWN(fault_va, PAGE_SIZE), permissions);

					//read page
					//load page content from disk
					int ret = pf_read_env_page(faulted_env, (void*)ROUNDDOWN(fault_va, PAGE_SIZE));

					// handle pages that don't exist in page file
					if(ret == E_PAGE_NOT_EXIST_IN_PF)
					{
						if((fault_va >= USTACKBOTTOM && fault_va < USTACKTOP) ||
						   (fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX))
						{
							// Stack/Heap - already mapped
						}
						else
						{
							// invalid page
							// terminate process
							unmap_frame(faulted_env->env_page_directory, ROUNDDOWN(fault_va, PAGE_SIZE));
							env_exit();
							return;
						}
					}
					//create ws element
					struct WorkingSetElement *new_elem = env_page_ws_list_create_element(faulted_env, ROUNDDOWN(fault_va, PAGE_SIZE));
					//maintain circular structure
					LIST_INSERT_BEFORE(&(faulted_env->page_WS_list),faulted_env->page_last_WS_element,new_elem);
				}
			}

		}
	}
#endif
}


void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va)
{
	panic("this function is not required...!!");

}
