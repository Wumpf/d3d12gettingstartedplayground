#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>

using namespace Microsoft::WRL;

class Window;

#ifdef _DEBUG
	#define D3DDEBUG
#endif

class D3D12Device
{
public:
	D3D12Device(Window& window);
	~D3D12Device();

	/// Swaps backbuffer. Might stall for activated V-Sync.
	/// Signals the internal frameFence that is used to determine how many frames are inflight.
	/// Does not call any additional wait function (like WaitForFreeInflightFrame)
	void Present();

	/// Returns how many frames are currently in-flight.
	/// This means how many frames the CPU has prepared but are not yet completed by the GPU.
	unsigned int GetNumFramesInFlight();

	/// Waits until only MAX_FRAMES_INFLIGHT-1 frames are inflight so that a set of frame resources are available.
	void WaitForFreeInflightFrame();

	/// Waits until all prepared frames are renderd and the GPU has no more tasks.
	void WaitForIdleGPU();


	ID3D12Device* GetD3D12Device() const					{ return device.Get(); }
	ID3D12CommandQueue* GetDirectCommandQueue() const		{ return commandQueue.Get(); }
	unsigned int GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE type) { return descriptorSize[type]; }

	/// Returns the currently targeted swap chain buffer (= "backbuffer")
	ID3D12Resource* GetCurrentSwapChainBuffer()				{ return backbufferRenderTargets[activeSwapChainBufferIndex].Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentSwapChainBufferRTVDesc();



	// There are two Syncs in the pipeline: CPU -> GPU and GPU -> Screen. 

	/// How many backbuffers are there (important for GPU -> Screen synchronization).
	/// The more, the more frames can the GPU prepare that will later be shown by the screen.
	/// (Needs to be bigger than 0)
	static const unsigned int SWAPCHAINBUFFERCOUNT = 3;
	/// How many frames can be maximal in-flight (important from CPU -> GPU synchronization)
	/// The more, the more command-queues can the CPU prepare that will executed / rendered to the next backbuffer.
	/// (Needs to be bigger than 0)
	static const unsigned int MAX_FRAMES_INFLIGHT = 3; 

private:
	void SignalFrameFence();

	unsigned int activeSwapChainBufferIndex; ///< The backbuffer/swapchainbuffer index on which the GPU currently works.

	ComPtr<ID3D12Device> device;
	ComPtr<IDXGISwapChain3> swapChain;
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12CommandAllocator> commandAllocator;

	ComPtr<ID3D12Resource> backbufferRenderTargets[SWAPCHAINBUFFERCOUNT]; ///< Resource interface to swap chain resources.
	ComPtr<ID3D12DescriptorHeap> backbufferDescriptorHeap;
	ComPtr<ID3D12RootSignature> rootSignature;
	
	unsigned int descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	ComPtr<ID3D12Fence> frameFence;
	UINT64 frameFenceValue; ///< Value of the last signal given to the frameFence. This is not a frame counter since it is also used otherwise!
	HANDLE fenceEvent;

	bool vsync;
};

