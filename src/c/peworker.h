#pragma once

/*************************************************************
 *                                                           *
 * Pedometer module. Subscribe to the accelerometer data and *
 * compute the total steps number.                           *
 *                                                           *
 *************************************************************/

/** Initialize module */
void pe_init();

/** Free allocated resources... */
void pe_deinit();

/** Set the total steps count */
void pe_set(int total);

/** 
 *  Subscribe to be updated each time the total steps count
 *  is modified.
 */
void pe_subscribe(void (*handler)(int));