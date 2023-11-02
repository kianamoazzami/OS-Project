/**
 * @file:   k_msg.c
 * @brief:  kernel message passing routines
 * @author: Yiqing Huang
 * @date:   2020/10/09
 */

#include "k_msg.h"
#include "k_task.h"
#include "printf.h"



#ifdef DEBUG_0
#include "printf.h"
#endif /* ! DEBUG_0 */


/*
 * CITCULAR QUEUE FUNCTIONSL
 *
 */

int enqueue(TCB * tcb, char *buf) {
	//check that mailbox has space first
	//
	//potentially remove , doing this in send_msg
//	if (tcb->mailbox == NULL || buf == NULL){
//			return RTX_ERR;
//	}

	size_t size_buf = ((RTX_MSG_HDR*) buf)->length;


//	printf("message size = %d , avaialblable size = %d \n",size_buf, (tcb->mb_size - abs( tcb->mb_tail - tcb->mb_head)));
	if (is_full(tcb) || ((size_buf+sizeof(task_t)) > tcb->mb_size) || (size_buf+sizeof(task_t)) > (tcb->mb_size - abs( tcb->mb_tail - tcb->mb_head)) || size_buf <  MIN_MSG_SIZE){
		//not enough space
		return RTX_ERR;
	}



	if (is_empty(tcb)){
		// empty queue
		tcb->mb_head = 0;
		tcb->mb_tail = 0;
	}

//	char * tid_ptr =  (char *) &(gp_current_task->tid);
//	for (size_t i = 0;  i< sizeof(task_t); i++){
//		tcb->mailbox[tcb->mb_tail] =  *(tid_ptr + i ); //assuming lowest address
//		tcb->mb_tail = (tcb->mb_tail+1) %  tcb->mb_size;
//	}
	for (size_t i = 0;  i< size_buf; i++){
		tcb->mailbox[tcb->mb_tail] = buf[i]; //assuming lowest address
		tcb->mb_tail = (tcb->mb_tail+1) %  tcb->mb_size;
	}

	char *p_sender_tid = (char *) &(gp_current_task->tid);
	for (size_t i = 0; i < sizeof(task_t); i++) {
		tcb->mailbox[tcb->mb_tail] = p_sender_tid[i];
		tcb->mb_tail = (tcb->mb_tail+1) %  tcb->mb_size;
	}

	return RTX_OK;

}

int dequeue(char * buf, task_t * sender_tid, size_t buf_len) {
	if (gp_current_task->mailbox == NULL ){
		return RTX_ERR;
	}

//	if (is_empty(gp_current_task)){
//		buf = NULL; // check if buf is null in rcv message, meant mailbox was empty
//		return RTX_OK;
//	}


//	size_t msg_size = ((RTX_MSG_HDR *)((gp_current_task->mailbox)[gp_current_task->mb_head]))->length;
	size_t msg_size = 0;
	for (size_t i = 0; i < sizeof(U32); i++) {
		msg_size |= ((size_t)(gp_current_task->mailbox)[(gp_current_task->mb_head) + i]) << (8*i);
	}

	if ((msg_size) > buf_len || buf == NULL){									//Checks if buffer doesn't have enough space then discards the message
		 gp_current_task->mb_head = (gp_current_task->mb_head+(msg_size+sizeof(task_t))) %  gp_current_task->mb_size;
		 if (gp_current_task -> mb_head == gp_current_task->mb_tail){
		 		gp_current_task-> mb_head = gp_current_task->mb_size;
		 		gp_current_task->mb_tail =  gp_current_task->mb_size ;
		 }
		 return RTX_ERR;
	}

	for (size_t i = 0;  i< msg_size; i++){
		 buf[i] = gp_current_task->mailbox[gp_current_task->mb_head]; //assuming lowest address
		 gp_current_task->mb_head = (gp_current_task->mb_head+1) %  gp_current_task->mb_size;
//		 printf("%s", buf[i]);
	}

//	if (sender_tid != NULL) {
//		*sender_tid = 0;;
//	}
//	for (size_t i = 0; i < sizeof(task_t); i++) {
//		if (sender_tid != NULL) {
//			*sender_tid |= ((size_t)(gp_current_task->mailbox)[(gp_current_task->mb_head)]) << (8*i);
//		}
//		gp_current_task->mb_head = (gp_current_task->mb_head+1) %  gp_current_task->mb_size;
//	}
	char *p_sender_tid = (char *) &(gp_current_task->mailbox[gp_current_task->mb_head]);
	for (size_t i = 0; i < sizeof(task_t); i++) {
		if (sender_tid != NULL) {
			sender_tid[i] = p_sender_tid[i];
		}
		gp_current_task->mb_head = (gp_current_task->mb_head+1) %  gp_current_task->mb_size;
	}


	if (gp_current_task -> mb_head == gp_current_task->mb_tail){
		gp_current_task-> mb_head = gp_current_task->mb_size;
		gp_current_task->mb_tail =  gp_current_task->mb_size ;
	}


	return RTX_OK;

}

int is_full(TCB * tcb) {
	 if ((is_empty(tcb) == 0) && (tcb->mb_head == tcb->mb_tail)){
		 return 1;
	 }
	  return 0;
}

int is_empty(TCB * tcb) {
	if (tcb->mb_head == tcb->mb_size && tcb->mb_tail ==  tcb->mb_size){
		return 1;
	}
	return 0;

}


int k_mbx_create(size_t size) {
#ifdef DEBUG_0
    printf("k_mbx_create: size = %d\r\n", size);
#endif /* DEBUG_0 */
    if (gp_current_task->mailbox != NULL || size < MIN_MBX_SIZE) { //make sure mailbox is initialized to NULL
    	return RTX_ERR;
    }

    //mem at run time is not enough (check if alloc is returning err)
    task_t tid = gp_current_task->tid;
    gp_current_task->tid = 0;
    gp_current_task->mailbox = k_mem_alloc(size);
    gp_current_task->tid = tid;

    if (gp_current_task->mailbox == NULL) {
    	return RTX_ERR;
    }
    gp_current_task->mb_size = size;
    gp_current_task->mb_head = size;
    gp_current_task->mb_tail = size;

    return 0;
}

int k_send_msg(task_t receiver_tid, const void *buf) {
#ifdef DEBUG_0
    printf("k_send_msg: receiver_tid = %d, buf=0x%x\r\n", receiver_tid, buf);
#endif /* DEBUG_0 */
    if ((g_tcbs[receiver_tid].state == DORMANT) || (g_tcbs[receiver_tid].mailbox == NULL) || (buf == NULL) ){
    	return RTX_ERR;
    }

    if (enqueue(&g_tcbs[receiver_tid], (char *)buf) == RTX_ERR){			//MODIFY TO STORE SENDER TID !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    	return RTX_ERR;
    }

    if(g_tcbs[receiver_tid].state == BLK_MSG){
    	g_tcbs[receiver_tid].state = READY;
    	k_tsk_add_q(&g_tcbs[receiver_tid]);
    	if (g_tcbs[receiver_tid].prio < gp_current_task->prio){ //higher prio than current task
			if(k_tsk_run_new() == RTX_ERR){
				return RTX_ERR;
			}
    	}
    }
    return RTX_OK;
}


int k_recv_msg(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */
    //all RTX_ERR conditions in dequeue
//    printf("IN RECIEVE\n");
    // DO  SENDER TID STUFF!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//    if(buf == NULL){
//    	//discard first message TODO
//    	return RTX_ERR;
//    }
    if (is_empty(gp_current_task) == 1){
		if(buf == NULL){						//if empty and buf is NUll what to do?
			return RTX_ERR;
		}
    	gp_current_task->state = BLK_MSG;
//    	buf = NULL;		;		//removes from ready queue and runs a new task
    	if(k_tsk_run_new() != RTX_ERR){
    		return dequeue((char*)buf, sender_tid, len);
    	}
    	else{
    		return RTX_ERR;
    	}

    }
    else{
    	return dequeue((char*)buf, sender_tid, len);
    }

}

int k_recv_msg_nb(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg_nb: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */
    return 0;
}

int k_mbx_ls(task_t *buf, int count) {
#ifdef DEBUG_0
    printf("k_mbx_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}
