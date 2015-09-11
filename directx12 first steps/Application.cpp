#include "Application.h"
#include "Window.h"
#include "D3D12Device.h"

#include "d3dx12.h"
#include "Helper.h"

#include <d3dcompiler.h>
#include <chrono>

namespace
{
	void OutputDXError(ID3DBlob* errorMessages)
	{
		if (errorMessages)
		{
			std::cout << static_cast<char*>(errorMessages->GetBufferPointer()) << std::endl;
			errorMessages->Release();
		}
		if (errorMessages)
			errorMessages->Release();
	}
}

Application::Application() :
	window(new Window(1280, 720, L"testerata!")),
	device(new D3D12Device(*window)),
	frameQueueIndex(0)
{
	// Create a command allocator for every inflight-frame
	for (int i = 0; i < D3D12Device::MAX_FRAMES_INFLIGHT; ++i)
	{
		if (FAILED(device->GetD3D12Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator[i]))))
			CRITICAL_ERROR("Failed to create command list allocator.");
	}
	// Create the command list.
	if (FAILED(device->GetD3D12Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator[frameQueueIndex].Get(), pso.Get(), IID_PPV_ARGS(&commandList))))
		CRITICAL_ERROR("Failed to create command list");
	commandList->Close();

	CreateRootSignature();
	CreatePSO();
	CreateVertexBuffer();
	CreateTextures();

	// Configure viewport and scissor rect.
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
	D3D12_DESCRIPTOR_RANGE ranges[1];
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].RegisterSpace = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	//D3DX: ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);


	D3D12_ROOT_PARAMETER rootParameters[2];
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // Would also be possible to create a shader resource view right away, since we have only one texture. But for practicing we use a table.
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[0].DescriptorTable.pDescriptorRanges = ranges;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[1].Constants.ShaderRegister = 0;
	rootParameters[1].Constants.RegisterSpace = 0;
	rootParameters[1].Constants.Num32BitValues = 1;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;


	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.NumParameters = 2;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignatureDesc.pStaticSamplers = &sampler;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
	{
		OutputDXError(error.Get());
		CRITICAL_ERROR("Failed to serialize root signature.");
	}
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
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
	psoDesc.CachedPSO;
	
	if(FAILED(device->GetD3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso))))
		CRITICAL_ERROR("Failed to PSO.");
}

void Application::CreateVertexBuffer()
{
	struct Vertex
	{
		float position[2];
		float texcoord[2];
	};

	// Define the geometry for a quad.
	float screenAspectRatio = static_cast<float>(window->GetWidth()) / window->GetHeight();
	Vertex quadVertices[] =
	{
		{ { -0.25f, -0.25f * screenAspectRatio }, { 0.0f, 0.0f } },
		{ { -0.25f, 0.25f * screenAspectRatio },{ 0.0f, 1.0f } },
		{ { 0.25f, -0.25f * screenAspectRatio },{ 1.0f, 0.0f } },
		{ { 0.25f, 0.25f * screenAspectRatio },{ 1.0f, 1.0f } }
	};

	const unsigned int vertexBufferSize = sizeof(quadVertices);

	// Create an upload heap for the
	ComPtr<ID3D12Resource> uploadHeap;
	if (FAILED(device->GetD3D12Device()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
																&CD3DX12_RESOURCE_DESC::Buffer(sizeof(quadVertices)), D3D12_RESOURCE_STATE_GENERIC_READ, // D3D12_RESOURCE_STATE_GENERIC_READ is the only possible for D3D12_HEAP_TYPE_UPLOAD.
																nullptr, IID_PPV_ARGS(&uploadHeap))))
	{
		CRITICAL_ERROR("Failed to create upload heap.");
	}

	// Copy the triangle data to the vertex buffer.
	UINT8* pVertexDataBegin;
	if (FAILED(uploadHeap->Map(0, nullptr, reinterpret_cast<void**>(&pVertexDataBegin))))
		CRITICAL_ERROR("Failed to map vertex buffer");
	memcpy(pVertexDataBegin, quadVertices, sizeof(quadVertices));
	uploadHeap->Unmap(0, nullptr);

	// Create vertex buffer.
	if (FAILED(device->GetD3D12Device()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
																&CD3DX12_RESOURCE_DESC::Buffer(sizeof(quadVertices)), D3D12_RESOURCE_STATE_COPY_DEST,
																nullptr, IID_PPV_ARGS(&vertexBuffer))))
	{
		CRITICAL_ERROR("Failed to create vertex buffer.");
	}

	// Copy over and wait until its done.
	commandList->Reset(commandAllocator[0].Get(), nullptr);
	commandList->CopyResource(vertexBuffer.Get(), uploadHeap.Get());
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
	commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
	device->GetDirectCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	device->WaitForIdleGPU();

	// Initialize the vertex buffer view.
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	vertexBufferView.SizeInBytes = vertexBufferSize;
}

void Application::CreateTextures()
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc;
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc.NumDescriptors = numTextures;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptorHeapDesc.NodeMask = 1;
	if (FAILED(device->GetD3D12Device()->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&textureDescriptorHeap))))
		CRITICAL_ERROR("Failed to create texture descriptor heap.");
	auto descriptorHandle = textureDescriptorHeap->GetCPUDescriptorHandleForHeapStart();


	// Would also be easy possible to place all textures into the same heap, but I do not see the advantes of that yet.

	const unsigned int textureSize = 16;
	char textureData[textureSize * textureSize * 4];

	// Texture desc, used by all textures
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Width = textureSize;
	textureDesc.Height = textureSize;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;


	ComPtr<ID3D12Resource> textureUploadHeap;
	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = textureData;
	subresourceData.RowPitch = textureSize * 4;
	subresourceData.SlicePitch = subresourceData.RowPitch * textureSize;

	// Create the texture.
	for (unsigned int tex = 0; tex < numTextures; ++tex)
	{
		// Create texture
		if (FAILED(device->GetD3D12Device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
			&textureDesc, D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr, IID_PPV_ARGS(&textures[tex]))))
		{
			CRITICAL_ERROR("Failed to create texture");
		}

		// Create Upload buffer. Creating it outside the loop would make it impossible to use the GetRequiredIntermediateSize helper function
		if (tex == 0)
		{
			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(textures[0].Get(), 0, 1);
			if (FAILED(device->GetD3D12Device()->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ, // D3D12_RESOURCE_STATE_GENERIC_READ is the only possible for D3D12_HEAP_TYPE_UPLOAD.
				nullptr,
				IID_PPV_ARGS(&textureUploadHeap))))
			{
				CRITICAL_ERROR("Failed to create upload heap for textures");
			}
		}


		// Create the GPU upload buffer.
		for (unsigned int j = 0; j < sizeof(textureData); ++j)
			textureData[j] = static_cast<unsigned char>(rand() % 255);

		// Copy over and wait until its done.
		commandList->Reset(commandAllocator[0].Get(), nullptr);
		//commandList->CopyResource(textures[i].Get(), textureUploadHeap.Get());
		UpdateSubresources(commandList.Get(), textures[tex].Get(), textureUploadHeap.Get(), 0, 0, 1, &subresourceData);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(textures[tex].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		commandList->Close();
		ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
		device->GetDirectCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		device->WaitForIdleGPU();

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->GetD3D12Device()->CreateShaderResourceView(textures[tex].Get(), &srvDesc, descriptorHandle);

		descriptorHandle.ptr += device->GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
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
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	ID3D12DescriptorHeap* descriptorHeaps[] = { textureDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	auto textureDescHandle = textureDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	for (int i = 0; i < numTextures; ++i)
	{
		commandList->SetGraphicsRootDescriptorTable(0, textureDescHandle);
		commandList->SetGraphicsRoot32BitConstant(1, i, 0);
		commandList->DrawInstanced(4, 1, 0, 0);
		textureDescHandle.ptr += device->GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

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