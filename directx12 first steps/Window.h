#pragma once

#include <string>
#include <functional>
#include <Windows.h>

class Window
{
public:
	Window(unsigned int width, unsigned int height, const std::wstring& name);
	~Window();

	void SetCaption(const std::wstring& caption);
	void ReceiveMessages(const std::function<void(MSG msg)>& onMessage);

	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }
	HWND GetHandle() const { return windowHandle; }


private:
	void InitializeWindow();

	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	unsigned int width;
	unsigned int height;
	std::wstring title;

	HWND windowHandle;
};

