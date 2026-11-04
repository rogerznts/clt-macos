/*
 * Export the winemac.drv entry points DXMT (D3D11 -> Metal) looks up with dlsym(RTLD_DEFAULT, "macdrv_functions").
 *
 * Upstream Wine builds winemac.so with -fvisibility=hidden, so DXMT cannot reach these functions, and DXMT reads
 * struct macdrv_win_data in the CrossOver layout {hwnd, cocoa_window, cocoa_view, client_cocoa_view}, which differs
 * from upstream ({hwnd, cocoa_window, client_view, rects, ...}). get_win_data here hands DXMT an adapter in the
 * CrossOver layout and keeps the real record for release_win_data. The Cocoa half lives in dxmt_cocoa.m.
 */
#if 0
#pragma makedep unix
#endif

#include "config.h"
#include <dispatch/dispatch.h>
#include <pthread.h>

#include "macdrv.h"

/* dxmt_cocoa.m */
extern WineContentView *dxmt_window_content_view(WineWindow *window);
extern WineMetalView *dxmt_view_create_metal_view_on_top(WineContentView *view, MTLDevice_id device);
/* cocoa_event.h is Objective-C only */
extern void OnMainThread(dispatch_block_t block);

struct dxmt_win_data
{
    HWND                     hwnd;
    WineWindow              *cocoa_window;
    WineContentView         *cocoa_view;
    WineContentView         *client_cocoa_view;
    struct macdrv_win_data  *real;
};

struct dxmt_macdrv_functions
{
    void (*macdrv_init_display_devices)(BOOL);
    struct dxmt_win_data *(*get_win_data)(HWND hwnd);
    void (*release_win_data)(struct dxmt_win_data *data);
    WineWindow *(*macdrv_get_cocoa_window)(HWND hwnd, BOOL require_on_screen);
    MTLDevice_id (*macdrv_create_metal_device)(void);
    void (*macdrv_release_metal_device)(MTLDevice_id d);
    WineMetalView *(*macdrv_view_create_metal_view)(WineContentView *v, MTLDevice_id d);
    CAMetalLayer *(*macdrv_view_get_metal_layer)(WineMetalView *v);
    void (*macdrv_view_release_metal_view)(WineMetalView *v);
    void (*on_main_thread)(dispatch_block_t b);
};

/* The adapter lives only while win_data_mutex is held (get_win_data locks, release_win_data unlocks), so one
 * per thread is enough and no allocation is needed. */
static __thread struct dxmt_win_data dxmt_data;

static struct dxmt_win_data *dxmt_get_win_data(HWND hwnd)
{
    struct macdrv_win_data *data = get_win_data(hwnd);
    WineContentView *view;
    if (!data) return NULL;
    view = data->client_view ? data->client_view : dxmt_window_content_view(data->cocoa_window);
    dxmt_data.hwnd = data->hwnd;
    dxmt_data.cocoa_window = data->cocoa_window;
    dxmt_data.cocoa_view = view;
    dxmt_data.client_cocoa_view = view;
    dxmt_data.real = data;
    return &dxmt_data;
}

static void dxmt_release_win_data(struct dxmt_win_data *data)
{
    if (data) release_win_data(data->real);
}

static WineWindow *dxmt_get_cocoa_window(HWND hwnd, BOOL require_on_screen)
{
    return macdrv_get_cocoa_window(hwnd, require_on_screen);
}

static void dxmt_init_display_devices(BOOL force)
{
}

static void dxmt_on_main_thread(dispatch_block_t b)
{
    OnMainThread(b);
}

__attribute__((visibility("default"))) const struct dxmt_macdrv_functions macdrv_functions =
{
    dxmt_init_display_devices,
    dxmt_get_win_data,
    dxmt_release_win_data,
    dxmt_get_cocoa_window,
    macdrv_create_metal_device,
    macdrv_release_metal_device,
    dxmt_view_create_metal_view_on_top,
    macdrv_view_get_metal_layer,
    macdrv_view_release_metal_view,
    dxmt_on_main_thread,
};
