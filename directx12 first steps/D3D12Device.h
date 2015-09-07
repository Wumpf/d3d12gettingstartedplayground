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

	void Present();
	void WaitForPreviousFrame();


	ID3D12Device* GetD3D12Device()						{ return device.Get(); }
	ID3D12CommandQueue* GetRenderQueue()				{ return commandQueue.Get(); }
	ID3D12CommandAllocator* GetDirectCommandAllocator() { return commandAllocator.Get(); }
	ID3D12Resource* GetCurrentBackBuffer()				{ return backbufferRenderTargets[gpuFrameIndex].Get(); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferRTVDesc();


private:
	static const unsigned int BACKBUFFERCOUNT = 3;
	unsigned int gpuFrameIndex; ///< The frame/backbuffer index on which the GPU currently works.

	ComPtr<ID3D12Device> device;
	ComPtr<IDXGISwapChain3> swapChain;
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12CommandAllocator> commandAllocator;

	ComPtr<ID3D12Resource> backbufferRenderTargets[BACKBUFFERCOUNT]; ///< Resource interface to swap chain resources.
	ComPtr<ID3D12DescriptorHeap> backbufferDescriptorHeap;
	ComPtr<ID3D12RootSignature> rootSignature;
	
	unsigned int descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];


	ComPtr<ID3D12Fence> fence;
	UINT64 fenceValue;
	HANDLE fenceEvent;
};

