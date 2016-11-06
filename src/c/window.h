#pragma once

/*************************************************************
 *                                                           *
 * Window module. Create the user interface, manage layers   *
 * and subscribe to the pedometer module for total steps     *
 * updates.                                                  *
 *                                                           *
 *************************************************************/

/** Initalize UI */
void init_ui(Window*);

/** Free all allocated resources */
void deinit_ui(Window*);