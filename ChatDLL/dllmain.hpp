#pragma once

#ifdef CHATLIBRARY_EXPORTS
#define CHATLIBRARY_API __declspec(dllexport)
#else
#define CHATLIBRARY_API __declspec(dllimport)
#endif

#include <Windows.h>
#include <iostream>
#include <mutex>
#include <memory>
#include <condition_variable>

static std::mutex condMutex;
static std::condition_variable cv;

#define IDC_BUTTON 34567
#define IDC_DIALOG 999
#define IDC_TEXTBOX 789

class Chat
{
public:
	Chat(HINSTANCE hInstance, int cmdShow, std::wstring title)
	{
		if (AllocConsole())
		{
			FILE* pCout;
			freopen_s(&pCout, "CONOUT$", "w", stdout);
			SetConsoleTitle(L"Debug Console");
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED);
		}


		const wchar_t className[] = L"WinApp";
		//wchar_t windowTitle[] = title;
		WNDCLASSEX wcex;

		wcex.cbClsExtra = 0;
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.cbWndExtra = 0;
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
		wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
		wcex.hInstance = hInstance;
		wcex.lpfnWndProc = WndProc;
		wcex.lpszClassName = className;
		wcex.lpszMenuName = NULL;
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		if (!RegisterClassEx(&wcex))
		{
			MessageBox(NULL, TEXT("RegisterClassEx Failed!"), TEXT("Error"), MB_ICONERROR);
			exit(-1);
		}

		if (!(parentHwnd = CreateWindow(className, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL)))
		{
			MessageBox(NULL, TEXT("CreateWindow Failed!"), TEXT("Error"), MB_ICONERROR);
			exit(-1);
		}

		HWND dialogField = CreateWindowW(L"Edit", L"Not Connected", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_READONLY,
			50, 10, 500, 500, parentHwnd, (HMENU)IDC_DIALOG, NULL, NULL);

		HWND hwndEdit = CreateWindowW(L"Edit", L"test", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
			570, 20, 300, 50, parentHwnd, (HMENU)IDC_TEXTBOX, NULL, NULL);

		HWND button = CreateWindow(L"Button", L"Send", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 600, 80, 60, 30,
			parentHwnd, (HMENU)IDC_BUTTON, (HINSTANCE)GetWindowLongPtr(parentHwnd, GWLP_HINSTANCE), NULL);

		std::cout << "Chat has been inited" << std::endl;

		ShowWindow(parentHwnd, cmdShow);
		ShowWindow(GetConsoleWindow(), SW_SHOW);
		UpdateWindow(parentHwnd);
	}

	void Start()
	{
		std::cout << "Chat has been started" << std::endl;
		MSG msg;

		while (GetMessage(&msg, NULL, 0, 0) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	static void AddStrToDialog(HWND& hwnd, std::wstring inputStr, std::wstring owner)
	{
		wchar_t dialogText[1024];

		HWND dialogBox = GetDlgItem(hwnd, IDC_DIALOG);
		GetWindowText(dialogBox, dialogText, 1024);

		std::wstring dialogStr(dialogText);

		std::wstring res = dialogStr + L"\r\n" + owner + L": " + inputStr;
		SetWindowText(dialogBox, res.c_str());
	}

	HWND& GetHwnd()
	{
		return parentHwnd;
	}

	std::wstring GetEnteredText()
	{
		return enteredText;
	}

	bool GetIsConnected()
	{
		return isConnected;
	}

	void SetIsConnected(bool value)
	{
		isConnected = value;
	}

	void AddSystemText(std::string str)
	{
		wchar_t dialogText[1024];

		HWND dialogBox = GetDlgItem(parentHwnd, IDC_DIALOG);

		GetWindowTextW(dialogBox, dialogText, 1024);

		std::wstring newStr(str.begin(), str.end());
		std::wstring res = std::wstring(dialogText) + L"\r\n" + newStr;

		SetWindowText(dialogBox, res.c_str());
	}

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_DESTROY:
			PostQuitMessage(EXIT_SUCCESS);
			break;

		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
			case IDC_BUTTON:
				if (!isConnected)
					break;

				wchar_t inputText[512];

				HWND textBox = GetDlgItem(hwnd, IDC_TEXTBOX);

				GetWindowTextW(textBox, inputText, 512);

				SetWindowText(textBox, L"");
				SetFocus(textBox);

				std::wstring inputStr(inputText);
				enteredText = inputStr;

				AddStrToDialog(hwnd, inputStr, L"You");

				//Send text to the server
				std::unique_lock<std::mutex> lock(condMutex);
				cv.notify_all();

				break;
			}
		}
		break;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
		return FALSE;
	}

	static std::wstring enteredText;
	std::mutex strMutex;
	HWND parentHwnd;
	static bool isConnected;
};

std::wstring Chat::enteredText;
bool Chat::isConnected = false;