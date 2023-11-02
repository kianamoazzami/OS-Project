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
 * @file        k_task.c
 * @brief       task management C file
 *              l2
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only two simple privileged task and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/

//#include "VE_A9_MP.h"
#include "Serial.h"
#include "k_task.h"
#include "k_rtx.h"

extern void kcd_task(void);

#ifdef DEBUG_0
#include "printf.h"
#endif /* DEBUG_0 */

/*
 * ========================================================================
 * PRAYERS
 * ========================================================================
 *Our Father who is in heaven, uphold the holiness of your name.
 *Bring in your kingdom so that your will is done on earth as it’s done in heaven.
 *Give us the bread we need for today.
 *Forgive us for the ways we have wronged you, just as we also forgive those who have wronged us.
 *And don’t lead us into temptation, but rescue us from ece.
 *Amen
 */


/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */



typedef struct pair {
	TCB* head;
	TCB* tail;
}pair;

extern TCB             *gp_current_task = NULL;	// the current RUNNING task
//task_t   		gp_current_added_task = 0;	// the current RUNNING task
TCB             g_tcbs[MAX_TASKS];			// an array of TCBs  //WHAT IF TID_KCD IS GREATER THAN MAX_TASKS??
RTX_TASK_INFO   g_null_task_info;			// The null task info
U32             g_num_active_tasks = 0;		// number of non-dormant tasks
pair            g_queue[PRIO_NULL];			// an array of pairs
int				highest_prio = PRIO_NULL;
task_t 			kcd_index = 0; 				//index of the KCD //not sure if we need this to be global //initialize to 0 or -1?

/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:

                       RAM_END+---------------------------+ High Address
                              |                           |
                              |                           |
                              |    Free memory space      |
                              |   (user space stacks      |
                              |         + heap            |
                              |                           |
                              |                           |
                              |                           |
 &Image$$ZI_DATA$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      U_STACK_SIZE         |     |
             g_p_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |  other kernel proc stacks |     |
                              |---------------------------|     |
                              |      U_STACK_SIZE         |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      K_STACK_SIZE         |     |                
             g_k_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |                              
                              |---------------------------|     |
                              |      K_STACK_SIZE         |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |                      
              g_k_stacks[1]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |          
                              |                           |     |
                              |                           |     |
                              |                           |     V
                     RAM_START+---------------------------+ Low Address
    
---------------------------------------------------------------------------*/ 

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 *
 *****************************************************************************/

TCB *scheduler(void)
{
#ifdef DEBUG_0
    printf("k_tsk_schedd: Entering\r\n");
#endif /* DEBUG_0 */
   // task_t tid = gp_current_task->tid;

    //Seeing if the current running task is already highest priority
	if (highest_prio >= gp_current_task->prio && (gp_current_task->state == RUNNING)){
		return gp_current_task;
	}
	// trusting there's a task at highest priority, head is not null

	//else returning the first one of the highest priority
    return g_queue[highest_prio-1].head;
}



/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 *
 * @see         k_tsk_create_new
 *****************************************************************************/

int k_tsk_init(RTX_TASK_INFO *task_info, int num_tasks)
{
    extern U32 SVC_RESTORE;
	task_t j = 0;
	task_t kcd_tsk_flg = 0;

    RTX_TASK_INFO *p_taskinfo = &g_null_task_info;
    g_num_active_tasks = 0;

    if (num_tasks >= MAX_TASKS) {
    	return RTX_ERR;
    }

     for (int l = 0; l < PRIO_NULL; ++l) {
    	 g_queue[l].head = NULL;
    	 g_queue[l].tail = NULL;
     }

    // create the first task
    TCB *p_tcb = &g_tcbs[0];
    p_tcb->prio     = PRIO_NULL;
    p_tcb->priv     = 1;
    p_tcb->tid      = TID_NULL;
    p_tcb->state    = RUNNING;
//    k_tsk_add_q(p_tcb); //DO THIS LATER -- add that if prio is prio null, dont remove it from q
    g_num_active_tasks++;
    gp_current_task = p_tcb;

    // create the rest of the tasks
    //maybe initilaize every other tid to dormant
    p_taskinfo = task_info;
    for ( task_t i = 0; i < num_tasks; i++ ) {

    	if (p_taskinfo->ptask == &kcd_task) {
    		if (MAX_TASKS < 160) {
    			j = MAX_TASKS - 1;  //make sure TID is still 159 (TID_KCD) though!
    		} else {
        		j = TID_KCD;
    		}
    		kcd_tsk_flg = i+1;
    		kcd_index = j;
    	} else {
    		j = i+1;
    	}

    	TCB *p_tcb = &g_tcbs[j];

		if (k_tsk_create_new(p_taskinfo, p_tcb, j) == RTX_OK) { //have to fix create new for kcd tid
			g_num_active_tasks++;
			if (k_tsk_add_q(p_tcb) == RTX_ERR) { //have to fix this q add for kcd tid
				return RTX_ERR;
			}
		} else {
			return RTX_ERR;
		}
		p_taskinfo++;
    }


//    for (int j = num_tasks + 1 ; j < MAX_TASKS; j++) {
//    	g_tcbs[j].state = DORMANT;
//    }

//kcd_tsk_flg holds the skipped value
    //will be 0 if no kcd task created

    if (kcd_tsk_flg != 0) { //skipped this in the above loop
		TCB *init_tcb = &g_tcbs[kcd_tsk_flg];
		init_tcb->state = DORMANT;
		init_tcb->tid = kcd_tsk_flg;
    }

    for ( int i = num_tasks + 1; i < MAX_TASKS; i++ ) {
    	if (i == kcd_index) { //if there is no kcd task, kcd_index will be 0 so i can never equal kcd_index
    		continue; //theres a way to do this outside of for loop without checking condition each time so if its slow we can refactor
    	}
		TCB *init_tcb = &g_tcbs[i];
		init_tcb->state = DORMANT;
		init_tcb->tid = i;
	}
    //from num tasks to end
    return RTX_OK;
}
/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task information structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR12)
 *              then we stack up the kernel initial context (kLR, kR0-kR12)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              30 registers in total
 *
 *****************************************************************************/
int k_tsk_create_new(RTX_TASK_INFO *p_taskinfo, TCB *p_tcb, task_t tid)
{
    extern U32 SVC_RESTORE;

    U32 *sp;

    if (p_taskinfo == NULL || p_tcb == NULL)
    {
        return RTX_ERR;
    }

    if ((kcd_index != 0) && (tid == kcd_index)) { //we are creating the KCD task (bc kcd_index may not be TID_KCD, it could be MAX_TASKS - 1)
    	p_tcb ->tid = TID_KCD;
    } else {
        p_tcb ->tid = tid;
    }
    p_tcb->state = READY;
    p_tcb->prio = p_taskinfo->prio;
	p_tcb->priv = p_taskinfo->priv;
	p_tcb->u_stack_size = p_taskinfo->u_stack_size;
	p_tcb->ptask = p_taskinfo->ptask;
	p_tcb->next = NULL;
	p_tcb->mailbox = NULL;
	p_tcb->mb_head = 0;
	p_tcb->mb_tail = 0;
	p_tcb->mb_size = 0;


    /*---------------------------------------------------------------
     *  Step1: allocate kernel stack for the task
     *         stacks grows down, stack base is at the high address
     * -------------------------------------------------------------*/

    ///////sp = g_k_stacks[tid] + (K_STACK_SIZE >> 2) ;
    sp = k_alloc_k_stack(p_tcb ->tid); //used to be just (tid) but we need to account for KCD task


    // 8B stack alignment adjustment
    if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
        sp--;               // adjust it to 8B aligned
    }

    /*-------------------------------------------------------------------
     *  Step2: create task's user/sys mode initial context on the kernel stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the user/sys mode context saved on the kernel stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         16 registers listed in push order
     *         <xPSR, PC, uSP, uR12, uR11, ...., uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    // uSP: initial user stack
    if ( p_taskinfo->priv == 0 ) { // unprivileged task
        // xPSR: Initial Processor State
        *(--sp) = INIT_CPSR_USER;
        // PC contains the entry point of the user/privileged task
        *(--sp) = (U32) (p_taskinfo->ptask);

        //********************************************************************//
        //*** allocate user stack from the user space, not implemented yet ***//
        //********************************************************************//

        p_tcb->usp = (U32*) k_alloc_p_stack(p_tcb ->tid); //used to be just (tid) but we need to account for KCD task
        if (p_tcb->usp == NULL) {
        	return RTX_ERR;
        }
        *(--sp) =  (U32)p_tcb->usp;

        // uR12, uR11, ..., uR0
        for ( int j = 0; j < 13; j++ ) {
            *(--sp) = 0x0;
        }
    }


    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         14 registers listed in push order
     *         <kLR, kR0-kR12>
     * -------------------------------------------------------------*/
    if ( p_taskinfo->priv == 0 ) {
        // user thread LR: return to the SVC handler
        *(--sp) = (U32) (&SVC_RESTORE);
    } else {
        // kernel thread LR: return to the entry point of the task
        *(--sp) = (U32) (p_taskinfo->ptask);
    }

    // kernel stack R0 - R12, 13 registers
    for ( int j = 0; j < 13; j++) {
        *(--sp) = 0x0;
    }

    // kernel stack CPSR
    *(--sp) = (U32) INIT_CPSR_SVC;
    p_tcb->ksp = sp;

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param:      p_tcb_old, the old tcb that was in RUNNING
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note:       caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PUSH    {R0-R12, LR}
        MRS 	R1, CPSR
        PUSH 	{R1}
        STR     SP, [R0, #TCB_KSP_OFFSET]   ; save SP to p_old_tcb->ksp
        LDR     R1, =__cpp(&gp_current_task);
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_KSP_OFFSET]   ; restore ksp of the gp_current_task
        POP		{R0}
        MSR		CPSR_cxsf, R0
        POP     {R0-R12, PC}
}


/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 *
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(void)
{
    TCB *p_tcb_old = NULL;
    
    if (gp_current_task == NULL) {
    	return RTX_ERR;
    }
    //save current task and get new current task
    p_tcb_old = gp_current_task;
    gp_current_task = scheduler();
    
    //if new current task is NULL return an error and revert back to the old task
    if ( gp_current_task == NULL  ) {
        gp_current_task = p_tcb_old;        // revert back to the old task
        return RTX_ERR;
    }

    // at this point, gp_current_task != NULL and p_tcb_old != NULL

    //if new current task is not the old one then make its state RUNNING and Remove it from the READY queue
    if (gp_current_task != p_tcb_old) { 		// add old task back into queue
        gp_current_task->state = RUNNING;   	// change state of the to-be-switched-in  tcb
        //REMOVE WAS CALLED HERE!!!
        k_tsk_rm_q(gp_current_task);
        // make the old current task to ready and add it back to the queue and switch the kernel
        if ( (p_tcb_old->state != DORMANT) && (p_tcb_old->state != BLK_MSG)){
        	p_tcb_old->state = READY;           // change state of the to-be-switched-out tcb //EDGE CASE!!!!!!! have to make sure it stays dormant after run new in exit
			// ADD BACK INTO QUEUE
        	if (k_tsk_add_q(p_tcb_old) == RTX_ERR) {
        		return RTX_ERR;
        	}
        }
    	k_tsk_switch(p_tcb_old);            // switch stacks //DO WE NEED TO DO THIS STILL IF OLD TASK IS DORMANT?????????
    }

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       remove from the ready queue
 * @param		p_tcb you want to remove
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 *****************************************************************************/
int k_tsk_rm_q(TCB *p_tcb)
{
	//go to priority in the queueueueuu (set a current variable to be head)
	TCB *current = (g_queue[(p_tcb->prio)-1]).head;
	TCB *tail = (g_queue[(p_tcb->prio)-1]).tail;
	//check if we are removing head
	if (current->tid == p_tcb->tid) {
		(g_queue[(p_tcb->prio)-1]).head = p_tcb->next;
		p_tcb->next = NULL;
		if (tail == p_tcb) {
			(g_queue[(p_tcb->prio)-1]).tail = NULL;
			(g_queue[(p_tcb->prio)-1]).head = NULL;
			if (p_tcb->prio == highest_prio) {
				for (int i = p_tcb->prio + 1; i <= PRIO_NULL; i++) {
					if (g_queue[i-1].head != NULL) {	//NULL check
						highest_prio = i;	//i+1 because priority is array index + 1 b/c (index = prio - 1)
						break;
					} else {
						highest_prio = PRIO_NULL;
					}
				}
			}
					// go through queue and stop when head != NULL and then set that one (make sure it is tcb prio) to new highest prio
		}
	} else {
		if (current->next == NULL) {
			return RTX_ERR; //shouldnt happen
		}
		while ((current->next)->tid != p_tcb->tid) {
			current = current->next;
			if (current->next == NULL)  {
				return RTX_ERR; // shouldnt happen
			}
		}
		current->next = p_tcb->next;
		p_tcb->next = NULL;
		if (tail == p_tcb) {
			tail = current;
			tail->next = NULL; // just in case!!!
		}
	}

   return RTX_OK;// edit function, call scheduler
}

/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 *****************************************************************************/
int k_tsk_add_q(TCB *p_tcb)
{
	//check its priority
	//go to that priority-1
	TCB *head_tcb = g_queue[p_tcb->prio - 1].head;
	//if head is null, set head and tail to p_tcb and set next to null
	if (head_tcb == NULL) {
		g_queue[p_tcb->prio - 1].head = p_tcb;
	} else {
		//tail's next should be NULL
		(g_queue[p_tcb->prio - 1].tail)->next = p_tcb;
	}
	g_queue[p_tcb->prio - 1].tail = p_tcb;
	p_tcb->next = NULL;

	//set highest priority if needed
	if (p_tcb->prio < highest_prio) {
		highest_prio = p_tcb->prio;
	}

   return RTX_OK;
}
/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{
	//DO NOT USE SCHEDULER OR RUN NEW HERE!!!
	if (gp_current_task == NULL || gp_current_task->state != RUNNING) {
		return RTX_ERR;
	}

	if (gp_current_task->prio < highest_prio) {
		return RTX_OK; // keep running ourself
	} else {
		// run g_queue[highest_prio-1].head


		//save current task and get new current task
		TCB *p_tcb_old = gp_current_task;
		gp_current_task = g_queue[highest_prio-1].head;

		//if new current task is NULL return an error and revert back to the old task
		if ( gp_current_task == NULL  ) {
			gp_current_task = p_tcb_old;        // revert back to the old task
			return RTX_ERR;
		}

		if (gp_current_task != p_tcb_old) {
			gp_current_task->state = RUNNING;
			//now remove the current task from queue
			k_tsk_rm_q(gp_current_task);
			if ((p_tcb_old->state != DORMANT) && (p_tcb_old->state != BLK_MSG)){ //when a task changes from BLK_MSG to ready, we have to add back into q
				p_tcb_old->state = READY;
				// ADD BACK INTO QUEUE
				if (k_tsk_add_q(p_tcb_old) == RTX_ERR) {
					return RTX_ERR;
				}
			}
			k_tsk_switch(p_tcb_old);
		} else {
			//should not be the same task in this case
			return RTX_ERR;
		}

	}

   return RTX_OK;
}


/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */

int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U16 stack_size)
{
#ifdef DEBUG_0
    printf("k_tsk_create: entering...\n\r");
    printf("task = 0x%x, task_entry = 0x%x, prio=%d, stack_size = %d\n\r", task, task_entry, prio, stack_size);
#endif /* DEBUG_0 */

    if (task == NULL || task_entry == NULL){
    	return RTX_ERR;
    }
    if (prio >= 255 || prio <= 0){
    	return RTX_ERR;
    }

    if (stack_size < U_STACK_SIZE || (stack_size %8 != 0) ){ //??
    	return RTX_ERR;					//!!!!!!!ask if stack size is a multiple of 8 and what if its zero!!!!!!!!!!!
    }

    if (g_num_active_tasks == MAX_TASKS){
    	return RTX_ERR;
    }

    RTX_TASK_INFO task_info;

    task_t i = 1;


    //create
    while ( g_tcbs[i].state != DORMANT){
    	i++;
    	if (i == MAX_TASKS) {
    		return RTX_ERR;
    	}
    }

    // dont assign tid to be kcd_index.
    //REMOVE IF SLOW
    if (i == kcd_index) { //technically this will never happen because KCD will never be dormant if it exists
    	if (i == MAX_TASKS - 1) {
    		//full
    	   return RTX_ERR;
    	}
    	i++;
    	while ( g_tcbs[i].state != DORMANT){
			i++;
			if (i == MAX_TASKS) {
				return RTX_ERR;
			}
		}
    }


    TCB *c_tcb = &g_tcbs[i];
    task_t new_tid = i;
    *task = new_tid;


    task_info.ptask = task_entry;		//not sure if syntax is good
    task_info.tid = new_tid;
    task_info.prio = prio;
    task_info.priv = 0;
    task_info.u_stack_size = stack_size;

    if (k_tsk_create_new(&task_info, c_tcb, new_tid) != RTX_OK){
    	return RTX_ERR;
    }

    //CHECK PREEMPTION
	//sets new highest_prio, state to READY
    if (k_tsk_add_q(c_tcb) == RTX_ERR) {
    	return RTX_ERR;
    }
    if (prio < gp_current_task->prio) {
    	//preempt current_task
    	if (k_tsk_run_new()== RTX_ERR) {
    		return RTX_ERR; //scheduler returns task_b, removes task_b from queue, adds current_task back to queue
    	}
    	//make sure we arent just running same task again bc if we are then we will be trying to remove a task that isnt in the  q
//    	k_tsk_rm_q(gp_current_task);
    }

    g_num_active_tasks++;
//    k_tsk_add_q(c_tcb);
//    k_tsk_run_new();

    return RTX_OK;

}

void k_tsk_exit(void) //assume KCD will never exit (never go dormant)
{
#ifdef DEBUG_0
    printf("k_tsk_exit: entering...\n\r");
#endif /* DEBUG_0 */
    //MAKE SURE IT IS CURRENT TASK BEING EXITED
    if (gp_current_task->priv == 0) { //user task
    	//current task shit->usp dealloc this
    	task_t tid = gp_current_task->tid;
    	gp_current_task->tid = 0;
        k_mem_dealloc(gp_current_task->usp);
        if (gp_current_task->mailbox != NULL){
        	k_mem_dealloc(gp_current_task->mailbox);
        	gp_current_task->mailbox = NULL;
        }
        gp_current_task->tid = tid;
    }
    gp_current_task->state = DORMANT; // have to make sure it stays dormant after run new
    g_num_active_tasks -= 1;
//    k_tsk_rm_q(gp_current_task);


    if (g_num_active_tasks == 1) {
    	highest_prio = PRIO_NULL;
    }
	if (k_tsk_run_new()== RTX_ERR) {
		//THERE IS AN ISSUE IF THIS HAPPENS
		return;
	}
	//make sure we arent just running same task again bc if we are then we will be trying to remove a task that isnt in the  q
//	k_tsk_rm_q(gp_current_task);

    return;
}

int k_tsk_set_prio(task_t task_id, U8 prio) 
{
#ifdef DEBUG_0
    printf("k_tsk_set_prio: entering...\n\r");
    printf("task_id = %d, prio = %d.\n\r", task_id, prio);
#endif /* DEBUG_0 */
    if (task_id >= MAX_TASKS || task_id == 0){
    	return RTX_ERR;
    }

    TCB *ptcb = NULL;

    if ((kcd_index != 0) && (task_id == TID_KCD)) { //KCD exists and we are setting TID of KCD
        ptcb = &g_tcbs[kcd_index];
    } else {
        ptcb = &g_tcbs[task_id];
    }

    if (ptcb->state == DORMANT || prio < 1 || prio > 254 || (ptcb->priv  == 1 && gp_current_task->priv == 0)){
    	return RTX_ERR;
    }

    if(ptcb->state == BLK_MSG){
    	ptcb->prio = prio;
    	return RTX_OK;
    }
    //the task calling set_prio not to itself
    if (gp_current_task->tid != task_id) {
		k_tsk_rm_q(ptcb);
	    ptcb->prio = prio;
		if (k_tsk_add_q(ptcb)== RTX_ERR) {
			return RTX_ERR;
		}
    	if (prio < gp_current_task->prio) {
    		if (k_tsk_run_new() == RTX_ERR) {
    			return RTX_ERR;
    		}
//    		k_tsk_rm_q(gp_current_task);
    	}
    } else {
    	//on itself
    	if (prio < gp_current_task->prio) {
    		gp_current_task->prio = prio;
    		//do nothing
    	} else {
    		gp_current_task->prio = prio;
    		if (highest_prio == gp_current_task->prio) {
    			TCB *p_tcb_old = NULL;

				//save current task and get new current task
				p_tcb_old = gp_current_task;
				gp_current_task = g_queue[highest_prio-1].head;

				//if new current task is NULL return an error and revert back to the old task
				if ( gp_current_task == NULL  ) {
					gp_current_task = p_tcb_old;        // revert back to the old task
					return RTX_ERR;
				}

				if (gp_current_task != p_tcb_old) {
					gp_current_task->state = RUNNING;
					//now remove the current task from queue
					k_tsk_rm_q(gp_current_task);
					if ( (p_tcb_old->state != DORMANT) && (p_tcb_old->state != BLK_MSG)){
						p_tcb_old->state = READY;
						// ADD BACK INTO QUEUE
						if (k_tsk_add_q(p_tcb_old) == RTX_ERR) {
							return RTX_ERR;
						}
					}
					k_tsk_switch(p_tcb_old);
				} else {
					//should not be the same task in this case
					return RTX_ERR;
				}
    		} else {
        		if (k_tsk_run_new() == RTX_ERR) {
        			return RTX_ERR;
        		}
    		}
    	}
    }

    return RTX_OK;    
}

int k_tsk_get_info(task_t task_id, RTX_TASK_INFO *buffer)
{
#ifdef DEBUG_0
    printf("k_tsk_get_info: entering...\n\r");
    printf("task_id = %d, buffer = 0x%x.\n\r", task_id, buffer);
#endif /* DEBUG_0 */    
    if (buffer == NULL) {
        return RTX_ERR;
    }
    if (task_id >= MAX_TASKS) {
    	return RTX_ERR;
	}
    /* The code fills the buffer with some fake task information. 
       You should fill the buffer with correct information    */
    TCB *init_tcb = NULL;

    if ((kcd_index != 0) && (task_id == TID_KCD)) { //KCD exists and we are setting TID of KCD
    	init_tcb = &g_tcbs[kcd_index];
    } else {
    	init_tcb = &g_tcbs[task_id];
    }
//    buffer->tid = task_id;
//    buffer->prio = 99;
//    buffer->state = 101;
//    buffer->priv = 0;
//    buffer->ptask = 0x0;
//    buffer->k_stack_size = K_STACK_SIZE;
//    buffer->u_stack_size = 0x200;

    buffer->tid = init_tcb->tid;
    buffer->prio = init_tcb->prio;
    buffer->state = init_tcb->state;
    buffer->priv = init_tcb->priv;
    buffer->ptask = init_tcb->ptask;	//return task_entry
    buffer->k_stack_size = K_STACK_SIZE;
    buffer->u_stack_size = init_tcb->u_stack_size;

    return RTX_OK;     
}

task_t k_tsk_get_tid(void)
{
#ifdef DEBUG_0
    printf("k_tsk_get_tid: entering...\n\r");

#endif /* DEBUG_0 */
    return gp_current_task->tid;
}

int k_tsk_ls(task_t *buf, int count){
#ifdef DEBUG_0
    printf("k_tsk_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB4
 *===========================================================================
 */

int k_tsk_create_rt(task_t *tid, TASK_RT *task)
{
    return 0;
}

void k_tsk_done_rt(void) {
#ifdef DEBUG_0
    printf("k_tsk_done: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

void k_tsk_suspend(TIMEVAL *tv)
{
#ifdef DEBUG_0
    printf("k_tsk_suspend: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
