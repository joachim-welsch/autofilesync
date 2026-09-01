#ifdef _MACOS
#import <Cocoa/Cocoa.h>

void macTransformToForegroundApp()
{
    // Not needed for .app bundles - they are already foreground apps.
    // Calling [NSApp setActivationPolicy:] before wxWidgets initialization
    // interferes with wxWidgets' Cocoa menu system.
}
#endif
