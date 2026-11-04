/*
 * Cocoa side of dxmt_export.c.
 *
 * Wine's newMetalViewWithDevice: inserts the metal view *below* its siblings, which is right for the Vulkan/GL
 * client views Wine 11 creates for those APIs. DXMT draws into the window's content view instead, where the window
 * surface (GDI, black for a D3D window) sits above and hides every presented frame. Raise the view to the top.
 */
#import "config.h"
#import "macdrv.h"

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>

#import "cocoa_event.h"

/* Upstream Wine 11 sets client_view only once a GL/Vulkan client surface exists; a plain D3D11 window has none.
 * CrossOver hands DXMT the window's content view (a WineContentView, which implements newMetalViewWithDevice:).
 * Read directly: the caller holds win_data_mutex, so it must not wait on the main thread here. */
WineContentView *dxmt_window_content_view(WineWindow *window)
{
    return window ? (WineContentView *)[(NSWindow *)window contentView] : NULL;
}

WineMetalView *dxmt_view_create_metal_view_on_top(WineContentView *view, MTLDevice_id device)
{
    WineMetalView *metal = macdrv_view_create_metal_view(view, device);
    if (!metal) return NULL;
    OnMainThread(^{
        NSView *parent = (NSView *)view;
        [parent addSubview:(NSView *)metal positioned:NSWindowAbove relativeTo:nil];
    });
    return metal;
}
