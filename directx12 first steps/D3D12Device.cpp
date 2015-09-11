#include "D3D12Device.h"
#include "Window.h"

#include <iostream>

#include <dxgi1_4.h>
#include "d3dx12.h"

#include "Helper.h"

D3D12Device::D3D12Device(Window& window) : activeSwapChainBufferIndex(0), vsync(false)
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

	// Describe and create the swap chain.
	{
		ComPtr<IDXGIFactory4> factory;
		if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
			CRITICAL_ERROR("Failed to create DXGI factory");

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferCount = SWAPCHAINBUFFERCOUNT;
		swapChainDesc.BufferDesc.Width = window.GetWidth();
		swapChainDesc.BufferDesc.Height = window.GetHeight();
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

		activeSwapChainBufferIndex = swapChain->GetCurrentBackBufferIndex();
	}

	// Create descriptor heap for backbuffer.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = SWAPCHAINBUFFERCOUNT;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		if(FAILED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&backbufferDescriptorHeap))))
			CRITICAL_ERROR("Failed to create descriptor heap for backbuffer descriptors.");
	}

	// Create render target view for backbuffer.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(backbufferDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (int i = 0; i < SWAPCHAINBUFFERCOUNT; ++i)
		{
			if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&backbufferRenderTargets[i]))))
				CRITICAL_ERROR("Failed to retrieve ID3D12Resource from swapchain buffer.");

			device->CreateRenderTargetView(backbufferRenderTargets[i].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]);
		}
	}

	// Create Fence
	{
		frameFenceValue = 0;
		if (FAILED(device->CreateFence(frameFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frameFence))))
			CRITICAL_ERROR("Failed to create frameFence!");

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
	WaitForIdleGPU();
	CloseHandle(fenceEvent);
}

void D3D12Device::Present()
{
	swapChain->Present(vsync ? 1 : 0, 0);
	SignalFrameFence();
}

unsigned int D3D12Device::GetNumFramesInFlight()
{
	return static_cast<unsigned int>(frameFenceValue - frameFence->GetCompletedValue());
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Device::GetCurrentSwapChainBufferRTVDesc()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(backbufferDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), activeSwapChainBufferIndex, descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]);
}

void D3D12Device::SignalFrameFence()
{
	// Signal and increment the frameFence value.
	frameFenceValue++;
	if (FAILED(commandQueue->Signal(frameFence.Get(), frameFenceValue)))
		CRITICAL_ERROR("Failed to signal command queue.");
}

void D3D12Device::WaitForFreeInflightFrame()
{
	if (GetNumFramesInFlight() >= MAX_FRAMES_INFLIGHT)
	{
		if (FAILED(frameFence->SetEventOnCompletion(frameFenceValue - MAX_FRAMES_INFLIGHT + 1, fenceEvent)))
			CRITICAL_ERROR("Failed to set event for WaitForFreeInflightFrame.");
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	activeSwapChainBufferIndex = swapChain->GetCurrentBackBufferIndex();
}

void D3D12Device::WaitForIdleGPU()
{
	// Adds a signal to the frame-fence to ensure that all operations so fare are completed.
	SignalFrameFence();

	// Wait until all inflight frames are finished.
	if (frameFence->GetCompletedValue() < frameFenceValue)
	{
		if (FAILED(frameFence->SetEventOnCompletion(frameFenceValue, fenceEvent)))
			CRITICAL_ERROR("Failed to set event for WaitForIdleGPU.");
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	activeSwapChainBufferIndex = swapChain->GetCurrentBackBufferIndex();
}