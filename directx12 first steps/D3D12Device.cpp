#include "D3D12Device.h"
#include "Window.h"

#include <iostream>

#include <dxgi1_4.h>
#include "d3dx12.h"

#include "Helper.h"

D3D12Device::D3D12Device(Window& window) : gpuFrameIndex(0)
{
#ifdef D3DDEBUG
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}
#endif

	// Actual device.
	if(FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
		CRITICAL_ERROR("Failed to create D3D12 device");

	// Get Descriptor sizes
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		descriptorSize[i] = device->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));

	// Main command queue.
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue))))
			CRITICAL_ERROR("Failed to create D3D12 commandqueue");
	}

	// Create command allocator.
	if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator))))
		CRITICAL_ERROR("Failed to create command list allocator.");

	// Describe and create the swap chain.
	{
		ComPtr<IDXGIFactory4> factory;
		if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
			CRITICAL_ERROR("Failed to create DXGI factory");

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferCount = BACKBUFFERCOUNT;
		swapChainDesc.BufferDesc.Width = window.GetWidth();
		swapChainDesc.BufferDesc.Height = window.GetWidth();
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.OutputWindow = window.GetHandle();
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.Windowed = TRUE;

		ComPtr<IDXGISwapChain> localSwapChain;
		if (FAILED(factory->CreateSwapChain(
			commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
			&swapChainDesc,
			&localSwapChain)) ||
			FAILED(localSwapChain.As(&swapChain)))
		{
			CRITICAL_ERROR("Failed to create swap chain");
		}

		gpuFrameIndex = swapChain->GetCurrentBackBufferIndex();
	}

	// Create descriptor heap for backbuffer.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = BACKBUFFERCOUNT;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		if(FAILED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&backbufferDescriptorHeap))))
			CRITICAL_ERROR("Failed to create descriptor heap for backbuffer descriptors.");
	}

	// Create render target view for backbuffer.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(backbufferDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (int i = 0; i < BACKBUFFERCOUNT; ++i)
		{
			if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&backbufferRenderTargets[i]))))
				CRITICAL_ERROR("Failed to retrieve ID3D12Resource from swapchain buffer.");

			device->CreateRenderTargetView(backbufferRenderTargets[i].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]);
		}
	}

	// Create Fence
	{
		if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
			CRITICAL_ERROR("Failed to create fence!");
		fenceValue = 1;

		// Create an event handle to use for frame synchronization.
		fenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
		if (fenceEvent == nullptr)
		{
			CRITICAL_ERROR("Failed to create event " << HRESULT_FROM_WIN32(GetLastError()));
		}
	}
}

D3D12Device::~D3D12Device()
{
	CloseHandle(fenceEvent);
}

void D3D12Device::Present()
{
	swapChain->Present(1, 0);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Device::GetCurrentBackBufferRTVDesc()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(backbufferDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), gpuFrameIndex, descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]);
}

void D3D12Device::WaitForPreviousFrame()
{
	// TODO TODO TODO !!!
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. More advanced samples 
	// illustrate how to use fences for efficient resource usage.

	// Signal and increment the fence value.
	const UINT64 oldFenceValue = fenceValue;
	if (FAILED(commandQueue->Signal(fence.Get(), oldFenceValue)))
		CRITICAL_ERROR("Failed to signal command queue.");
	fenceValue++;

	// Wait until the previous frame is finished.
	if (fence->GetCompletedValue() < oldFenceValue)
	{
		if (FAILED(fence->SetEventOnCompletion(oldFenceValue, fenceEvent)))
			CRITICAL_ERROR("Failed to set event for fence completion.");
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	gpuFrameIndex = swapChain->GetCurrentBackBufferIndex();
}