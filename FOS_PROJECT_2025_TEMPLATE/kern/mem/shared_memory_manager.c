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
#if USE_KHEAP
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #1 alloc_share
	//Your code is here

	// first allocate the shared obj if can't allocate => null
    struct Share* bosh = kmalloc(sizeof(struct Share));
    if (bosh == NULL)
    	return NULL;

    // init it's members and mask the id
	bosh->size = (int)size;
	bosh->ownerID = ownerID;
	bosh->isWritable = isWritable;
	bosh->references = 1;
	bosh->ID = (uint32)bosh & 0x7FFFFFFF;
	for (int i = 0;i<strlen(shareName);i++)
		bosh->name[i] = shareName[i];

	// Calculate the number of needed frames to allocate it too
	int num_of_frames = ROUNDUP(size,PAGE_SIZE) / PAGE_SIZE;
	bosh->framesStorage = (struct FrameInfo**) kmalloc(sizeof(struct FrameInfo*) * num_of_frames);

	// if i can't allocate it undo any allocated sizes
	if (bosh->framesStorage == NULL) {
	    kfree(bosh);
	    return NULL;
	}

	for (int i = 0;i<num_of_frames;i++)
		bosh->framesStorage[i] = NULL;

	return bosh;
#else
	panic("error");
#endif
}

//=========================
// [4] Create Share Object:
//=========================
int create_shared_object(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
#if USE_KHEAP
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #3 create_shared_object
	//Your code is here
	//Comment the following line
	//panic("create_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment

	// Check if shared obj is exist
	if (find_share(ownerID,shareName) != NULL)
		return E_SHARED_MEM_EXISTS;

	// if not exist so i allocate it
	struct Share* bosh = alloc_share(ownerID,shareName,size,isWritable);
	if (bosh == NULL)
		return E_NO_SHARE;

	// check if i have enough free frames to map it
	int num_of_free_frames = LIST_SIZE(&MemFrameLists.free_frame_list);
	int num_of_frames = ROUNDUP(size,PAGE_SIZE) / PAGE_SIZE;
	if(num_of_free_frames < num_of_frames)
		return E_NO_SHARE;

	// lock the shares list while inserting the new element
	acquire_kspinlock(&AllShares.shareslock);
	LIST_INSERT_TAIL(&AllShares.shares_list, bosh);
	release_kspinlock(&AllShares.shareslock);


	// allocate the requirement frames then map it
	struct FrameInfo *f = NULL;
	uint32 va = (uint32)virtual_address;
	for(int i = 0;i<num_of_frames;i++){
		allocate_frame(&f);
		bosh->framesStorage[i] = f;
		map_frame(myenv->env_page_directory , f , va , PERM_WRITEABLE | PERM_USER | PERM_PRESENT | PERM_UHPAGE );
		va += PAGE_SIZE;
	}



	return bosh->ID;
	// This function should create the shared object at the given virtual address with the given size
	// and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_EXISTS if the shared object already exists
	//	c) E_NO_SHARE if failed to create a shared object
#else
	panic("error");
#endif
}


//======================
// [5] Get Share Object:
//======================
int get_shared_object(int32 ownerID, char* shareName, void* virtual_address)
{
#if USE_KHEAP
    struct Env* myenv = get_cpu_proc(); //The calling environment
    struct Share * bosh = find_share(ownerID, shareName);
    // check if the shared obj is exist or not
    if (bosh == NULL)
        return E_SHARED_MEM_NOT_EXISTS;

    bosh->references++;

    // check if i have permission to write in this shared object or not
    int perms = 0;
    if(bosh->isWritable == 1)
        perms = PERM_WRITEABLE | PERM_USER | PERM_PRESENT | PERM_UHPAGE;
    else
        perms = PERM_USER | PERM_PRESENT;

    // make my object map to same frames as shared one
    uint32 start = (uint32)virtual_address;
    uint32 end = start + ROUNDUP(bosh->size , PAGE_SIZE);
    int idx = 0;
    for(uint32 i = start; i < end; i += PAGE_SIZE ){
        map_frame(myenv->env_page_directory, bosh->framesStorage[idx], i, perms);
        idx++;
    }

    return bosh->ID;
#else
	panic("error");
#endif
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
#if USE_KHEAP
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - free_share
	//Your code is here
	//Comment the following line
	panic("free_share() is not implemented yet...!!");
#else
	panic("error");
#endif
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
