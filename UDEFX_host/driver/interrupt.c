/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Interrupt.c

Abstract:

    This modules has routines configure a continuous reader on an
    interrupt pipe to asynchronously read toggle switch states.

Environment:

    Kernel mode

--*/

#include <osrusbfx2.h>

#if defined(EVENT_TRACING)
#include "interrupt.tmh"
#endif




