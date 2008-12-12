#include "usbbox.h"

#include <stdio.h>

#define TEST_PASSED 0
#define TEST_FAILED -1
#define BOX_NOT_CONNECTED -2

/* slightly nasty using a macro for these, but it works. */
#define ASSERT_TRUE(condition, msg) if ( !(condition) ) { printf("%s:%u: %s\n", __FILE__, __LINE__, msg); return TEST_FAILED; }

typedef int (*test_function)(usb_box);

#define NUM_TESTS 5

int wait_for_reply(usb_box box, int msg_id, usb_box_message* msg_ptr) {
    int res = -1;
    int found = 0;
    int tries = 5;
    do {
        if ( (res=usb_box_read(box,msg_ptr,500)) != 0) {
            return res;
        }
        if ( (*msg_ptr)[0] == msg_id ) {
            return 0;
        }
        tries--;
    }
    while( !found && tries > 0 );
    return -1;
}

void clear_queued_messages(usb_box box) {
    usb_box_message msg;
    while(1) {
        if ( usb_box_read(box,&msg,500) != 0) {
            return;
        }
    }
}

void setup(usb_box box) {
    /* enable then disable loopback to force a reset */
    usb_box_message msg = {0x57,0x01,0x00,0x00,0x00,0x00,0x00,0x00};
    usb_box_write(box,msg);
    msg[1] = 0x00;
    usb_box_write(box,msg);
    clear_queued_messages(box);
}

/* placeholder test to exercise connecting to the box */
int test_box_found(usb_box box) {
    return TEST_PASSED;
}

/* check that sending a loop-back message doesn't bail */
int test_box_write(usb_box box) {
    usb_box_message msg = {0x57,0x01,0x00,0x00,0x00,0x00,0x00,0x00};
    ASSERT_TRUE(usb_box_write(box,msg) == 0,"problem writing to box");
    return TEST_PASSED;
}

/* just check that nothing messes up trying to read messages. */
int test_box_read(usb_box box) {
    usb_box_message msg;
    usb_box_read(box,&msg,100);
    return TEST_PASSED;
}

int test_reset_clock(usb_box box) {
    usb_box_message msg = {0x52,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    usb_box_message received;
    
    ASSERT_TRUE(usb_box_write(box,msg) == 0, "problem writing to box");
    
    ASSERT_TRUE(wait_for_reply(box,0x4B,&received) == 0, "did not received reply");
    
    ASSERT_TRUE( received[0] == 0x4B, "wrong replay received" );
    /*char buffer[100];
    usb_box_sprintf_message(buffer,received);
    printf("%s\n",buffer);*/
    
    return TEST_PASSED;
}

/* test setting LEDs on port 2 */
int test_setting_LEDs(usb_box box) {
    int res = 0;
    /* 0x32 = P2SET */
    usb_box_message msg = {0x32,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    usb_box_message received;
    unsigned int i = 0;
    
    for ( i = 0; i < 256; i++ ) {
        msg[1] = i; /* set port state */
        ASSERT_TRUE(usb_box_write(box,msg) == 0, "problem writing to box");
        
        /* 0x50 = PXREP */
        ASSERT_TRUE(wait_for_reply(box,0x50,&received) == 0, "did not received reply");
        
        ASSERT_TRUE( received[2] == i, "wrong port state set" );
    }
    
    return TEST_PASSED;
}

int run_test(test_function fn) {
    usb_box box = usb_box_open();
    if ( box == NULL ) {
        return BOX_NOT_CONNECTED;
    }
    else {
        setup(box);
        int result = fn(box);
        usb_box_close(box);
        return result;
    }
}

int main(int argc, char** argv) {
    test_function tests[NUM_TESTS] = { 
        test_box_found,
        test_box_write,
        test_box_read,
        test_reset_clock,
        test_setting_LEDs
    };
    
    int failed=0, ran=0, i;
    for ( i = 0; i < NUM_TESTS; i++ ) {
        int res = run_test(tests[i]);
        switch(res) {
            case TEST_PASSED: {
                printf(".");
            }
            break;
            case TEST_FAILED: {
                printf("F");
                failed++;
            }
            break;
            case BOX_NOT_CONNECTED: {
                /* force loop to quit */
                printf("box not connected");
                failed++;
                i = NUM_TESTS;
            }
            break;
        }
        ran++;
    }
    
    printf("\n");
    if ( failed ) {
        printf("FAILED\n");
    }
    else {
        printf("OK\n");
    }
    printf("%d test ran, %d failed\n",ran,failed);
    
    return 0;
}