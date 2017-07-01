/* Copyright (C) 2009-2012 Red Hat, Inc.
 Copyright (C) 2013-2014 Nathan Hoad.
 Copyright (C) 2016 by Sergey Ptashnick.

 This was an interface with iwlib by Nathan Hoad <nathan@getoffmalawn.com>.
 Castrated and disfigured for XML output by Sergey Ptashnick <0comffdiz@inbox.ru>.

 This application is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; version 2.

 This application is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details. */

#include <iwlib.h> 

#define IWSCAN_ALL_IFACES "all"

/* IWSCAND_PID_FILE_DIR can also be "/run" */
#define IWSCAND_PID_FILE_DIR "."

/* Just for reference */
#define IWS_TIMESTAMP_FORMAT "%FT%TZ"
/* Length: YYYY-MM-DD (10) + "T" (1) + "HH:MM:SS" (8) + "Z\0" (2) = 21 */
#define IWS_TIMESTAMP_LENGTH 21

/* stamp_buf must be preallocated for IWS_TIMESTAMP_LENGTH chars */
void iws_format_timestamp(char * stamp_buf);

int iws_print_scanning_info_xml(FILE * fd, int skfd, char * ifname, char * args[], 
                int count);

