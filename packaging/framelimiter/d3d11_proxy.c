/*
 * d3d11.dll proxy for CLT.exe: a low-latency frame limiter in front of DXMT.
 *
 * The game's only d3d11 import is D3D11CreateDeviceAndSwapChain. This proxy forwards it to the real d3d11.dll
 * (DXMT, loaded from system32 by full path) and replaces IDXGISwapChain::Present in the returned swapchain's
 * vtable. The wrapper presents first and then sleeps until the next frame deadline, so the game samples input right
 * after the wait and the queued frame never waits behind it. Metal-paced limiting (DXMT preferredMaxFrameRate) blocks
 * before the frame reaches the screen instead, which adds one to two frames of input latency.
 *
 * CLT_MAX_FPS (environment, read once): frames per second; 0 or unset disables the limiter.
 */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdlib.h>

typedef HRESULT(WINAPI *create_device_and_swapchain_fn)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT,
	const D3D_FEATURE_LEVEL *, UINT, UINT, const DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D11Device **,
	D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
typedef HRESULT(STDMETHODCALLTYPE *present_fn)(IDXGISwapChain *, UINT, UINT);

/* IUnknown (3) + IDXGIObject (4) + IDXGIDeviceSubObject (1): Present is slot 8. */
enum { PRESENT_SLOT = 8 };

static present_fn real_present;
static LONGLONG frame_ticks;  /* QPC ticks per frame; 0 = no limit */
static LONGLONG next_deadline;
static LONGLONG qpc_freq;

static LONGLONG now(void)
{
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return t.QuadPart;
}

static void wait_for_deadline(void)
{
	LONGLONG t = now();
	if (!next_deadline || t - next_deadline > frame_ticks) {
		/* first frame, or the game fell behind by more than a frame: restart the cadence instead of bursting */
		next_deadline = t + frame_ticks;
		return;
	}
	/* Sleep coarsely, then spin the last ~1.5 ms: Sleep granularity is about a millisecond. */
	LONGLONG spin = qpc_freq * 3 / 2000;
	while (next_deadline - (t = now()) > spin) {
		DWORD ms = (DWORD)((next_deadline - t - spin) * 1000 / qpc_freq);
		Sleep(ms ? ms : 1);
	}
	while (now() < next_deadline) YieldProcessor();
	next_deadline += frame_ticks;
}

static HRESULT STDMETHODCALLTYPE limited_present(IDXGISwapChain *swapchain, UINT sync_interval, UINT flags)
{
	HRESULT hr = real_present(swapchain, sync_interval, flags);
	if (!(flags & DXGI_PRESENT_TEST)) wait_for_deadline();
	return hr;
}

static void hook_present(IDXGISwapChain *swapchain)
{
	void **vtable = *(void ***)swapchain;
	DWORD old;
	if (vtable[PRESENT_SLOT] == (void *)limited_present) return;
	if (!VirtualProtect(&vtable[PRESENT_SLOT], sizeof(void *), PAGE_EXECUTE_READWRITE, &old)) return;
	real_present = (present_fn)vtable[PRESENT_SLOT];
	vtable[PRESENT_SLOT] = (void *)limited_present;
	VirtualProtect(&vtable[PRESENT_SLOT], sizeof(void *), old, &old);
}

static HMODULE real_d3d11(void)
{
	static HMODULE module;
	WCHAR path[MAX_PATH];
	UINT n;
	if (module) return module;
	n = GetSystemDirectoryW(path, MAX_PATH - 16);
	if (!n) return NULL;
	lstrcpyW(path + n, L"\\d3d11.dll");
	return module = LoadLibraryW(path);
}

HRESULT WINAPI D3D11CreateDeviceAndSwapChain(IDXGIAdapter *adapter, D3D_DRIVER_TYPE driver_type, HMODULE software,
	UINT flags, const D3D_FEATURE_LEVEL *levels, UINT level_count, UINT sdk_version,
	const DXGI_SWAP_CHAIN_DESC *desc, IDXGISwapChain **swapchain, ID3D11Device **device,
	D3D_FEATURE_LEVEL *level, ID3D11DeviceContext **context)
{
	HMODULE real = real_d3d11();
	create_device_and_swapchain_fn create;
	HRESULT hr;
	if (!real || !(create = (create_device_and_swapchain_fn)GetProcAddress(real, "D3D11CreateDeviceAndSwapChain")))
		return E_FAIL;
	hr = create(adapter, driver_type, software, flags, levels, level_count, sdk_version, desc, swapchain, device,
		level, context);
	if (SUCCEEDED(hr) && swapchain && *swapchain && frame_ticks) hook_present(*swapchain);
	return hr;
}

HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter *adapter, D3D_DRIVER_TYPE driver_type, HMODULE software, UINT flags,
	const D3D_FEATURE_LEVEL *levels, UINT level_count, UINT sdk_version, ID3D11Device **device,
	D3D_FEATURE_LEVEL *level, ID3D11DeviceContext **context)
{
	return D3D11CreateDeviceAndSwapChain(adapter, driver_type, software, flags, levels, level_count, sdk_version,
		NULL, NULL, device, level, context);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
	if (reason == DLL_PROCESS_ATTACH) {
		const char *env = getenv("CLT_MAX_FPS");
		int fps = env ? atoi(env) : 0;
		LARGE_INTEGER f;
		DisableThreadLibraryCalls(instance);
		QueryPerformanceFrequency(&f);
		qpc_freq = f.QuadPart;
		frame_ticks = fps > 0 ? qpc_freq / fps : 0;
	}
	return TRUE;
}
