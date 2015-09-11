#pragma once

#include <memory>
#include <Windows.h>
#include "D3D12Device.h"


class Window;
class D3D12Device;

class Application
{
public:
	Application();
	~Application();

	void Update(float lastFrameTimeInSeconds);
	void Render();

	void Run();

private:
	void CreateRootSignature();
	void CreatePSO();
	void CreateVertexBuffer();

	void PopulateCommandList();

	void OnWindowMessage(MSG message);

	std::unique_ptr<Window> window;
	std::unique_ptr<D3D12Device> device;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pso;

	unsigned int frameQueueIndex;
	ComPtr<ID3D12CommandAllocator> commandAllocator[D3D12Device::MAX_FRAMES_INFLIGHT];
	ComPtr<ID3D12GraphicsCommandList> commandList;

	ComPtr<ID3D12Resource> vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;


	bool running;
};

