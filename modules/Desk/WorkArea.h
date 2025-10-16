/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *  WorkArea.h
 *  The nModules Project
 *
 *  Function declarations for WorkArea.cpp
 *  
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma once

#include "../ModuleKit/MonitorInfo.hpp"

namespace WorkArea {
    void LoadSettings(MonitorInfo *, bool = false);
    void ResetWorkAreas(MonitorInfo *);
    void ParseLine(MonitorInfo *, LPCTSTR);
}

