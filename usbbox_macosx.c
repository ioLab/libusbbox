/*
 *  usbbox_macosx.c
 *  lib-test
 *
 *  Created by John Montgomery on 19/01/2008.
 *  Copyright 2008 ioLab. All rights reserved.
 *
 */

#include "usbbox.h"
#include <stdio.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
#include <time.h>

#define EXPORT __attribute__((visibility("default")))

#define READ_BUFFER_SIZE 338

typedef struct {
    usb_box_message* msgs;
    int capacity;
    int length;
} message_queue;

typedef struct {
    int is_open;
    io_object_t dev;
    IOHIDDeviceInterface122 **interface;
    char buffer[READ_BUFFER_SIZE];
    message_queue queue;
    pthread_t read_thread;
    pthread_mutex_t mutex;
} usb_box_macosx;


static void queue_allocate(message_queue* queue) {
    queue->capacity=100;
    queue->length=0;
    queue->msgs=malloc(sizeof(usb_box_message)*queue->capacity);
}

static void queue_free(message_queue* queue) {
    free(queue->msgs);
}

static void queue_push(message_queue* queue, usb_box_message msg) {
    if ( queue->length >= queue->capacity ) {
        /* resize queue to bee big enough */
        int new_capacity = queue->capacity * 2;
        usb_box_message* new_msgs = realloc(queue->msgs,sizeof(usb_box_message)*new_capacity);
        if ( new_msgs ) {
            queue->msgs     = new_msgs;
            queue->capacity = new_capacity;
        }
        else {
            /* can't re-allocate any memory */
            perror("could not increase size of message queue - message lost");
            return;
        }
    }
    
    /* copy message onto end of queue */
    memcpy(*(queue->msgs + queue->length), msg, sizeof(usb_box_message));
    queue->length++;
}

static int queue_pop(message_queue* queue, usb_box_message* msg_ptr) {
    if ( queue->length <= 0 ) {
        return 0;
    }
    /* copy message from front of queue */
    memcpy(*msg_ptr, *queue->msgs, sizeof(usb_box_message));
    queue->length--;
    /* move messages down by 1 */
    queue->msgs = memmove(queue->msgs,queue->msgs+1,queue->length);
    return 1;
}

/* sync with mutex when popping and pushing messages onto boxes queue. */
static int pop_msg(usb_box_macosx* box, usb_box_message* msg_ptr) {
    int result = -1;
    pthread_mutex_lock(&(box->mutex));
    if ( queue_pop(&(box->queue),msg_ptr) ) {
        result=0;
    }
    pthread_mutex_unlock(&(box->mutex));
    return result;
}

static void push_msg(usb_box_macosx* box, usb_box_message msg) {
    pthread_mutex_lock(&(box->mutex));
    queue_push(&(box->queue),msg);
    pthread_mutex_unlock(&(box->mutex));
}

static int _last_error = 0;

static void set_last_error(int err) {
    _last_error=err;
}

static void message_callback(void *arg, IOReturn result, void *refcon, void *sender, uint32_t size) {
    usb_box_macosx* box = (usb_box_macosx*)arg;
    usb_box_message msg;
    memcpy(msg, box->buffer, sizeof(usb_box_message));
    push_msg(box, msg);
}


/*main message loop*/
static void *read_messages(void* arg) {
    CFRunLoopSourceRef eventSource;
    mach_port_t port;
    SInt32 reason;
    
    usb_box_macosx* box = (usb_box_macosx*)arg;
    
    (*(box->interface))->createAsyncPort(box->interface, &port);
    (*(box->interface))->createAsyncEventSource(box->interface, &eventSource);
    (*(box->interface))->setInterruptReportHandlerCallback(
        box->interface,
        box->buffer,
        READ_BUFFER_SIZE,
        message_callback,
        box,
        NULL);
    (*(box->interface))->startAllQueues(box->interface);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), eventSource, kCFRunLoopDefaultMode);
    
    while( box->is_open ) {
        pthread_testcancel();
        reason = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, 0);
    }
    
    return 0;
}

static void open_box(usb_box_macosx* box) {
    SInt32 score;
    IOCFPlugInInterface **plugInInterface;
    
    IOCreatePlugInInterfaceForService(
        box->dev,
        kIOHIDDeviceUserClientTypeID,
        kIOCFPlugInInterfaceID,
        &plugInInterface,
        &score
    );
    
    (*plugInInterface)->QueryInterface(
        plugInInterface,
        CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID),
        (LPVOID)&(box->interface)
    );
    
    (*plugInInterface)->Release(plugInInterface);
    (*(box->interface))->open(box->interface, 0);
    
    queue_allocate(&box->queue);
    
    pthread_mutex_init(&(box->mutex),NULL);
    
    box->is_open=1;
    
    pthread_create(&(box->read_thread), NULL, read_messages, box);
}

static void close_box(usb_box_macosx* box) {
    /* TODO halt threads etc  */
    box->is_open = 0;
    
    pthread_cancel(box->read_thread);
    pthread_join(box->read_thread, NULL);
    
    pthread_mutex_destroy(&(box->mutex));
    
    queue_free(&box->queue);
    
    (*(box->interface))->close(box->interface);
    
}

/*
 * return the last known error that occurred when performing 
 * a call to the box
 */
EXPORT int usb_box_last_error(void) {
    return _last_error;
}


 /*
 * find the usb box and return a device handle for it or NULL if not such device found (or
 * an error occurs)
 * a successful call to usb_box_open must be accompanied by a call
 * to usb_box_close when the device has been finished with
 */
EXPORT usb_box usb_box_open(void) {
    IOReturn result = kIOReturnSuccess;
    io_iterator_t hidObjectIterator = 0;
    io_object_t hidDevice = IO_OBJECT_NULL;
    int found = 0;
    CFMutableDictionaryRef hidMatchDictionary = 0;
    CFMutableDictionaryRef hidProperties = 0;

    hidMatchDictionary = IOServiceMatching(kIOHIDDeviceKey);
    result = IOServiceGetMatchingServices(kIOMasterPortDefault, hidMatchDictionary, &hidObjectIterator);

    if ((result != kIOReturnSuccess) || (hidObjectIterator == 0)) {
        perror("Can't obtain an IO iterator\n");
        set_last_error(-1);
        return NULL;
    }

    while ((hidDevice = IOIteratorNext(hidObjectIterator))) {
        hidProperties = 0;
        int vendor = 0, product = 0;
        
        result = IORegistryEntryCreateCFProperties(hidDevice, &hidProperties, kCFAllocatorDefault, kNilOptions);
        if ((result == KERN_SUCCESS) && hidProperties) {
            CFNumberRef vendorRef, productRef;
            
            vendorRef = CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDVendorIDKey));
            productRef = CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDProductIDKey));
            if (vendorRef) {
                CFNumberGetValue(vendorRef, kCFNumberIntType, &vendor);
                CFRelease(vendorRef);
            }
            if (productRef) {
                CFNumberGetValue(productRef, kCFNumberIntType, &product);
                CFRelease(productRef);
            }
        }
        if ( vendor == USB_BOX_VENDOR_ID && product == USB_BOX_PRODUCT_ID ) {
            found=1;
            break;
        }
        else {
            IOObjectRelease(hidDevice);
        }
    }
    
    IOObjectRelease(hidObjectIterator);
    
    /* no error either found box ok or didn't */
    set_last_error(0);
    
    if ( found ) {
        usb_box_macosx* box = malloc(sizeof(usb_box_macosx));
        box->is_open=0;
        box->dev = hidDevice;
        open_box(box);
        return (usb_box)box;
    }
    
    return NULL;
}


/*
 * close the usb box handle and free up any resource associated with it.
 */
EXPORT void usb_box_close(usb_box box) {
    if ( box ) {
        usb_box_macosx* osx_box = (usb_box_macosx*)box;
        if ( osx_box->is_open ) {
            /* close the device */
            close_box(osx_box);
        }
        if ( osx_box->dev != IO_OBJECT_NULL ) {
            /* release the box and null the reference */
            IOObjectRelease(osx_box->dev);
            osx_box->dev = IO_OBJECT_NULL;
        }
        free(box);
    }
}


/*
 * send a message to the box (8 bytes)
 * 0 indicates the message was sent ok.
 * any other value indicates an error
 */
EXPORT int usb_box_write(usb_box box, usb_box_message msg) {
    if ( box ) {
        usb_box_macosx* osx_box = (usb_box_macosx*)box;

        (*(osx_box->interface))->setReport(
            osx_box->interface,     // self
            kIOHIDReportTypeOutput, // report type
            0,                      // report ID
            msg,                    // buffer
            sizeof(msg),            // size
            100,                    // timeout (in ms)
            0,                      // callback function
            0,                      // ... and arguments
            0
        );
    }
    return 0;
}

long time_diff_micro(struct timeval begin, struct timeval end) {
    long sec_diff = end.tv_sec - begin.tv_sec;
    long usec_diff = end.tv_usec - begin.tv_usec;
    long total_usec_diff = sec_diff*1000000 + usec_diff;
    return total_usec_diff;
}

/*
 * try to read a message from the box.  
 * use a timeout of 0 to return immediately (polling).
 * 
 * 0 indicates the message was read successfully within
 * timeout milliseconds into the usb_box_message passed into
 * the function.
 * -1 indicates no message was read within the timeout.
 * any other return value indicates an error.
 */
EXPORT int usb_box_read(usb_box box, usb_box_message* msg_ptr, int timeout) {
    if ( box ) {
        usb_box_macosx* osx_box = (usb_box_macosx*)box;
        int result = -1, repeat=0;
        struct timeval begin, end;
        do {
            result = pop_msg(osx_box, msg_ptr);
            if ( result != 0 ) {
                /* nothing on the queue */
                if ( timeout <= 0 ) {
                    /* special case, no timeout, so just quit
                       the loop straight away. */
                    break;
                }
                
                /*otherwise we'll update the time and sleep for a little while*/
                if (!repeat) {
                    /* first time through */
                    gettimeofday(&begin, NULL);
                }
                gettimeofday(&end, NULL);
                /* yield */
                usleep(100);
                repeat++;
            }
        }
        while( result != 0 && time_diff_micro(begin,end) < (timeout*1000) );
        
        return result;
    }
	return -1;
}

int usb_box_sprintf_message(char* str, usb_box_message msg) {
    int i;
    int bytes[USB_BOX_MESSAGE_SIZE];
    for ( i = 0; i < USB_BOX_MESSAGE_SIZE; i++ ) {
        bytes[i]=msg[i];
    }
    return sprintf(str, "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X",bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7]);
}

/* insert data into msg_ptr based on string representation (8 c-style 2-digit hex numbers) separated by commas*/
int usb_box_sscanf_message(const char* str, usb_box_message* msg_ptr) {
    int i;
    int bytes[USB_BOX_MESSAGE_SIZE];
    int res = sscanf(
        str,
        "0x%2X,0x%2X,0x%2X,0x%2X,0x%2X,0x%2X,0x%2X,0x%2X",
        &(bytes[0]),
        &(bytes[1]),
        &(bytes[2]),
        &(bytes[3]),
        &(bytes[4]),
        &(bytes[5]),
        &(bytes[6]),
        &(bytes[7])
    );
    for ( i = 0; i < USB_BOX_MESSAGE_SIZE; i++ ) {
        (*msg_ptr)[i]=bytes[i];
    }
    return res;
}


 


