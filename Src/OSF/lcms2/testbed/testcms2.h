//---------------------------------------------------------------------------------
//
//  Little Color Management System
//  Copyright (c) 1998-2020 Marti Maria Saguer
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//---------------------------------------------------------------------------------
//

#ifndef TESTCMS2_H
#define TESTCMS2_H

#include "lcms2_internal.h"

// On Visual Studio, use debug CRT
#ifdef _MSC_VER
#    include "crtdbg.h"
#endif

#ifdef CMS_IS_WINDOWS_
#    include <io.h>
#endif

#define cmsmin(a, b) (((a) < (b)) ? (a) : (b))

// Used to mark special pointers
void DebugMemDontCheckThis(void * Ptr);

boolint STDCALL IsGoodVal(const char * title, double in, double out, double max);
boolint STDCALL IsGoodFixed15_16(const char * title, double in, double out);
boolint IsGoodFixed8_8(const char * title, double in, double out);
boolint STDCALL IsGoodWord(const char * title, uint16 in, uint16 out);
boolint STDCALL IsGoodWordPrec(const char * title, uint16 in, uint16 out, uint16 maxErr);

void* PluginMemHandler(void);
cmsContext WatchDogContext(void* usr);

void ResetFatalError(void);
void Die(const char* Reason, ...);
void Dot(void);
void Fail(const char* frm, ...);
void SubTest(const char* frm, ...);
void TestMemoryLeaks(boolint ok);
void Say(const char* str);

// Plug-in tests
cmsInt32Number CheckSimpleContext(void);
cmsInt32Number CheckAllocContext(void);
cmsInt32Number CheckAlarmColorsContext(void);
cmsInt32Number CheckAdaptationStateContext(void);
cmsInt32Number CheckInterp1DPlugin(void);
cmsInt32Number CheckInterp3DPlugin(void);
cmsInt32Number CheckParametricCurvePlugin(void);
cmsInt32Number CheckFormattersPlugin(void);
cmsInt32Number CheckTagTypePlugin(void);
cmsInt32Number CheckMPEPlugin(void);
cmsInt32Number CheckOptimizationPlugin(void);
cmsInt32Number CheckIntentPlugin(void);
cmsInt32Number CheckTransformPlugin(void);
cmsInt32Number CheckMutexPlugin(void);
cmsInt32Number CheckMethodPackDoublesFromFloat(void);

// Zoo
void CheckProfileZOO(void);

#endif
