#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "kernel.h"
#include "kernel_id.h"

#include "ecrobot_interface.h"
#include "USBLoopBack.h"

/* OSEK declarations */
DeclareTask(Task_ts1);
DeclareTask(Task_background);
DeclareResource(USB_Rx);
DeclareCounter(SysTimerCnt);

#define MAX_NUM_OF_CHAR 16
#define MAX_NUM_OF_LINE 8

static int pos_x = 0;
static int pos_y = 0;

static void display_string_with_offset(U8 *data, int offset)
{
    char buffer[MAX_NUM_OF_CHAR+1];
    int offset_local = offset;
    int iterate = 1;
    size_t len;
    size_t n_bytes;

    /* iterate in case the string is longer than MAX_NUM_OF_CHAR */
    while(iterate) {
      len = strlen((char *)&data[offset_local]);
        n_bytes = MAX_NUM_OF_CHAR;
        if (len < MAX_NUM_OF_CHAR) {
            n_bytes = len;
            iterate = 0;
        }
        memcpy(buffer, (char *)&data[offset_local], n_bytes);
	    buffer[n_bytes] = 0;
        offset_local += n_bytes;

        if (pos_x == 0 && pos_y == 0) {
            display_clear(0);
        }

        /* update and set the cursor position */
        display_goto_xy(pos_x, pos_y);
        display_string(buffer);
        display_update();

        //added! : simple motor control code
        if(!strcmp(buffer, STOP)){
            // stop every motor
            nxt_motor_set_speed(NXT_PORT_A, 0, 1);
            nxt_motor_set_speed(NXT_PORT_B, 0, 1);
            nxt_motor_set_speed(NXT_PORT_C, 0, 1);
        }
        else if(!strcmp(buffer, FORWARD)){
            // go forward!
            nxt_motor_set_speed(NXT_PORT_A, -20, 1);
            nxt_motor_set_speed(NXT_PORT_B, 20, 1);
            nxt_motor_set_speed(NXT_PORT_C, 0, 1);
        }
        else if(!strcmp(buffer, BACKWARD)){
            // go backward!
            nxt_motor_set_speed(NXT_PORT_A, 20, 1);
            nxt_motor_set_speed(NXT_PORT_B, -20, 1);
            nxt_motor_set_speed(NXT_PORT_C, 0, 1);
        }
        else if(!strcmp(buffer, LEFT)){
            // go left!
            nxt_motor_set_speed(NXT_PORT_A, -20, 1);
            nxt_motor_set_speed(NXT_PORT_B, -20, 1);
            nxt_motor_set_speed(NXT_PORT_C, -20, 1);
        }
        else if(!strcmp(buffer, RIGHT)){
            // go right!
            nxt_motor_set_speed(NXT_PORT_A, 20, 1);
            nxt_motor_set_speed(NXT_PORT_B, 20, 1);
            nxt_motor_set_speed(NXT_PORT_C, 20, 1);
        }

        pos_y = pos_y+1;
        if (pos_y >= MAX_NUM_OF_LINE)
        {
            pos_x = 0;
            pos_y = 0;
        }
    }
}

static void showInitScreen(void)
{
    pos_x = 0;
    pos_y = 0;

    display_clear(0);
    display_string_with_offset("narNXT ver 1.0.0", 0);
}

/* ECRobot hooks */
void ecrobot_device_initialize()
{
    /* init USB */
    ecrobot_init_usb();
}

void ecrobot_device_terminate()
{
    /* terminate USB */
    ecrobot_term_usb();
}

/* nxtOSEK hook to be invoked from an ISR in category 2 */
void user_1ms_isr_type2(void)
{
    /* Increment System Timer Count to activate periodical Tasks */
    (void)SignalCounter(SysTimerCnt);
}

/* 1msec periodical Task */
TASK(Task_ts1)
{
    GetResource(USB_Rx);
    /* USB process handler (must be invoked every 1msec) */
    ecrobot_process1ms_usb();
    ReleaseResource(USB_Rx);

    TerminateTask();
}

/* background Task */
TASK(Task_background)
{
    int len;
    /* first byte is preserved for ID */
    U8 data[MAX_USB_DATA_LEN];
    char buffer[MAX_NUM_OF_CHAR+1];

    showInitScreen();

    while(1) {
        /* flush buffer */
        memset(data, 0, MAX_USB_DATA_LEN);
        /* critical section */
        GetResource(USB_Rx);
        /* read USB data */
        len = ecrobot_read_usb(data, 0, MAX_USB_DATA_LEN);
        ReleaseResource(USB_Rx);

        if (len > 0) {
            if (data[0] == DISCONNECT_REQ) {
                /* disconnect current connection */
                ecrobot_disconnect_usb();		
                showInitScreen();
            } else if (data[0] == COMM_STRING) {
                /* output the message */
                display_string_with_offset(data, 1);

                /* send back the acknowledgment ok string */
                data[0] = ACK_STRING;
		data[1] = 'o';
                data[2] = 'k';
                data[3] = 0;

                GetResource(USB_Rx);
		len = ecrobot_send_usb(data, 0, 4);
                ReleaseResource(USB_Rx);

		sprintf(buffer, " [%d]", len);
		display_string(buffer);
		display_update();
            }
        }
    }
}
