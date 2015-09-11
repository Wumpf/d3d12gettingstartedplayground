#include "Application.h"
#include "Window.h"
#include "D3D12Device.h"

#include "d3dx12.h"
#include "Helper.h"

#include <d3dcompiler.h>
#include <chrono>

Application::Application() :
	window(new Window(1280, 720, L"testerata!")),
	device(new D3D12Device(*window)),
	frameQueueIndex(0)
{
	CreateRootSignature();
	CreatePSO();
	CreateVertexBuffer();

	
	// Create a command allocator for every inflight-frame
	for (int i = 0; i < D3D12Device::MAX_FRAMES_INFLIGHT; ++i)
	{
		if (FAILED(device->GetD3D12Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator[i]))))
			CRITICAL_ERROR("Failed to create command list allocator.");
	}
	// Create the command list.
	if (FAILED(device->GetD3D12Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator[frameQueueIndex].Get(), pso.Get(), IID_PPV_ARGS(&commandList))))
		CRITICAL_ERROR("Failed to create command list");

	// Command lists are created in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.
	commandList->Close();

	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(window->GetWidth());
	viewport.Height = static_cast<float>(window->GetHeight());
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = static_cast<LONG>(window->GetWidth());
	scissorRect.bottom = static_cast<LONG>(window->GetHeight());
}


Application::~Application()
{
}

void Application::CreateRootSignature()
{	
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.NumParameters = 0;
	rootSignatureDesc.pParameters = nullptr;
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignatureDesc.pStaticSamplers = nullptr;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
		CRITICAL_ERROR("Failed to serialize root signature");
	if (FAILED(device->GetD3D12Device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature))))
		CRITICAL_ERROR("Failed to createroot signature");
}

void Application::CreatePSO()
{
	ComPtr<ID3DBlob> vertexShader, pixelShader;

#ifdef _DEBUG
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	ID3D10Blob* errorMessages;
	if (FAILED(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errorMessages)))
	{
		if (errorMessages)
		{
			std::cout << static_cast<char*>(errorMessages->GetBufferPointer()) << std::endl;
			errorMessages->Release();
		}
		if (errorMessages)
			errorMessages->Release();

		CRITICAL_ERROR("Failed to compile vertex shader.");
	}
	if (FAILED(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errorMessages)))
	{
		if (errorMessages)
		{
			std::cout << static_cast<char*>(errorMessages->GetBufferPointer()) << std::endl;
			errorMessages->Release();
		}
		if (errorMessages)
			errorMessages->Release();

		CRITICAL_ERROR("Failed to compile pixel shader.");
	}

	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.VS = { reinterpret_cast<UINT8*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
	psoDesc.PS = { reinterpret_cast<UINT8*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
	psoDesc.DS;
	psoDesc.HS;
	psoDesc.GS;
	psoDesc.StreamOutput;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.CachedPSO; // TODO: Need to learn about this.
	
	if(FAILED(device->GetD3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso))))
		CRITICAL_ERROR("Failed to PSO.");
}

void Application::CreateVertexBuffer()
{
	struct Vertex
	{
		float position[2];
		float color[4];
	};

	// Define the geometry for a triangle.
	Vertex triangleVertices[] =
	{
		{ { -0.75f, -0.75f },{ 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { -1.0f, -1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, -1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } }
	};

	const unsigned int vertexBufferSize = sizeof(triangleVertices);

	// TODO: Do not use upload heap.
	// Note: using upload heaps to transfer static data like vert buffers is not 
	// recommended. Every time the GPU needs it, the upload heap will be marshalled 
	// over. Please read up on Default Heap usage. An upload heap is used here for 
	// code simplicity and because there are very few verts to actually transfer.
	if (FAILED(device->GetD3D12Device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&vertexBuffer))))
	{
		CRITICAL_ERROR("Failed to create vertex buffer");
	}

	// Copy the triangle data to the vertex buffer.
	UINT8* pVertexDataBegin;
	if (FAILED(vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pVertexDataBegin))))
		CRITICAL_ERROR("Failed to map vertex buffer");
	memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
	vertexBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	vertexBufferView.SizeInBytes = vertexBufferSize;
}

void Application::PopulateCommandList()
{
	// Should be completely save now to reset this command allocator.
	if (FAILED(commandAllocator[frameQueueIndex]->Reset()))
		CRITICAL_ERROR("Failed to reset the command allocator.");
	// Restart command queue with new command allocator (last frame a different was used)
	if(FAILED(commandList->Reset(commandAllocator[frameQueueIndex].Get(), pso.Get())))
		CRITICAL_ERROR("Failed to reset the command list.");

	// Set necessary state.
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	// Indicate that the back buffer will be used as a render target.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(device->GetCurrentSwapChainBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	auto rtvDesc = device->GetCurrentSwapChainBufferRTVDesc();
	commandList->OMSetRenderTargets(1, &rtvDesc, FALSE, nullptr);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	commandList->ClearRenderTargetView(rtvDesc, clearColor, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->DrawInstanced(3, 100, 0, 0);

	// Indicate that the back buffer will now be used to present.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(device->GetCurrentSwapChainBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	if (FAILED(commandList->Close()))
		CRITICAL_ERROR("Failed to close the command list.");
}

void Application::Update(float lastFrameTimeInSeconds)
{
}

void Application::Render()
{
	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
	device->GetDirectCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	device->Present();

	device->WaitForFreeInflightFrame();
	frameQueueIndex = (frameQueueIndex + 1) % D3D12Device::MAX_FRAMES_INFLIGHT;
}

void Application::Run()
{
	auto messageFunc = std::bind(&Application::OnWindowMessage, this, std::placeholders::_1);

	running = true;
	float lastFrameTimeInSeconds = 0.0f;

	while (running)
	{
		auto begin = std::chrono::high_resolution_clock::now(); // should be as good as QueryPerformanceCounter in VS2015
		
		window->ReceiveMessages(messageFunc);
		Update(lastFrameTimeInSeconds);
		Render();

		auto end = std::chrono::high_resolution_clock::now();
		long long duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
		lastFrameTimeInSeconds = static_cast<float>(duration / 1000.0 / 1000.0 / 1000.0);
		window->SetCaption(std::to_wstring(device->GetNumFramesInFlight()) + L" frames in-flight --- " + 
			std::to_wstring(duration / 1000.0 / 1000.0) + L" ms -- " + std::to_wstring(1.0f / lastFrameTimeInSeconds) + L" fps");
	}
}

void Application::OnWindowMessage(MSG message)
{
	if (message.message == WM_QUIT)
		running = false;
}