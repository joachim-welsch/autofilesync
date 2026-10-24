/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#include "ForegroundApp.h"
#import <AppKit/AppKit.h>
#include <unistd.h>

namespace guitest {

void bring_to_foreground()
{
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
}

void disable_window_animations()  // [ForegroundApp.md#disable_window_animations]
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{@"NSAutomaticWindowAnimationsEnabled": @NO}];
}

void settle_window_server()  // [ForegroundApp.md#settle_window_server]
{
    for (int i = 0; i < 5; ++i)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
}

namespace {

NSDictionary* window_info_at(int x, int y)  // [ForegroundApp.md#window_at_point]
{
    const CGPoint point = CGPointMake(x, y);
    NSArray* windows = CFBridgingRelease(CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID));
    for (NSDictionary* window in windows) {
        CGRect bounds;
        if ([window[(id)kCGWindowLayer] intValue] > kCGModalPanelWindowLevel || [window[(id)kCGWindowAlpha] doubleValue] == 0.0
            || !CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)window[(id)kCGWindowBounds], &bounds) || !CGRectContainsPoint(bounds, point))
            continue;
        return window;
    }
    return nil;
}

}

long window_number(void* ns_window)  // [ForegroundApp.md#window-number]
{
    return [(__bridge NSWindow*)ns_window windowNumber];
}

bool window_exists(long number)
{
    return [NSApp windowWithWindowNumber:number] != nil;
}

bool window_at_point_is(int x, int y, long number)
{
    @autoreleasepool {
        NSDictionary* window = window_info_at(x, y);
        return window != nil && [window[(id)kCGWindowNumber] integerValue] == number;
    }
}

std::string window_at_point(int x, int y)
{
    @autoreleasepool {
        NSDictionary* window = window_info_at(x, y);
        if (window == nil)
            return "kein Fenster";
        NSWindow* own = [NSApp windowWithWindowNumber:[window[(id)kCGWindowNumber] integerValue]];
        NSString* name = own != nil ? [own title] : window[(id)kCGWindowOwnerName];
        return std::string([name UTF8String]);
    }
}

}

