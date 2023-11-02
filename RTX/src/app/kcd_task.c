/* The KCD Task Template File */

//do we need any includes?? probably lol
#include "common.h" //for KCD_MBX_SIZE
#include "k_rtx.h"
#include "rtx.h"
#include "printf.h"
#include "k_HAL_CA.h"
#include "interrupt.h"
#include "Serial.h"
// 3. reserve TID_UART_IRQ (255)
//3.5 error hadnler
// 4. check over and test (+ ask questions on piazza if needed)
U8 rcv_buf[sizeof(char) + sizeof(RTX_MSG_HDR)] ={0};

void kcd_task(void)
{
    /* not implemented yet */
	//user task
	//could be passed to k_tsk_init (along with prio and user stack size) through task_info argument
	//RESERVE TID_KCD FOR KCD TID (159) -- set the tid of a task with kcd_task as its ENTRY POINT to TID_KCD
		//have to change k_task stuff
		//ALREADY CHANGED IN CREATE BUT CHANGE IT IN INIT!!! INIT HAS TO MAKE SURE TID IS RIGHT TODO

	//running
	// first, request a mailbox of size KCD_MBX_SIZE
	mbx_create(KCD_MBX_SIZE);
	char command[64] = { 0 };
	int start_msg = 0;
	size_t command_size = 0;

	task_t cmd_id[123] = {0}; //ascii code values from 0 to 122 (we dont use 0-47 ... maybe do smthn abt it??)
	//init to 0 bc NULL task will never register an ID




	//ask if we will every get the weird characters and if we have to check that we are not getting them

	U8 msg_buf_u8[64+sizeof(RTX_MSG_HDR)];
	task_t receiver_tid = 0;
	while (1) {

		size_t length = sizeof(char) + sizeof(RTX_MSG_HDR); //is every msg (KEY_IN and KCD_REG) only one char?
//		void * buffer = k_mem_alloc(length);
//		if(buffer == NULL){
//			continue;
//		}
		task_t sender_tid = 0;
		size_t ptr_len = 0;
		size_t ptr_type = 0;

//		if (!is_empty(gp_current_task)){
//			for (size_t i = 0; i < sizeof(U32); i++) {
//				ptr_len |= ((size_t)(gp_current_task->mailbox)[(gp_current_task->mb_head) + i]) << (8*i);
//			}
//
//			for (size_t i = 0; i < sizeof(U32); i++) {
//				ptr_type |= ((size_t)(gp_current_task->mailbox)[(gp_current_task->mb_head) + i + 4]) << (8*i);
//			}
//		}
//
		if (recv_msg(&sender_tid, (void *)rcv_buf, length) == RTX_ERR) {
//	    	task_t tid = gp_current_task->tid;
//	    	gp_current_task->tid = 0;
//			mem_dealloc(buffer);
//			gp_current_task->tid = tid;
			continue;
		}
		//might not work bc of 4 byte align shit
		RTX_MSG_HDR *ptr = (RTX_MSG_HDR *)rcv_buf;
		ptr_len = ptr->length;
		ptr_type = ptr->type;


		if (ptr_type == KCD_REG && ptr_len == (sizeof(char) + sizeof(RTX_MSG_HDR))) { //unsure about length bc of % stuff
			U8 *buf = rcv_buf; //is this correct??
			buf += sizeof(RTX_MSG_HDR);
			int index = (int) *buf; //should this be an int???
			if (index < 48 || index  > 122 || (index > 57 && index < 65) || (index > 90 && index < 97)) {			//if not an alphanumeric character
				continue;
			}
			cmd_id[index] = sender_tid; //sender_tid should not be null
		} else if (ptr_type == KEY_IN && sender_tid == TID_UART_IRQ) { //is there a min length???
			//empty out command array once we get enter key and send the msg and stuff
			U8 *buf = rcv_buf; //is this correct??
			buf += sizeof(RTX_MSG_HDR);
			U8 lookup_char;
			int lookup = 0;


			if(start_msg){
				if (command_size == 0) {
					//command id
					command[0] = *buf;
					lookup_char = command[0];
					lookup = (int)lookup_char;
					if(lookup < 48 || lookup  > 122 || (lookup > 57 && lookup < 65) || (lookup > 90 && lookup < 97)||(cmd_id[lookup] == 0)){
						command[0] = 0;
						SER_PutStr(1, "Command cannot be processed\n\r");
						command_size = 0;
						start_msg = 0;
						continue;
					}
					command_size++;
				} else if (*buf == '\r' ) { //enter was pressed
					lookup_char = command[0];
                    lookup = (int)lookup_char;
					receiver_tid = cmd_id[lookup];

					if ((receiver_tid == 0) || (g_tcbs[receiver_tid]).state == DORMANT){
                        SER_PutStr(1, "Command cannot be processed\n\r");
                        command_size = 0;
						start_msg = 0;                                   //return that error thing
						continue;
					}
//					RTX_MSG_HDR * msg_buf = (RTX_MSG_HDR *)mem_alloc(sizeof(RTX_MSG_HDR)+command_size);
//					send_length = sizeof(RTX_MSG_HDR)+command_size;
					RTX_MSG_HDR * msg_buf = (RTX_MSG_HDR *)msg_buf_u8;

					msg_buf->length = (sizeof(RTX_MSG_HDR))+command_size;
					msg_buf->type = KCD_CMD;
//					U8 *msg_buf_u8 = (U8 *)msg_buf;

					for(size_t i = 0; i < command_size; i++){
						*((char*)msg_buf + sizeof(RTX_MSG_HDR) + i) = command[i];
					}
					if (send_msg(receiver_tid,(void*) msg_buf) != RTX_OK) {
						SER_PutStr(1, "Command cannot be processed\n\r");
					}

					command_size = 0;
					start_msg = 0;
					continue;
				} else {
					//if command_size == 64 : throw Invalid Command
					//contents of the message
					if (command_size == 64){
						SER_PutStr(1, "Invalid Command\n\r");
						command_size = 0;
						start_msg = 0;
						continue;

						//throw error
					}
                    if(*buf < 48 || *buf  > 122 || (*buf > 57 && *buf < 65) || (*buf > 90 && *buf < 97)){
						command[0] = 0;
						SER_PutStr(1, "Command cannot be processed\n\r");
						command_size = 0;
						start_msg = 0;
						continue;
					}
                    command[command_size] = *buf;
					command_size++;
				}
				/*if key is enter:
				 * 0) check that the command[0] is a registered identifier and find it in cmd_id and save its TID (if invalid throw error:)
				 * 1) create a message buffer of size command_size + headersize
				 * 2) assign length and type for header and add it to the msg buffer
				 * 3) in a for loop, copy over from array into msg buffer
				 * 4) send message
				 * 4.5) check if send message fails then throw: Command cant be proc
				 * 5) set command size to 0
				 */

			}
			else if ((*buf == '%')) { // we are starting a new msg (not in the middle of one!)
				start_msg = 1;
				//reset the size of the command either here or when we hit enter
			}else{
				SER_PutStr(1, "Invalid Command\n\r");
				//send "Invalid command error
			}

		}


	}
}
