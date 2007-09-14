//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: //depot/SOURCE/OPENSOURCE/kfs/src/cc/libkfsIO/Globals.h#3 $
//
// Created 2006/10/09
// Author: Sriram Rao (Kosmix Corp.) 
//
// Copyright 2006 Kosmix Corp.
//
// This file is part of Kosmos File System (KFS).
//
// Licensed under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.
//
// \brief Define the globals needed by the KFS IO library.  These
//globals are also available to any app that uses the KFS IO library.
//----------------------------------------------------------------------------

#ifndef LIBKFSIO_GLOBALS_H
#define LIBKFSIO_GLOBALS_H

#include "DiskManager.h"
#include "NetManager.h"
#include "EventManager.h"
#include "Counter.h"

namespace libkfsio
{

    struct Globals_t {
        DiskManager diskManager;
        NetManager netManager;
        EventManager eventManager;
        CounterManager counterManager;
        // Commonly needed counters
        Counter ctrOpenNetFds;
        Counter ctrOpenDiskFds;
        Counter ctrNetBytesRead;
        Counter ctrNetBytesWritten;
        Counter ctrDiskBytesRead;
        Counter ctrDiskBytesWritten;
        // track the # of failed read/writes
        Counter ctrDiskIOErrors;
    };

    void InitGlobals();

    Globals_t & globals();
}

#endif // LIBKFSIO_GLOBALS_H