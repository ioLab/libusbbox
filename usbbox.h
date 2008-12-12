/*
 *  usbbox.h
 *  lib-test
 *
 *  Created by John Montgomery on 19/01/2008.
 *  Copyright 2008 IOLab. All rights reserved.
 *
 */

#ifndef __USBBOX__
#define __USBBOX__ 

#define USB_BOX_VENDOR_ID 0x19BC
#define USB_BOX_PRODUCT_ID 0x0001

#define USB_BOX_MESSAGE_SIZE 8

typedef void* usb_box; /*handle to the device - platform specific*/

typedef unsigned char usb_box_message[USB_BOX_MESSAGE_SIZE]; /* 8 byte message */


/*
 * return the last known error that occurred when performing 
 * a call to the box
 */
int usb_box_last_error(void);

/*
 * find the usb box and return a device handle for it or NULL if not such device found (or
 * an error occurs)
 * a successful call to usb_box_open must be accompanied by a call
 * to usb_box_close when the device has been finished with
 */
usb_box usb_box_open(void);

/*
 * close the usb box handle and free up any resource associated with it.
 */
void usb_box_close(usb_box box);

/*
 * send a message to the box (8 bytes)
 * 0 indicates the message was sent ok.
 * any other value indicates an error
 */
int usb_box_write(usb_box box, usb_box_message msg);

/*
 * try to read a message from the box.  
 * 0 indicates the message was read successfully within
 * timeout milliseconds into the usb_box_message passed into
 * the function.
 * -1 indicates no message was read within the timeout.
 * any other return value indicates an error.
 */
int usb_box_read(usb_box box, usb_box_message* msg_ptr, int timeout); 

/* output message in format suitable for usb_box_sscanf_message */
int usb_box_sprintf_message(char* str, usb_box_message msg);

/* insert data into msg_ptr based on string representation (8 c-style 2-digit hex numbers) separated by commas*/
int usb_box_sscanf_message(const char* str, usb_box_message* msg_ptr);

#endif
