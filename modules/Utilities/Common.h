//-------------------------------------------------------------------------------------------------
// /Utilities/Common.h
// The nModules Project
//
// Common header used for including windows.h via the LiteStep core helper.
//-------------------------------------------------------------------------------------------------
#pragma once

#ifndef NOCOMM
#define NOCOMM
#endif
#ifndef NOMCX
#define NOMCX
#endif

// Reuse the core LiteStep common header so target constants and system defines stay aligned.
#include "../../utility/common.h"

#include "Debugging.h"
#include "Macros.h"
