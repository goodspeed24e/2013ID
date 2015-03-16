/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __FILE_DEFS_H__
#define __FILE_DEFS_H__

#include "mfxdefs.h"

#include <stdio.h>

#define MSDK_FOPEN(file, name, mode) _tfopen_s(&file, name, mode)

#define msdk_fgets  _fgetts

#endif // #ifndef __FILE_DEFS_H__
