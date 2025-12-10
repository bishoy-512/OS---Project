#include <inc/lib.h>
 //==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

static uint32 all_va[1000];
static uint32 all_va_size[1000];
static int elements_count = 0;

//==============================================
// [1] INITIALIZE USER HEAP:
//==============================================
int __firstTimeFlag = 1;
void uheap_init()
{
	if(__firstTimeFlag)
	{
		initialize_dynamic_allocator(USER_HEAP_START, USER_HEAP_START + DYN_ALLOC_MAX_SIZE);
		uheapPlaceStrategy = sys_get_uheap_strategy();
		uheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		uheapPageAllocBreak = uheapPageAllocStart;
		__firstTimeFlag = 0;
	}
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = __sys_allocate_page(ROUNDDOWN(va, PAGE_SIZE), PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
	if (ret < 0)
		panic("get_page() in user: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	int ret = __sys_unmap_frame(ROUNDDOWN((uint32)va, PAGE_SIZE));
	if (ret < 0)
		panic("return_page() in user: failed to return a page to the kernel");
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//=================================
// [1] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size){
	uheap_init();
	if (size == 0) return NULL ;
	uint32 rounded_size = ROUNDUP(size, PAGE_SIZE);

	//Check if size <= 2K (Block Allocate)
	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE){
		uint32 *alloc_start = alloc_block(size);
		return alloc_start;
	}
	//Else If size > 2k (Page Allocate)
	/* if elements_count = 0 (the beginning of page allocator and arrays is empty) */
	else if (elements_count == 0){
		/* round up the size then update the break */
		uint32 rounded_size = ROUNDUP(size, PAGE_SIZE);
		uheapPageAllocBreak += rounded_size;
		/* then call the sys_allocate_user_mem with rounded size
		 * to switch from user to kernel to allocate*/
		sys_allocate_user_mem(uheapPageAllocStart , rounded_size);
		all_va[elements_count] = uheapPageAllocStart;
		all_va_size[elements_count] = rounded_size;
		elements_count++;
		return (void *) uheapPageAllocStart;
	}
	/* if arrays is not empty */
	else{
		uint32 rounded_size = ROUNDUP(size, PAGE_SIZE);
		uint32 temp_size = 0; // Size of the space between two allocated pages
		uint32 worst_size = 0; // The biggest size i found
		uint32 worst_va = 0; // The biggest size VA
		bool fir_alloc = 0; // Boolean if i found the first allocated page
		uint32 fir_size = 0; // Size of first VA allocated
		uint32 fir_va = 0; // First Allocated Pages VA
		int first_index = -1; // First Allocated Pages Index In array
		bool sec_alloc = 0; // Same As First
		uint32 sec_size = 0; // Same As First
		uint32 sec_va = 0; // Same As First
		int second_index = -1; // Same As First
		int insert_index = -1; // The index that i need to insert new VA & Size At
		for (int i = 0;i<elements_count;i++){
			/* Searching for First Allocated Element In Array */
			if(all_va[i] != 0 && !fir_alloc){
				fir_size = all_va_size[i];
				fir_va = all_va[i];
				first_index = i;
				fir_alloc = 1;
			}
			/* Searching for Second Allocated Element In Array If I found The First One*/
			else if(all_va[i] != 0 && fir_alloc){
				sec_size = all_va_size[i];
				sec_va = all_va[i];
				sec_alloc = 1;
				second_index = i;
			}
			/* calculate The size between the first and second allocated */
			if(fir_alloc && !sec_alloc && all_va[i] == 0)
				temp_size += all_va_size[i];
			/* If i found the first and second allocated */
			else if (fir_alloc && sec_alloc){
				/* Check if the size is exact size i need
				 * then allocate it and update the arrays */
				if(temp_size == rounded_size) {
					sys_allocate_user_mem(fir_va + fir_size , rounded_size);
					all_va[i-1] = fir_va + fir_size;
					all_va_size[i-1] = rounded_size;
					return (void *) fir_va + fir_size;
				}
				/* if the size i found > the size i need (worst size) i save it till end */
				else if (temp_size > rounded_size && temp_size > worst_size){
					worst_size = temp_size;
					worst_va = fir_va + fir_size;
					insert_index = first_index+1;
				}
				/* then i do temp_size = 0 , and make the first VA & Size = Second VA & Size
				 * And Search Again for Second
				 * */
				temp_size = 0;
				fir_va = sec_va;
				fir_size = sec_size;
				first_index = second_index;
				sec_alloc = 0;

			}
		}
		/* After The loop if don't get the exact size then i check if i have the worst */
		if(worst_size != 0){
			sys_allocate_user_mem(worst_va , rounded_size);
			elements_count++;
			/* This loop for split the array index to get the size i need and the remaining for the next index */
			for(int i = elements_count;i>insert_index;i--){
				all_va[i] = all_va[i-1];
				all_va_size[i] = all_va_size[i-1];
			}
			all_va[insert_index] = worst_va;
			all_va_size[insert_index] = rounded_size;
			all_va[insert_index+1] = 0;
			all_va_size[insert_index+1] = worst_size - rounded_size;

			return (void *) worst_va;
		}
		/* If i don't get worst or exact i check if i have enough space in user_heap
		 * Then update the break and allocate the size i need
		 * */
		else if (uheapPageAllocBreak <= USER_HEAP_MAX - rounded_size){
			uint32 va = uheapPageAllocBreak;
			sys_allocate_user_mem(va , rounded_size);
			all_va[elements_count] = va;
			all_va_size[elements_count] = rounded_size;
			elements_count++;
			uheapPageAllocBreak += rounded_size;
			return (void *) va;
		}
	}
	return NULL;
}


//=================================
// [2] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{

    uint32 va = (uint32)virtual_address;
    bool f = 1;
    // if VA Between the dynamic limits => Block
    if (va >= dynAllocStart && va < dynAllocEnd){
        free_block(virtual_address);
        f = 0;
    }
    else if (va >= uheapPageAllocStart && va < uheapPageAllocBreak){
    	/*loop for get the va that i need to remove it*/
        for(int i = 0; i < elements_count; i++) {
            if(all_va[i] == va) {

                uint32 size = all_va_size[i];
                sys_free_user_mem(va, size);
                /* make the new VA = 0 ( It represents Free VA ) */
                all_va[i] = 0;
                /* If i free the VA from the last of the user_stack
                 * Then must update the Break
                 * And checking if i can update the break more
                 * */
                if(uheapPageAllocBreak == va + ROUNDUP(size, PAGE_SIZE)) {
                    uheapPageAllocBreak = va;
                    while(uheapPageAllocBreak > uheapPageAllocStart) {
                    	/* check if the page below the break is free or not
                    	 * if free => i update the break
                    	 * else exit from loop
                    	 *  */
                        uint32 eee = ROUNDDOWN(uheapPageAllocBreak - PAGE_SIZE, PAGE_SIZE);
                        bool is_taken = 0;
                        int j = 0;
                        while(j < elements_count) {
                            if(all_va[j] != 0) {
                                uint32 start = all_va[j];
                                uint32 end = all_va[j] + ROUNDUP(all_va_size[j], PAGE_SIZE);
                                if(eee >= start && eee < end) {
                                    is_taken = 1;
                                    break;
                                }
                            }
                            j++;
                        }
                        if(!is_taken)
                            uheapPageAllocBreak = eee;
                        else
                            break;
                    }
                }

//                /* This loop for update the elements count after free from last */
//                for(int i = elements_count;i >= 0;i--){
//                	if(all_va[i] != 0)
//                		break;
//                	elements_count -= 1;
//                }

                f = 0;
                break;
            }
        }
        /* This loop to merge the free sizes */
        for(int i = 0;i < elements_count;i++) {
        	/* if i found free VA and the next index is free too so i merge it
        	 * 1) sum there sizes
        	 * 2) put them in 1 index together
        	 * */
        	if(all_va[i + 1] == 0 && all_va[i] == 0){
				all_va_size[i] += all_va_size[i + 1];
				for(int j = i + 1; j < elements_count - 1; j++) {
					all_va[j] = all_va[j + 1];
					all_va_size[j] = all_va_size[j + 1];
				}
				elements_count--;
				i--;
        	}
        }

    }

    if (f) panic("INVALID VA");
}


//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	//==============================================================
	uheap_init();
	if (size == 0) return NULL ;
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
	//Your code is here
	uint32 rounded_size = ROUNDUP(size, PAGE_SIZE);
	/* if elements_count = 0 (the beginning of page allocator and arrays is empty) */
	if (elements_count == 0) {
		/* round up the size then update the break */
	    uheapPageAllocBreak += rounded_size;
	    int id = sys_create_shared_object(sharedVarName , size , isWritable , (void *)uheapPageAllocStart);
	    if(id != E_SHARED_MEM_EXISTS && id != E_NO_SHARE){
	        all_va[elements_count] = uheapPageAllocStart;
	        all_va_size[elements_count] = rounded_size;
	        elements_count++;
	        return (void *)uheapPageAllocStart;
	    }
	    else
	        return NULL;
	}
	else {
		uint32 temp_size = 0; // Size of the space between two allocated pages
		uint32 worst_size = 0; // The biggest size i found
		uint32 worst_va = 0; // The biggest size VA
		bool fir_alloc = 0; // Boolean if i found the first allocated page
		uint32 fir_size = 0; // Size of first VA allocated
		uint32 fir_va = 0; // First Allocated Pages VA
		int first_index = -1; // First Allocated Pages Index In array
		bool sec_alloc = 0; // Same As First
		uint32 sec_size = 0; // Same As First
		uint32 sec_va = 0; // Same As First
		int second_index = -1; // Same As First
		int insert_index = -1; // The index that i need to insert new VA & Size At
	    for (int i = 0;i<elements_count;i++){
	        if(all_va[i] != 0 && !fir_alloc){
	            fir_size = all_va_size[i];
	            fir_va = all_va[i];
	            first_index = i;
	            fir_alloc = 1;
	        }
	        else if(all_va[i] != 0 && fir_alloc){
	            sec_size = all_va_size[i];
	            sec_va = all_va[i];
	            sec_alloc = 1;
	            second_index = i;
	        }
	        if(fir_alloc && !sec_alloc && all_va[i] == 0)
	            temp_size += all_va_size[i];
	        else if (fir_alloc && sec_alloc){
	            if(temp_size == rounded_size) {
	                int id = sys_create_shared_object(sharedVarName , size , isWritable , (void *)(fir_va+fir_size));
	                if(id != E_SHARED_MEM_EXISTS && id != E_NO_SHARE){
	                    all_va[i-1] = fir_va + fir_size;
	                    all_va_size[i-1] = rounded_size;
	                    return (void *)(fir_va+fir_size);
	                }
	                else
	                    return NULL;
	            }
	            else if (temp_size > rounded_size && temp_size > worst_size){
	                worst_size = temp_size;
	                worst_va = fir_va + fir_size;
	                insert_index = first_index+1;
	            }
	            temp_size = 0;
	            fir_va = sec_va;
	            fir_size = sec_size;
	            first_index = second_index;
	            sec_alloc = 0;
	        }
	    }

	    if(worst_size != 0){
	        int id = sys_create_shared_object(sharedVarName , size , isWritable , (void *)worst_va);
	        if(id != E_SHARED_MEM_EXISTS && id != E_NO_SHARE){
	            elements_count++;
	            for(int i = elements_count;i>insert_index;i--){
	                all_va[i] = all_va[i-1];
	                all_va_size[i] = all_va_size[i-1];
	            }
	            all_va[insert_index] = worst_va;
	            all_va_size[insert_index] = rounded_size;
	            all_va[insert_index+1] = 0;
	            all_va_size[insert_index+1] = worst_size - rounded_size;
	            return (void *)worst_va;
	        }
	        else
	            return NULL;
	    }
	    else if (uheapPageAllocBreak <= USER_HEAP_MAX - rounded_size){
	        uint32 va = uheapPageAllocBreak;
	        int id = sys_create_shared_object(sharedVarName , size , isWritable , (void *)va);
	        if(id != E_SHARED_MEM_EXISTS && id != E_NO_SHARE){
	            all_va[elements_count] = va;
	            all_va_size[elements_count] = rounded_size;
	            elements_count++;
	            uheapPageAllocBreak += rounded_size;
	            return (void*)va;
	        }
	        else
	            return NULL;
	    }
	}


	return NULL;
}
//========================================
// [4] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
    //==============================================================
    //DON'T CHANGE THIS CODE========================================
    //==============================================================
	uheap_init();

    int size = sys_size_of_shared_object(ownerEnvID, sharedVarName);
    if (size == E_SHARED_MEM_NOT_EXISTS || size == 0)
        return NULL;

    uint32 rounded_size = ROUNDUP(size, PAGE_SIZE);

    if (elements_count == 0) {
        uheapPageAllocBreak += rounded_size;
        int id = sys_get_shared_object(ownerEnvID, sharedVarName, (void *)uheapPageAllocStart);
        if(id != E_SHARED_MEM_NOT_EXISTS) {
            all_va[elements_count] = uheapPageAllocStart;
            all_va_size[elements_count] = rounded_size;
            elements_count++;
            return (void *)uheapPageAllocStart;
        }
        else
            return NULL;
    }

    else {
        uint32 temp_size = 0;
        uint32 worst_size = 0;
        uint32 worst_va = 0;
        bool fir_alloc = 0;
        uint32 fir_size = 0;
        uint32 fir_va = 0;
        int first_index = -1;
        bool sec_alloc = 0;
        uint32 sec_size = 0;
        uint32 sec_va = 0;
        int second_index = -1;
        int insert_index = -1;

        for (int i = 0; i < elements_count; i++) {
            if(all_va[i] != 0 && !fir_alloc) {
                fir_size = all_va_size[i];
                fir_va = all_va[i];
                first_index = i;
                fir_alloc = 1;
            }
            else if(all_va[i] != 0 && fir_alloc) {
                sec_size = all_va_size[i];
                sec_va = all_va[i];
                sec_alloc = 1;
                second_index = i;
            }

            if(fir_alloc && !sec_alloc && all_va[i] == 0)
                temp_size += all_va_size[i];
            else if (fir_alloc && sec_alloc) {
                if(temp_size == rounded_size) {
                    int id = sys_get_shared_object(ownerEnvID, sharedVarName, (void *)(fir_va + fir_size));
                    if(id != E_SHARED_MEM_NOT_EXISTS) {
                        all_va[i-1] = fir_va + fir_size;
                        all_va_size[i-1] = rounded_size;
                        return (void *)(fir_va + fir_size);
                    }
                    else
                        return NULL;
                }
                else if (temp_size > rounded_size && temp_size > worst_size) {
                    worst_size = temp_size;
                    worst_va = fir_va + fir_size;
                    insert_index = first_index + 1;
                }
                temp_size = 0;
                fir_va = sec_va;
                fir_size = sec_size;
                first_index = second_index;
                sec_alloc = 0;
            }
        }

        if(worst_size != 0) {
            int id = sys_get_shared_object(ownerEnvID, sharedVarName, (void *)worst_va);
            if(id != E_SHARED_MEM_NOT_EXISTS) {
                elements_count++;
                for(int i = elements_count; i > insert_index; i--) {
                    all_va[i] = all_va[i-1];
                    all_va_size[i] = all_va_size[i-1];
                }
                all_va[insert_index] = worst_va;
                all_va_size[insert_index] = rounded_size;
                all_va[insert_index+1] = 0;
                all_va_size[insert_index+1] = worst_size - rounded_size;
                return (void *)worst_va;
            }
            else
                return NULL;
        }
        else if (uheapPageAllocBreak <= USER_HEAP_MAX - rounded_size) {
            uint32 va = uheapPageAllocBreak;
            int id = sys_get_shared_object(ownerEnvID, sharedVarName, (void *)va);
            if(id != E_SHARED_MEM_NOT_EXISTS) {
                all_va[elements_count] = va;
                all_va_size[elements_count] = rounded_size;
                elements_count++;
                uheapPageAllocBreak += rounded_size;
                return (void *)va;
            }
            else
                return NULL;
        }
    }

    return NULL;
}



//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	panic("realloc() is not implemented yet...!!");
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
