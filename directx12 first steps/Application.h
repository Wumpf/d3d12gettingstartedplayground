#pragma once

#include <memory>
#include <Windows.h>

#include <d3d12.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

class Window;
class D3D12Device;

class Application
{
public:
	Application();
	~Application();

	void Update();
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
	ComPtr<ID3D12GraphicsCommandList> commandList;

	ComPtr<ID3D12Resource> vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;


	bool running;
};

