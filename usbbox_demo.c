#include "usbbox.h"

#include <stdio.h>
#include <pthread.h>

/* 
 A simple console-based app that reads messages from stdin and writes messages to std out.
 
 Suitable for use for (crude) control of the usb box via the bash shell
 */

static void clear_queued_messages(usb_box box) {
    usb_box_message purge;
    usb_box_sscanf_message("0x51,0x00,0x00,0x00,0x00,0x00,0x00,0x00",&purge);
    usb_box_write(box,purge);
    
    usb_box_message msg;
    while(1) {
        if ( usb_box_read(box,&msg,500) != 0) {
            return;
        }
    }
}

static void *read_messages(void* arg) {
    char buffer[64];
    usb_box_message msg;
    usb_box box = arg;
    
    while(1) {
        pthread_testcancel();
        
        if ( usb_box_read(box,&msg,100) == 0) {
            usb_box_sprintf_message(buffer,msg);
            printf("%s\n",buffer);
        }
    }
}

static void write_messages(usb_box box) {
    char buffer[64];
    usb_box_message msg;
    while( fgets(buffer, 64, stdin ) != NULL ) {
        usb_box_sscanf_message(buffer, &msg);
        if ( usb_box_write(box,msg) != 0 ) {
            /* if we fail to right quit. */
            perror("error writing message to box");
            return;
        }
    }
}

int main(int argc, char** argv) {
    pthread_t read_thread;
    usb_box box;
    
    box = usb_box_open();
    if ( box == NULL ) {
        perror("could not find usb box");
        return -1;
    }
    
    /* clear any messages that were pending */
    clear_queued_messages(box);
    
    /* spawn a thread to read incoming messages from the box. */
    pthread_create(&read_thread, NULL, read_messages, box);
    
    write_messages(box);
    
    pthread_cancel(read_thread);
    pthread_join(read_thread, NULL);
    
    usb_box_close(box);
    return 0;
}