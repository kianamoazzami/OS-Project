/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2021 Yiqing Huang
 *                          All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************
 */

/**************************************************************************//**
 * @file        k_mem.c
 * @brief       Kernel Memory Management API C Code
 *
 * @version     V1.2021.01.lab2
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @note        skeleton code
 *
 *****************************************************************************/

/**
 * @brief:  k_mem.c kernel API implementations, this is only a skeleton.
 * @author: Yiqing Huang
 */

#include "k_mem.h"
#include "Serial.h"
#ifdef DEBUG_0
#include "printf.h"
#endif  /* DEBUG_0 */

//force inline

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */
// kernel stack size, referred by startup_a9.s
const U32 g_k_stack_size = K_STACK_SIZE;
// task proc space stack size in bytes, referred by system_a9.cs
const U32 g_p_stack_size = U_STACK_SIZE;



// task kernel stacks
U32 g_k_stacks[MAX_TASKS][K_STACK_SIZE >> 2] __attribute__((aligned(8)));

//process stack for tasks in SYS mode
U32 g_p_stacks[MAX_TASKS][U_STACK_SIZE >> 2] __attribute__((aligned(8)));

typedef struct node {
    U32 size;
    task_t owner; //INITIALIZE THIS TO -1 SOMEHOW?? OR AN INVALID NUMBER LIKE 300
} node_t;
//let negative size = allocated and pos = freee
/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

U32* k_alloc_k_stack(task_t tid)
{
	 //do we have access to the array of tasks?? is stack size fixed as U_STACK_SIZE and K_STACK_SIZE?
	//where do we dealloc???? can we call k_mem_dealloc from exit?
	//check if full
    return g_k_stacks[tid+1]; //why +1
}

U32* k_alloc_p_stack(task_t tid)
{
	//int i = (int)tid;
	size_t stack_size = g_tcbs[tid].u_stack_size;
	node_t *rtn_ptr =((node_t *)k_mem_alloc(stack_size));
	if (rtn_ptr == NULL) {
		return NULL;
	}
	node_t *ptr = rtn_ptr - 1;

	ptr->owner = 0;

	return (U32*)((U32)rtn_ptr + stack_size);
//    return g_p_stacks[tid+1];
}

// Linked list of memory blocks
// This is initialized in k_mem_init
node_t *head;

int k_mem_init(void) {
    unsigned int end_addr = (unsigned int) &Image$$ZI_DATA$$ZI$$Limit;
    head = (node_t *) end_addr;
    if (end_addr > RAM_END) {
    	// wrong end_addr
    	return RTX_ERR;
    }
    head->size = (((U32) RAM_END) - end_addr) + 1 - sizeof(node_t);
    if (head->size <= sizeof(node_t)) {
    	// no free space
        return RTX_ERR;
    }
    return RTX_OK;
}

void* k_mem_alloc(size_t size) {
//#ifdef DEBUG_0
//    printf("k_mem_alloc: requested memory size = %d\r\n", size);
//#endif /* DEBUG_0 */
    if ((size == 0) || (head == NULL)) {
    	return NULL;
    }

    int total_size = size + ((8- size%8)%8);


    //check if total size is too big and see


    node_t *ptr = head;
	while ((ptr->size & 1) || ptr->size < total_size) {					//ptr is allocated or too small
		ptr = (node_t *) ((U32)ptr + sizeof(node_t) + (ptr->size & ~1));
		if ((U32)ptr > RAM_END) {
			//no free space of the right size
			return NULL;
		}
    }

	if (!((ptr->size) & 1) && (ptr->size) > (total_size + sizeof(node_t))) {										//if there is enough free space for ptr+header
		node_t * new_node = (node_t *) (((U32) ptr) + sizeof(node_t) + total_size);

		new_node->size = ptr->size - total_size - sizeof(node_t);

		ptr->size = total_size | 1;

	} else {
		ptr->size = (ptr->size) | 1;
	}
	ptr->owner = gp_current_task->tid;
	return (void *) (ptr + 1);
}


int k_mem_dealloc(void *ptr) {

	size_t stack_size = gp_current_task->u_stack_size;

	void *dealloc_ptr = (void*)((U32)ptr - stack_size);

	node_t *node_ptr = ((node_t*) dealloc_ptr) - 1;

	if(node_ptr->owner != gp_current_task->tid){
		return RTX_ERR;
	}

    node_t *list_ptr = head;
    node_t *prev = NULL;


    if ( ((U32)dealloc_ptr > (U32)RAM_END) || (dealloc_ptr <= (void*)head)) {
    	return RTX_ERR;
    }

    while ((void*)(list_ptr + 1) < dealloc_ptr) {
    	prev = list_ptr;
    	list_ptr = (node_t*)((U32)list_ptr + sizeof(node_t) + (list_ptr->size & ~1));
    }

    if (((void*)(list_ptr + 1) != dealloc_ptr) || !(list_ptr->size & 1)) {
        //we have passed the pointer OR it has already been freed
        return RTX_ERR;
    }

    list_ptr->size = (list_ptr->size) & ~1;			//making size pos i.e. free

    if ( (((node_t*)((U32)list_ptr + sizeof(node_t) + list_ptr->size))->size & 1) == 0) {
    	list_ptr->size = list_ptr->size + ((node_t*)((U32)list_ptr + sizeof(node_t) + list_ptr->size))->size + sizeof(node_t);
    }

    if (!(prev->size & 1)) {
    	prev->size = prev->size + list_ptr->size + sizeof(node_t);
    }

    return RTX_OK;
}

int k_mem_count_extfrag(size_t size) {
    node_t *ptr = head;
    int count = 0;

    while((U32)ptr < RAM_END) {
    	if (!(ptr->size & 1) && ((ptr->size + sizeof(node_t)) < size)) {
    		++count;
    	}
    	ptr = (node_t*)((U32)ptr + sizeof(node_t) + (ptr->size & ~1));
    }

    // RE

    return count;
}

/*
 *===========================================================================
 *                           HELPER FUNCTIONS
 *===========================================================================
 */


/*
 *===========================================================================
 *                           TESTING FUNCTIONS
 *===========================================================================
 */




/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
