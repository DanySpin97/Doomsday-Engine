/**
 * @file displaymode_macx.mm
 * Mac OS X implementation of the DisplayMode class. @ingroup gl
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "displaymode_macx.h"
#include "dd_types.h"

#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <AppKit/AppKit.h>
#include <vector>
#include <qDebug>

#include "window.h"

/// Returns -1 on error.
static int intFromDict(CFDictionaryRef dict, CFStringRef key)
{
    CFNumberRef ref = (CFNumberRef) CFDictionaryGetValue(dict, key);
    if(!ref) return -1;
    int value;
    if(!CFNumberGetValue(ref, kCFNumberIntType, &value)) return -1;
    return value;
}

/// Returns -1 on error.
static float floatFromDict(CFDictionaryRef dict, CFStringRef key)
{
    CFNumberRef ref = (CFNumberRef) CFDictionaryGetValue(dict, key);
    if(!ref) return -1;
    float value;
    if(!CFNumberGetValue(ref, kCFNumberFloatType, &value)) return -1;
    return value;
}

static DisplayMode modeFromDict(CFDictionaryRef dict)
{
    DisplayMode m;
    m.width       = intFromDict(dict,   kCGDisplayWidth);
    m.height      = intFromDict(dict,   kCGDisplayHeight);
    m.refreshRate = floatFromDict(dict, kCGDisplayRefreshRate);
    m.depth       = intFromDict(dict,   kCGDisplayBitsPerPixel);
    return m;
}

static std::vector<DisplayMode> displayModes;
static std::vector<CFDictionaryRef> displayDicts;
static CFDictionaryRef currentDisplayDict;

void DisplayMode_Native_Init(void)
{
    // Let's see which modes are available.
    CFArrayRef modes = CGDisplayAvailableModes(kCGDirectMainDisplay);
    CFIndex count = CFArrayGetCount(modes);
    for(CFIndex i = 0; i < count; ++i)
    {
        CFDictionaryRef dict = (CFDictionaryRef) CFArrayGetValueAtIndex(modes, i);
        displayModes.push_back(modeFromDict(dict));
        displayDicts.push_back(dict);
    }
    currentDisplayDict = (CFDictionaryRef) CGDisplayCurrentMode(kCGDirectMainDisplay);
}

static bool captureDisplays(int capture)
{
    if(capture && !CGDisplayIsCaptured(kCGDirectMainDisplay))
    {
        return CGCaptureAllDisplays() == kCGErrorSuccess;
    }
    else if(!capture && CGDisplayIsCaptured(kCGDirectMainDisplay))
    {
        CGReleaseAllDisplays();
        return true;
    }
    return true;
}

static void releaseDisplays()
{
    captureDisplays(false);
}

void DisplayMode_Native_Shutdown(void)
{
    displayModes.clear();
    releaseDisplays();
}

int DisplayMode_Native_Count(void)
{
    return displayModes.size();
}

void DisplayMode_Native_GetMode(int index, DisplayMode* mode)
{
    assert(index >= 0 && index < (int)displayModes.size());
    *mode = displayModes[index];
}

void DisplayMode_Native_GetCurrentMode(DisplayMode* mode)
{
    *mode = modeFromDict(currentDisplayDict);
}

static int findIndex(const DisplayMode* mode)
{
    for(unsigned int i = 0; i < displayModes.size(); ++i)
    {
        if(displayModes[i].width == mode->width &&
           displayModes[i].height == mode->height &&
           displayModes[i].depth == mode->depth &&
           displayModes[i].refreshRate == mode->refreshRate)
        {
            return i;
        }
    }
    return -1; // Invalid mode.
}

int DisplayMode_Native_Change(const DisplayMode* mode, boolean shouldCapture)
{
    const CGDisplayFadeInterval fadeTime = .5f;

    assert(mode);
    assert(findIndex(mode) >= 0); // mode must be an enumerated one

    // Fade all displays to black (blocks until faded).
    CGDisplayFadeReservationToken token;
    CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
    CGDisplayFade(token, fadeTime, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, true /* wait */);

    // Capture the displays now if haven't yet done so.
    bool wasPreviouslyCaptured = CGDisplayIsCaptured(kCGDirectMainDisplay);
    CGDisplayErr result = kCGErrorSuccess;

    CFDictionaryRef newModeDict = displayDicts[findIndex(mode)];

    // Capture displays if instructed to do so.
    if(shouldCapture && !captureDisplays(true))
    {
        result = kCGErrorFailure;
    }

    if(result == kCGErrorSuccess && currentDisplayDict != newModeDict)
    {
        qDebug() << "Changing to native mode" << findIndex(mode);

        // Try to change.
        result = CGDisplaySwitchToMode(kCGDirectMainDisplay, newModeDict);
        if(result != kCGErrorSuccess)
        {
            // Oh no!
            CGDisplaySwitchToMode(kCGDirectMainDisplay, currentDisplayDict);
            if(!wasPreviouslyCaptured) releaseDisplays();
        }
        else
        {
            currentDisplayDict = displayDicts[findIndex(mode)];
        }
    }

    // Fade back to normal.
    CGDisplayFade(token, 2*fadeTime, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, false);
    CGReleaseDisplayFadeReservation(token);

    // Release display capture if instructed to do so.
    if(!shouldCapture)
    {
        captureDisplays(false);
    }

    return result == kCGErrorSuccess;
}

void DisplayMode_Native_Raise(void* nativeHandle)
{
    assert(nativeHandle);

    NSView* handle = (NSView*) nativeHandle;
    NSWindow* wnd = [handle window];
    [wnd setLevel:CGShieldingWindowLevel()];
}
