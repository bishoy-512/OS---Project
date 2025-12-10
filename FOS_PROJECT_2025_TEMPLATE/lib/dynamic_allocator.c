/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//==================================
// [1] GET PAGE VA:
//==================================
__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo)
{
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
bool is_initialized = 0;
void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #1 initialize_dynamic_allocator
	//===========================================================================
	//Your code is here

	// 1 - Put DA Limits
	dynAllocStart = daStart;
	dynAllocEnd = daEnd;

	// 2 - initialize (Array of Page Info && Array of free Pages)
	int Num_of_Pages = (dynAllocEnd - dynAllocStart) / PAGE_SIZE;
	LIST_INIT(&freePagesList);

	for (int i = 0; i < Num_of_Pages; i++) {
		struct PageInfoElement *page = &pageBlockInfoArr[i];
		page->block_size = 0;
		page->num_of_free_blocks = 0;
		LIST_INSERT_TAIL(&freePagesList, page);
	}

	// 3 - free block lists
	for (int i = 0; i < LOG2_MAX_SIZE - LOG2_MIN_SIZE + 1; i++)
		LIST_INIT(&freeBlockLists[i]);

	//Comment the following line
	//panic("initialize_dynamic_allocator() Not implemented yet");
}

//===========================
// Return Page Index With VA: Hand_Made bishoy & beshoy
//===========================
/* this func get the va of any page and return the page index in pageBlockInfoArr[] */
/* by subtracting the va from the dynamic allocator start then div by Page_size it should return the index */
int get_page_with_va(void *va)
{
	int index = ((uint32)va - dynAllocStart) / PAGE_SIZE;
	return index;
}

//===========================
// [2] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #2 get_block_size
	//Your code is here

	/* now, i need to get the size of these block but this block in which page ? */
	/* so, i get the page index with prev func i do then get the page size */

	int index = get_page_with_va(va);
	struct PageInfoElement *page = &pageBlockInfoArr[index];
	return page->block_size;

	//Comment the following line
	//panic("get_block_size() Not implemented yet");
}

//===========================
// [3] ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #3 alloc_block
	//Your code is here

	// Check if size equal zero
	if(size == 0)
		return NULL;
	// get nearest pow of 2
	int index = 0;
	int size2 = 0;
	for (int i = 3; i < 12; i++) {
		if ((1 << i) >= size) {
			index = i - 3;
			size2 = (1 << i);
			break;
		}
	}
	//Case 1: if a free block exists
	struct BlockElement *element = NULL;
	int siz = LIST_SIZE(&freeBlockLists[index]);

	if (siz != 0) {
	    element = LIST_FIRST(&freeBlockLists[index]);
	    LIST_REMOVE(&freeBlockLists[index], element);
	    int idx = get_page_with_va(element);
	    pageBlockInfoArr[idx].num_of_free_blocks--;
	    return (void*)element;
	}

	//CASE2: else, if a free page exists
	siz = LIST_SIZE(&freePagesList);
	if(siz != 0){
		// Get Free Page And Remove It
		struct PageInfoElement *page = LIST_FIRST(&freePagesList);
	    LIST_REMOVE(&freePagesList, page);
	    page->block_size = size2;
	    page->num_of_free_blocks=(PAGE_SIZE/size2);
	    uint8 *va = (uint8 *)to_page_va(page);
	    get_page(va);
	    // Get The begin of address of the free page
	    for(int i = 0;i<page->num_of_free_blocks;i++){
	    	element = (struct BlockElement*)va;
	    	LIST_INSERT_TAIL(&freeBlockLists[index] , element);
	    	va += size2;
	    }
	    element = LIST_FIRST(&freeBlockLists[index]);
		LIST_REMOVE(&freeBlockLists[index], element);
		page->num_of_free_blocks--;
		return (void*)element;
	}

	// CASE3: else, allocate block from the next list
	else{
		for (int i = index + 1;i < 12;i++){
			siz = LIST_SIZE(&freeBlockLists[i]);
			if(siz !=0){
				element = LIST_FIRST(&freeBlockLists[i]);
				LIST_REMOVE(&freeBlockLists[i], element);
				int idx = get_page_with_va(element);
				pageBlockInfoArr[idx].num_of_free_blocks--;
				return (void*)element;
			}
		}
	}

	// Case 4 :
	//TODO: [PROJECT'25.BONUS#1] DYNAMIC ALLOCATOR - block if no free block
	// by (bishoy && beshoy)     :)
	while(1){
		int siz = LIST_SIZE(&freeBlockLists[index]);
		if (siz!=0){
			element = LIST_FIRST(&freeBlockLists[index]);
			LIST_REMOVE(&freeBlockLists[index], element);
			int idx = get_page_with_va(element);
			pageBlockInfoArr[idx].num_of_free_blocks--;
			return (void*)element;
		}
	}

//	//Comment the following line
//	panic("alloc_block() Not implemented yet");

}

//===========================
// [4] FREE BLOCK:
//===========================
void free_block(void *va)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert((uint32)va >= dynAllocStart && (uint32)va < dynAllocEnd);
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #4 free_block
	//Your code is here
	// get the page that block belong to
	int idx = get_page_with_va(va);
	struct PageInfoElement *page = &pageBlockInfoArr[idx];
	int size_of_block = get_block_size(va);
	int index = 0;
	for (int i = 3; i < 12;i++){
		if((1 << i) == size_of_block){
			index = i-3;
			break;
		}
	}
	// insert free block into free blocks list
	struct BlockElement *element = (struct BlockElement*)va;
	LIST_INSERT_TAIL(&freeBlockLists[index] , element);
	page->num_of_free_blocks++;
	// check if page come free
	if(page->num_of_free_blocks == (PAGE_SIZE/size_of_block)){

		/* if i found that page is become free so i need to init it and add it to free page list*/
		struct BlockElement *temp;
		LIST_FOREACH(temp, &freeBlockLists[index]) {
		    uint32 block_va = (uint32)temp;
		    int temp1 = get_page_with_va((void*)block_va);
		    if(temp1 == idx)
		    	LIST_REMOVE(&freeBlockLists[index] , temp);
		}

		page->block_size = 0;
		page->num_of_free_blocks = 0;

	    uint8 *va_page = (uint8 *)to_page_va(page);
		return_page(va_page);
		LIST_INSERT_TAIL(&freePagesList , page);

	}
	//Comment the following line
	//panic("free_block() Not implemented yet");
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void *va, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	panic("realloc_block() Not implemented yet");
}
