#define WIN32_LEAN_AND_MEAN
#undef UNICODE

#include <iostream>
#include <string>
#include <queue>
#include <thread>
#include <condition_variable>
#include <mutex>

#include <Windows.h>
#include <windowsx.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "Ws2_32.lib")

#define BTN_MODE 1
#define BTN_START 2
#define BTN_PAUSE 3
#define BTN_TERMINATE 4
#define BTN_CONNECT 5
#define BTN_DISCONNECT 6
#define EDIT_ADRESS 7
#define BTN_SERVER 8
#define BTN_CLIENT 9

#define MENU_FILE 10
#define MENU_SUB 11
#define MENU_EXIT 12
#define MENU_ABOUT 13

#define DEFAULT_PORT 27015
#define MAX_CLIENTS 1

const int iport = DEFAULT_PORT;

// Server screen dim is nScreenWidth[0], nScreenHeight[0]
// Client screen dim is nScreenWidth[1], nScreenHeight[1]
const int nScreenWidth[2] = { 1920 , 2560};
const int nScreenHeight[2] = { 1080 , 1440};


const int nNormalized = 65535;





// ================================================
// =================WINDOWS SOCKETS================
// ================================================
// code largely taken from
// https://docs.microsoft.com/en-us/windows/win32/winsock/complete-server-code
// https://docs.microsoft.com/en-us/windows/win32/winsock/complete-client-code
int InitializeServer(SOCKET& sktListen, int port) {
	WSADATA wsadata;
	int iResult;

	//Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsadata);


	if (iResult != 0) {
		std::cout << "WSAStartup failed: " << iResult << std::endl;;
		return 1;
	}

	struct addrinfo* result = NULL;
	struct addrinfo hints;

	ZeroMemory(&hints, sizeof(hints));

	//AF_INET for IPV4
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	//Resolve the local address and port to be used by the server
	iResult = getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &result);


	if (iResult != 0) {
		std::cout << "getaddrinfo failed: " << iResult << std::endl;
		WSACleanup();
		return 1;
	}

	//Create a SOCKET for the server to listen for client connections

	sktListen = socket(result->ai_family, result->ai_socktype, result->ai_protocol);


	if (sktListen == INVALID_SOCKET) {
		std::cout << "Error at socket(): " << WSAGetLastError() << std::endl;
		WSACleanup();
		freeaddrinfo(result);
		return 1;
	}

	//Setup the TCP listening socket
	iResult = bind(sktListen, result->ai_addr, (int)result->ai_addrlen);


	if (iResult == SOCKET_ERROR) {
		std::cout << "bind failed with error: " << WSAGetLastError() << std::endl;
		WSACleanup();
		freeaddrinfo(result);
		return 1;
	}

	freeaddrinfo(result);
	return 0;
}

int BroadcastInput(std::vector<SOCKET> vsktSend, INPUT* input) {
	int iResult = 0;

	for (auto& sktSend: vsktSend) {
		if (sktSend != INVALID_SOCKET) {

			iResult = send(sktSend, (char*)input, sizeof(INPUT), 0);
			if (iResult == SOCKET_ERROR) {
				std::cout << "send failed: " << WSAGetLastError() << std::endl;
			}
		}
	}
	return 0;
}

int TerminateServer(SOCKET& sktListen, std::vector<SOCKET>& sktClients) {

	int iResult;
	for (auto& client: sktClients) {
		if (client != INVALID_SOCKET) {
			iResult = shutdown(client, SD_SEND);
			if (iResult == SOCKET_ERROR) {
				std::cout << "shutdown failed: " << WSAGetLastError() << std::endl;
			}
			closesocket(client);
		}
	}
	closesocket(sktListen);
	WSACleanup();
	return 0;
}

int InitializeClient() {
	WSADATA wsaData;
	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		std::cout << "WSAStartup failed with error: " << iResult << std::endl;
		return 1;
	}
	return 0;
}

int ConnectServer(SOCKET& sktConn, std::string serverAdd, int port) {
	int iResult;
	struct addrinfo* result = NULL,
		* ptr = NULL,
		hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(serverAdd.c_str(), std::to_string(port).c_str(), &hints, &result);
	if (iResult != 0) {
		std::cout << "getaddrinfo failed with error: " << iResult << std::endl;
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	sktConn = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (sktConn == INVALID_SOCKET) {
		std::cout << "socket failed with error: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return 1;
	}

	// Connect to server.
	iResult = connect(sktConn, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		closesocket(sktConn);
		sktConn = INVALID_SOCKET;
	}

	freeaddrinfo(result);

	if (sktConn == INVALID_SOCKET) {
		std::cout << "Unable to connect to server" << std::endl;
		WSACleanup();
		return 1;
	}
	return 0;
}

int ReceiveServer(SOCKET sktConn, INPUT& data) {
	INPUT* buff = new INPUT;
	//std::cout << "Receiving..." << std::endl;
	int iResult = recv(sktConn, (char*)buff, sizeof(INPUT), 0);
	if (iResult == sizeof(INPUT)) 
	{
		//std::cout << "Bytes received: " << iResult << std::endl;
	}
	else if (iResult == 0) {
		std::cout << "Connection closed" << std::endl;
		delete buff;
		return 1;
	}
	else if (iResult < sizeof(INPUT))
	{
		int bytes_rec = iResult;
		int count = 0;
		while (bytes_rec < sizeof(INPUT))
		{
			std::cout << "Received partial input: " << count << " - "<< bytes_rec << " bytes of " << sizeof(INPUT) << std::endl;
			bytes_rec += recv(sktConn, (char*)buff + bytes_rec, sizeof(INPUT) - bytes_rec, 0);
			count++;
		}
	}
	else {
		std::cout << "Receive failed with error: " << WSAGetLastError() << std::endl;
		delete buff;
		return 1;
	}
	data = *buff;
	delete buff;
	return 0;
}

int CloseConnection(SOCKET* sktConn) {
	closesocket(*sktConn);
	WSACleanup();
	return 0;
}


// BaseWindow was taken from
// https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/begin/LearnWin32/BaseWindow/cpp/main.cpp
// Slightly modified it by removing the template
class BaseWindow
{
public:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		BaseWindow* pThis = NULL;

		if (uMsg == WM_NCCREATE)
		{
			CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
			pThis = (BaseWindow*)pCreate->lpCreateParams;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);

			pThis->m_hwnd = hwnd;
		}
		else
		{
			pThis = (BaseWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		}
		if (pThis)
		{
			return pThis->HandleMessage(uMsg, wParam, lParam);
		}
		else
		{
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	}

	BaseWindow(): m_hwnd(NULL), m_pParent(nullptr) { }

	BOOL Create(
		BaseWindow* parent,
		PCSTR lpWindowName,
		DWORD dwStyle,
		DWORD dwExStyle = 0,
		int x = CW_USEDEFAULT,
		int y = CW_USEDEFAULT,
		int nWidth = CW_USEDEFAULT,
		int nHeight = CW_USEDEFAULT,
		HWND hWndParent = 0,
		HMENU hMenu = NULL
	)
	{
		if (hWndParent == 0) {
			WNDCLASS wc = { 0 };

			wc.lpfnWndProc = BaseWindow::WindowProc;
			wc.hInstance = GetModuleHandle(NULL);
			wc.lpszClassName = ClassName();

			RegisterClass(&wc);
		}

		m_pParent = parent;

		m_hwnd = CreateWindowExA(
			dwExStyle, ClassName(), lpWindowName, dwStyle, x, y,
			nWidth, nHeight, hWndParent, hMenu, GetModuleHandle(NULL), this
		);

		return (m_hwnd ? TRUE: FALSE);
	}

	HWND Window() const { return m_hwnd; }

protected:

	virtual LPCSTR ClassName() const = 0;
	virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;

	HWND m_hwnd;
	BaseWindow* m_pParent;
};

class Button: public BaseWindow
{
public:
    LPCSTR ClassName() const { return "button"; }
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) { return DefWindowProc(m_hwnd, uMsg, wParam, lParam); }
};

class InputBox: public BaseWindow
{
public:
    LPCSTR ClassName() const { return "edit"; }
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) { return DefWindowProc(m_hwnd, uMsg, wParam, lParam); }
};

class StaticBox: public BaseWindow
{
public:
    LPCSTR ClassName() const { return "static"; }
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) { return DefWindowProc(m_hwnd, uMsg, wParam, lParam); }
};

class EditBox: public BaseWindow
{
public:
	LPCSTR ClassName() const { return "static"; }
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) { return DefWindowProc(m_hwnd, uMsg, wParam, lParam); }
};

class MainWindow: public BaseWindow
{
public:
    LPCSTR ClassName() const { return "Remote Window Class"; }
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

public:

	enum MODE
	{
		SERVER,
		CLIENT,
		UNDEF,
	};

	int InitializeInputDevice();
	void UpdateInput();
	void ConvertInput(PRAWINPUT pRaw, INPUT* pInput);
	int RetrieveInput(UINT uMsg, WPARAM wParam, LPARAM lParam);
	int SetMode(MODE m);
	void UpdateGuiControls();

	int ServerStart(int nPort = DEFAULT_PORT);
	int ServerTerminate();

	int ClientConnect();
	int ClientDisconnect();

	int HandleCreate(UINT uMsg, WPARAM wParam, LPARAM lParam);
	int HandlePaint(UINT uMsg, WPARAM wParam, LPARAM lParam);
	int HandleCommand(UINT uMsg, WPARAM wParam, LPARAM lParam);
	int HandleClose(UINT uMsg, WPARAM wParam, LPARAM lParam);

	int SendThread(); // thread that sends the input to the clients
	int OutputThread(); // thread that processes the inputs received from the server
	int ListenThread();
	int ReceiveThread();

	void Log(std::string msg);
	void ServerLog(std::string msg);
	void ClientLog(std::string msg);

private:

	struct WindowData
	{
		std::string sKeyboardState;
		std::string sMouseState[2];
		std::string sLabels[2];

		RAWINPUTDEVICE rid[3] = { 0 }; // index 2 not used
		MODE nMode = MODE::UNDEF;

		RECT textRect = { 0 };
	} Data;

	struct ServerData
	{
		INPUT inputBuff;
		int nConnected = 0;
		bool isOnline = false;
		bool bAccepting = false;
		bool clientConnected = false;
		bool wasServer = false;

		bool isRegistered = false;
		RAWINPUTDEVICE rid[3]; // index #2 not used
		std::queue<INPUT> inputQueue;

		bool bPause = true;

		struct ClientInfo
		{
			SOCKET socket;
			std::string ip;
			int id;
		};

		std::vector<ClientInfo> ClientsInformation;
		SOCKET sktListen = INVALID_SOCKET;

		std::thread tSend;
		std::thread tListen;
		std::condition_variable cond_listen;
		std::condition_variable cond_input;
		std::mutex mu_sktclient;
		std::mutex mu_input;


		bool bOnOtherScreen = false;
		short nOffsetX = 0;
		short nOffsetY = 0;
		int oldX = 0;
		int oldY = 0;
		POINT mPos;

	} Server;

	struct ClientData
	{
		std::string ip = "192.168.1.8";
		INPUT recvBuff;
		bool isConnected = false;
		bool wasClient = false;

		std::thread tRecv;
		std::thread tSendInput;
		std::condition_variable cond_input;
		std::condition_variable cond_recv;
		std::mutex mu_input;
		std::mutex mu_recv;

		SOCKET sktServer = INVALID_SOCKET;

		std::queue<INPUT> inputQueue;

	} Client;

	HMENU m_hMenu;
	Button m_btnOk;
	Button m_btnPause;
	Button m_btnModeClient, m_btnConnect, m_btnDisconnect;
	Button m_btnModeServer, m_btnStart, m_btnTerminate;
	InputBox m_itxtIP;
	StaticBox m_stxtNbConnected;
	StaticBox m_stxtKeyboard, m_stxtMouse, m_stxtMouseBtn, m_stxtMouseOffset;
};


LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {

    case WM_CREATE:
		return HandleCreate(uMsg, wParam, lParam);

	case WM_INPUT:
		RetrieveInput(uMsg, wParam, lParam);
		return 0;

	case WM_PAINT:
		return HandlePaint(uMsg, wParam, lParam);

	case WM_COMMAND:
		return HandleCommand(uMsg, wParam, lParam);

	case WM_CLOSE:
		return HandleClose(uMsg, wParam, lParam);

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
    }
    return TRUE;
}
int MainWindow::RetrieveInput(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (Data.nMode == MODE::SERVER)
	{
		unsigned int dwSize;
		// get the size of the input data
		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER)) == -1) {
			return 1;
		}

		// allocate enough space to store the raw input data
		LPBYTE lpb = nullptr;
		lpb = new unsigned char[dwSize];

		if (lpb == nullptr) {
			return 1;
		}

		// get the raw input data
		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
			delete[] lpb;
			return 1;
		}

		ConvertInput((PRAWINPUT)lpb, &Server.inputBuff);
		delete[] lpb;


		// This block is responsible for sending the input to the other computer if 
		// the mouse cursor reaches the right edge of the screen
		// at which point, the cursor on the server computer becomes "frozen" until 
		// the cursor on the other computer reaches the left side of its screen
		if (Server.inputBuff.type == 0 && !Server.bOnOtherScreen)
		{
			if (!GetCursorPos(&Server.mPos))
			{
				Log("Unable to retrieve absolute position of cursor");
			}
			else
			{
				if (Server.mPos.x >= nScreenWidth[0] - 1)
				{
					Server.inputBuff.mi.dwFlags = Server.inputBuff.mi.dwFlags | MOUSEEVENTF_ABSOLUTE;
					Server.inputBuff.mi.dx = 0;
					Server.inputBuff.mi.dy = ((float)Server.mPos.y / (float)nScreenHeight[0]) * nNormalized;
					Server.oldX = Server.mPos.x;
					Server.oldY = Server.mPos.y;
					Server.bPause = false;
					Server.bOnOtherScreen = true;
				}
			}
		}
		else if (Server.bOnOtherScreen)
		{
			short newOffsetX = Server.nOffsetX + Server.inputBuff.mi.dx;
			if (Server.nOffsetX > 12000)
			{
				Log("WARNING: massive jump in offsetX");
				Server.nOffsetX = 12000;
			}
			else
			{
				Server.nOffsetX += Server.inputBuff.mi.dx;
			}
			//d.nOffsetY += Server.inputBuff.mi.dy;
			GetCursorPos(&Server.mPos);
			SetCursorPos(nScreenWidth[0]-10, Server.mPos.y);

			if (Server.nOffsetX < 0)
			{
				Server.bPause = true;
				Server.bOnOtherScreen = false;
			}

		}
		if (!Server.bPause)
		{
			std::unique_lock<std::mutex> lock(Server.mu_input);
			Server.inputQueue.emplace(Server.inputBuff);
			Server.cond_input.notify_all();
		}

		UpdateInput();
	}
	return 0;
}
int MainWindow::HandleCommand(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (HIWORD(wParam)) {
	case BN_CLICKED:
		switch (LOWORD(wParam)) {

		case BTN_START:
			return ServerStart(iport);

		case BTN_PAUSE:
			Server.bPause = !Server.bPause;
			Log(((!Server.bPause) ? "Resumed": "Paused"));
			SetWindowText(m_btnPause.Window(), (Server.bPause) ? "Resume": "Pause");
			return 0;

		case BTN_TERMINATE:
			return ServerTerminate();

		case BTN_CONNECT:
			return ClientConnect();

		case BTN_DISCONNECT:
			return ClientDisconnect();

		case EDIT_ADRESS:

			break;

		case BTN_SERVER:
			return SetMode(MODE::SERVER);

		case BTN_CLIENT:
			return SetMode(MODE::CLIENT);

		case MENU_FILE:

			break;

		case MENU_SUB:

			break;

		case MENU_EXIT:
			PostMessage(m_hwnd, WM_CLOSE, 0, 0);
			return 0;

		case MENU_ABOUT:

			break;

		default:
			//MessageBox(m_hwnd, "A Button was clicked", "My application", MB_OKCANCEL);
			break;
		}
		return 0;

	default:
		break;
	}
}
int MainWindow::HandleCreate(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//m_btnOk.Create(L"OK", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 0, 50, 50, 100, 100, this->m_hwnd);
	CreateWindowA("BUTTON", "Mode", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_GROUPBOX, 20, 10, 190, 60, m_hwnd, (HMENU)BTN_MODE, (HINSTANCE)GetWindowLong(m_hwnd, GWL_HINSTANCE), NULL);
	m_btnModeServer.Create(this, "Server", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 0, 30, 35, 70, 20, m_hwnd, (HMENU)BTN_SERVER);
	m_btnModeClient.Create(this, "Client", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 0, 130, 35, 70, 20, m_hwnd, (HMENU)BTN_CLIENT);

	m_btnStart.Create(this, "Start", WS_TABSTOP | WS_CHILD | BS_PUSHBUTTON, 0, 20, 80, 50, 20, m_hwnd, (HMENU)BTN_START);
	m_btnPause.Create(this, "Pause", WS_TABSTOP | WS_CHILD | BS_PUSHBUTTON, 0, 80, 80, 50, 20, m_hwnd, (HMENU)BTN_PAUSE);
	m_btnTerminate.Create(this, "Terminate", WS_TABSTOP | WS_CHILD | BS_PUSHBUTTON, 0, 140, 80, 70, 20, m_hwnd, (HMENU)BTN_TERMINATE);

	m_btnConnect.Create(this, "Connect", WS_TABSTOP | WS_CHILD | BS_PUSHBUTTON, 0, 35, 80, 60, 20, m_hwnd, (HMENU)BTN_CONNECT);
	m_btnDisconnect.Create(this, "Disconnect", WS_TABSTOP | WS_CHILD | BS_PUSHBUTTON, 0, 115, 80, 80, 20, m_hwnd, (HMENU)BTN_DISCONNECT);

	//hLog = CreateWindowEx(0, "edit", txtLog.c_str(), WS_VISIBLE | WS_CHILD | ES_READONLY | ES_MULTILINE, 250, 10, 200, 260, m_hwnd, NULL, (HINSTANCE)GetWindowLong(m_hwnd, GWL_HINSTANCE), NULL);

	m_itxtIP.Create(this, Client.ip.c_str(), WS_VISIBLE | WS_CHILD | ES_READONLY, 0, 130, 120, 100, 20, m_hwnd, (HMENU)EDIT_ADRESS);
	m_stxtNbConnected.Create(this, std::to_string(Server.nConnected).c_str(), WS_VISIBLE | WS_CHILD, 0, 130, 150, 100, 20, m_hwnd, NULL);

	m_stxtKeyboard.Create(this, "", WS_VISIBLE | WS_CHILD, 0, 130, 180, 100, 20, m_hwnd, NULL);
	m_stxtMouse.Create(this, "", WS_VISIBLE | WS_CHILD, 0, 130, 210, 100, 20, m_hwnd, NULL);
	m_stxtMouseOffset.Create(this, "", WS_VISIBLE | WS_CHILD, 0, 130, 230, 100, 20, m_hwnd, NULL);
	m_stxtMouseBtn.Create(this, "", WS_VISIBLE | WS_CHILD, 0, 130, 250, 100, 20, m_hwnd, NULL);

	return 0;
}
int MainWindow::HandlePaint(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	RECT clientRect;
	RECT textRect;
	HRGN bgRgn;
	HBRUSH hBrush;
	HPEN hPen;


	hdc = BeginPaint(m_hwnd, &ps);
	GetClientRect(m_hwnd, &clientRect);

	hBrush = CreateSolidBrush(RGB(255, 255, 255));
	FillRect(hdc, &clientRect, hBrush);

	TextOut(hdc, 20, 120, Data.sLabels[0].c_str(), Data.sLabels[0].length());
	TextOut(hdc, 20, 150, Data.sLabels[1].c_str(), Data.sLabels[1].length());

	TextOut(hdc, 20, 180, "Keyboard Input:", 15);
	TextOut(hdc, 20, 210, "Mouse Input:", 12);

	EndPaint(m_hwnd, &ps);
	return 0;
}
int MainWindow::HandleClose(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (MessageBox(m_hwnd, "Really quit?", "Remote", MB_OKCANCEL) == IDOK) {
		Data.nMode = MODE::UNDEF;
		DestroyWindow(m_hwnd);
	}
	return 0;
}

void MainWindow::Log(std::string msg)
{
	switch (Data.nMode)
	{
	case MODE::CLIENT: ClientLog(msg); break;
	case MODE::SERVER: ServerLog(msg); break;
	case MODE::UNDEF: std::cout << msg << std::endl; break;
	}
}
void MainWindow::ServerLog(std::string msg)
{
	std::cout << "Server - " << msg << std::endl;
}
void MainWindow::ClientLog(std::string msg)
{
	std::cout << "Client - " << msg << std::endl;
}

int MainWindow::InitializeInputDevice() {

	// keyboard
	Server.rid[0].dwFlags = RIDEV_INPUTSINK;
	Server.rid[0].usUsagePage = 1;
	Server.rid[0].usUsage = 6;
	Server.rid[0].hwndTarget = m_hwnd;

	// mouse
	Server.rid[1].dwFlags = RIDEV_INPUTSINK;
	Server.rid[1].usUsagePage = 1;
	Server.rid[1].usUsage = 2;
	Server.rid[1].hwndTarget = m_hwnd;

	if (!RegisterRawInputDevices(Server.rid, 2, sizeof(RAWINPUTDEVICE)))
	{
		return 1;
	}
	return 0;
}

void MainWindow::UpdateInput() {
	if (Server.inputBuff.type == INPUT_KEYBOARD) {
		std::string out;

		//out = ConvertVKey(input->ki.wVk);
		out += MapVirtualKeyA(Server.inputBuff.ki.wVk, MAPVK_VK_TO_CHAR);
		if (Server.inputBuff.ki.dwFlags == KEYEVENTF_KEYUP) {
			out = out + " UP";
		}
		else {
			out = out + " DOWN";
		}

		SetWindowText(m_stxtKeyboard.Window(), out.c_str());
	}
	else if (Server.inputBuff.type == INPUT_MOUSE) {
		if (Server.inputBuff.mi.dwFlags == MOUSEEVENTF_LEFTDOWN) {
			SetWindowText(m_stxtMouseBtn.Window(), "Left Pressed");
		}
		else if (Server.inputBuff.mi.dwFlags == MOUSEEVENTF_RIGHTDOWN) {
			SetWindowText(m_stxtMouseBtn.Window(), "Right Pressed");
		}
		else if (Server.inputBuff.mi.dwFlags == MOUSEEVENTF_LEFTUP) {
			SetWindowText(m_stxtMouseBtn.Window(), "Left Released");
		}
		else if (Server.inputBuff.mi.dwFlags == MOUSEEVENTF_RIGHTUP) {
			SetWindowText(m_stxtMouseBtn.Window(), "Right Released");
		}
		else {
			SetWindowText(m_stxtMouseBtn.Window(), "");
		}
		Data.sMouseState[0] = std::to_string(Server.inputBuff.mi.dx);
		Data.sMouseState[1] = std::to_string(Server.inputBuff.mi.dy);
		std::string mouse_pos = "(" + Data.sMouseState[0] + ", " + Data.sMouseState[1] + ")";
		std::string mouse_offset = "(" + std::to_string(Server.nOffsetX) + ")";
		SetWindowText(m_stxtMouse.Window(), mouse_pos.c_str());
		SetWindowText(m_stxtMouseOffset.Window(), mouse_offset.c_str());
	}
}
void MainWindow::UpdateGuiControls()
{
	switch (Data.nMode)
	{
	case MODE::CLIENT:
	{
		if (!Client.isConnected && !Client.wasClient) {
			Button_Enable(m_btnStart.Window(), false);
			Button_Enable(m_btnTerminate.Window(), false);
			Button_Enable(m_btnPause.Window(), false);
			ShowWindow(m_btnStart.Window(), SW_HIDE);
			ShowWindow(m_btnTerminate.Window(), SW_HIDE);
			ShowWindow(m_btnPause.Window(), SW_HIDE);

			Button_Enable(m_btnConnect.Window(), true);
			Button_Enable(m_btnDisconnect.Window(), false);
			ShowWindow(m_btnConnect.Window(), SW_SHOW);
			ShowWindow(m_btnDisconnect.Window(), SW_SHOW);

			Data.sLabels[0] = "Server Address: ";
			Data.sLabels[1] = "Connected: ";

			SetRect(&Data.textRect, 20, 120, 129, 170);
			InvalidateRect(m_hwnd, &Data.textRect, true);

			PostMessage(m_itxtIP.Window(), EM_SETREADONLY, (WPARAM)false, 0);

			//SetWindowLong(hTxtIP, GWL_STYLE, GetWindowLong(hTxtIP, GWL_STYLE) ^ ES_READONLY);
			//UpdateWindow(hTxtIP);
			UpdateWindow(m_hwnd);
		}
		else if (Client.isConnected)
		{
			Button_Enable(m_btnConnect.Window(), false);
			Button_Enable(m_btnDisconnect.Window(), true);
			Button_Enable(m_btnModeServer.Window(), false);
			Button_Enable(m_btnModeClient.Window(), false);
		}
		else if (!Client.isConnected)
		{
			Button_Enable(m_btnConnect.Window(), true);
			Button_Enable(m_btnDisconnect.Window(), false);
			Button_Enable(m_btnModeServer.Window(), true);
			Button_Enable(m_btnModeClient.Window(), true);
		}
	}
		break;

	case MODE::SERVER:
	{
		if (!Server.isOnline && !Server.wasServer) {
			Button_Enable(m_btnStart.Window(), true);
			Button_Enable(m_btnPause.Window(), false);
			Button_Enable(m_btnTerminate.Window(), false);
			ShowWindow(m_btnStart.Window(), SW_SHOW);
			ShowWindow(m_btnTerminate.Window(), SW_SHOW);
			ShowWindow(m_btnPause.Window(), SW_SHOW);

			Button_Enable(m_btnConnect.Window(), false);
			Button_Enable(m_btnDisconnect.Window(), false);
			ShowWindow(m_btnConnect.Window(), SW_HIDE);
			ShowWindow(m_btnDisconnect.Window(), SW_HIDE);

			Data.sLabels[0] = "IP Address: ";
			Data.sLabels[1] = "Clients Connected: ";

			SetRect(&Data.textRect, 20, 120, 129, 170);
			InvalidateRect(m_hwnd, &Data.textRect, true);

			PostMessage(m_itxtIP.Window(), EM_SETREADONLY, (WPARAM)true, 0);
			SetWindowText(m_itxtIP.Window(), Client.ip.c_str());

			//SetWindowLong(hTxtIP, GWL_STYLE, GetWindowLong(hTxtIP, GWL_STYLE) ^ ES_READONLY);
			//UpdateWindow(hTxtIP);
			UpdateWindow(m_hwnd);
		}
		else if (Server.isOnline)
		{
			Button_Enable(m_btnStart.Window(), false);
			Button_Enable(m_btnTerminate.Window(), true);
			Button_Enable(m_btnPause.Window(), true);
			Button_Enable(m_btnModeServer.Window(), false);
			Button_Enable(m_btnModeClient.Window(), false);
		}
		else if (!Server.isOnline)
		{
			Button_Enable(m_btnStart.Window(), false);
			Button_Enable(m_btnTerminate.Window(), true);
			Button_Enable(m_btnPause.Window(), true);
			Button_Enable(m_btnModeServer.Window(), false);
			Button_Enable(m_btnModeClient.Window(), false);
		}
	}
	break;

	case MODE::UNDEF:
		break;
	}
}

void MainWindow::ConvertInput(PRAWINPUT pRaw, INPUT* pInput) {
	if (pRaw->header.dwType == RIM_TYPEMOUSE) {
		pInput->type = INPUT_MOUSE;
		pInput->mi.dx = pRaw->data.mouse.lLastX;
		pInput->mi.dy = pRaw->data.mouse.lLastY;
		pInput->mi.mouseData = 0;
		pInput->mi.dwFlags = 0;
		pInput->mi.time = 0;
		if (pRaw->data.mouse.lLastX != 0 || pRaw->data.mouse.lLastY != 0) {
			pInput->mi.dwFlags = pInput->mi.dwFlags | MOUSEEVENTF_MOVE;
		}
		else if (pRaw->data.mouse.usFlags == MOUSE_MOVE_ABSOLUTE) {
			pInput->mi.dwFlags = pInput->mi.dwFlags | MOUSEEVENTF_ABSOLUTE;
		}
		else {
			switch (pRaw->data.mouse.usButtonFlags) {
			case RI_MOUSE_LEFT_BUTTON_DOWN:
				pInput->mi.dwFlags = pInput->mi.dwFlags | MOUSEEVENTF_LEFTDOWN;
				break;

			case RI_MOUSE_LEFT_BUTTON_UP:
				pInput->mi.dwFlags = pInput->mi.dwFlags | MOUSEEVENTF_LEFTUP;
				break;

			case RI_MOUSE_MIDDLE_BUTTON_DOWN:
				pInput->mi.dwFlags = pInput->mi.dwFlags | MOUSEEVENTF_MIDDLEDOWN;
				break;

			case RI_MOUSE_MIDDLE_BUTTON_UP:
				pInput->mi.dwFlags = pInput->mi.dwFlags | MOUSEEVENTF_MIDDLEUP;
				break;

			case RI_MOUSE_RIGHT_BUTTON_DOWN:
				pInput->mi.dwFlags = pInput->mi.dwFlags | MOUSEEVENTF_RIGHTDOWN;
				break;

			case RI_MOUSE_RIGHT_BUTTON_UP:
				pInput->mi.dwFlags = pInput->mi.dwFlags | MOUSEEVENTF_RIGHTUP;
				break;

			case RI_MOUSE_WHEEL:
				pInput->mi.dwFlags = pInput->mi.dwFlags | MOUSEEVENTF_WHEEL;
				pInput->mi.mouseData = pRaw->data.mouse.usButtonData;
				break;
			}
		}

	}
	else if (pRaw->header.dwType == RIM_TYPEKEYBOARD) {
		pInput->type = INPUT_KEYBOARD;
		pInput->ki.wVk = pRaw->data.keyboard.VKey;
		pInput->ki.wScan = MapVirtualKeyA(pRaw->data.keyboard.VKey, MAPVK_VK_TO_VSC);
		pInput->ki.dwFlags = KEYEVENTF_SCANCODE;
		pInput->ki.time = 0;
		if (pRaw->data.keyboard.Message == WM_KEYUP) {
			pInput->ki.dwFlags = KEYEVENTF_KEYUP;
		}
		else if (pRaw->data.keyboard.Message == WM_KEYDOWN) {
			pInput->ki.dwFlags = 0;
		}
	}
}

int MainWindow::SetMode(MODE m)
{
	switch (m)
	{
	case MODE::SERVER:
		if (Client.wasClient)
		{

		}
		Log("Mode server");
		Data.nMode = MODE::SERVER;
		UpdateGuiControls();
		return 0;

	case MODE::CLIENT:

		if (Server.wasServer)
		{

		}
		Log("Mode client");
		Data.nMode = MODE::CLIENT;
		UpdateGuiControls();
		return 0;

	default:
		Log("Mode Unknown");
		Data.nMode = MODE::UNDEF;
		return 0;
	}
	return 0;
}

int MainWindow::ServerStart(int nPort)
{
	if (!Server.isRegistered)
	{
		InitializeInputDevice();
		Log("Input Device Registered");
		Server.isRegistered = true;
	}
	Log("Initializing");
	int error = InitializeServer(Server.sktListen, nPort);
	if (error == 1) {
		Log("Could not initialize server");
		if (MessageBox(m_hwnd, "Could not initialize server", "Remote - Error", MB_ABORTRETRYIGNORE | MB_DEFBUTTON1 | MB_ICONERROR) == IDRETRY) {
			PostMessage(m_hwnd, WM_COMMAND, MAKEWPARAM(BTN_START, BN_CLICKED), 0);
			return 1;
		}
	}
	else {
		Log("Server initialized");
		Server.ClientsInformation.resize(MAX_CLIENTS);
		for (auto& c: Server.ClientsInformation)
		{
			c.socket = INVALID_SOCKET;
			c.ip = "";
			c.id = -1;
		}
		Log("Sockets initialized");
		Server.isOnline = true;
		UpdateGuiControls();
		// start listening thread
		Log("Starting listening thread");
		Server.tListen = std::thread(&MainWindow::ListenThread, this);

		// start sending input thread
		Log("Starting sending thread");
		Server.tSend = std::thread(&MainWindow::SendThread, this);

		Server.tListen.detach();
		Server.tSend.detach();
	}
	return 0;
}
int MainWindow::ServerTerminate()
{
	if (Server.isOnline)
	{
		int error = 1;
		Log("Terminate");
		//MessageBox(m_hwnd, "Terminate", "Remote", MB_OK);
		std::vector<SOCKET> skt_clients;
		for (auto& skt: Server.ClientsInformation)
		{
			skt_clients.push_back(skt.socket);
		}
		TerminateServer(Server.sktListen, skt_clients);
		Server.isOnline = false;
		Server.cond_listen.notify_all();
		Server.cond_input.notify_all();
		UpdateGuiControls();
		return 0;
	}

	return 1;
}

int MainWindow::ClientConnect()
{
	char out[50];
	int error = 1;
	GetWindowText(m_itxtIP.Window(), out, 50);
	Client.ip = out;
	//MessageBox(m_hwnd, Client.ip.c_str(), "Remote - Connect", MB_OK);
	Log("Initializing client ");
	InitializeClient();
	Log("Connecting to server: " + Client.ip + ":" + std::to_string(iport));
	error = ConnectServer(Client.sktServer, Client.ip, iport);
	if (error == 1) {
		Log("Couldn't connect");
		//MessageBox(NULL, "couldn't connect", "Remote", MB_OK);
	}
	else {
		Log("Connected! " + std::to_string(iport));
		Client.isConnected = true;
		UpdateGuiControls();

		//start receive thread that will receive data
		Log("Starting receive thread");
		Client.tRecv = std::thread(&MainWindow::ReceiveThread, this);

		// start send input thread that sends the received input
		Log("Starting input thread");
		Client.tSendInput = std::thread(&MainWindow::OutputThread, this);

		Client.tSendInput.detach();
		Client.tRecv.detach();

	}
	return 0;
}
int MainWindow::ClientDisconnect()
{
	Log("Disconnect");
	CloseConnection(&Client.sktServer);
	//MessageBox(m_hwnd, "Disconnect", "Remote", MB_OK);
	Log("Ending receive thread");
	Client.isConnected = false;
	Client.cond_input.notify_all();
	Client.cond_recv.notify_all();
	UpdateGuiControls();
	return 0;
}

int MainWindow::ListenThread()
{
	bool socket_found = false;
	int index = 0;
	while (Server.isOnline && Data.nMode == MODE::SERVER)
	{
		std::unique_lock<std::mutex> lock(Server.mu_sktclient);
		if (Server.nConnected >= MAX_CLIENTS)
		{
			Server.cond_listen.wait(lock);
		}
		if (!socket_found) {
			for (int i = 0; i < Server.ClientsInformation.size(); i++)
			{
				if (Server.ClientsInformation[i].socket == INVALID_SOCKET)
				{
					socket_found = true;
					index = i;
				}
			}
		}
		lock.unlock();
		if (listen(Server.sktListen, 1) == SOCKET_ERROR) {
			Log("Listen failed with error: " + std::to_string(WSAGetLastError()));
		}
		sockaddr* inc_conn = new sockaddr;
		int sosize = sizeof(sockaddr);
		Server.ClientsInformation[index].socket = accept(Server.sktListen, inc_conn, &sosize);
		if (Server.ClientsInformation[index].socket == INVALID_SOCKET)
		{
			Log("accept failed: " + std::to_string(WSAGetLastError()));
		}
		else
		{
			Log("Connection accepted");

			socket_found = false;
		}
		delete inc_conn;

	}
	Log("Listen thread - ended");
	return 0;
}
int MainWindow::SendThread()
{
	while (Server.isOnline && Data.nMode == MODE::SERVER)
	{
		std::unique_lock<std::mutex> lock(Server.mu_input);
		if (Server.inputQueue.empty())
		{
			Server.cond_input.wait(lock);
		}
		else
		{
			INPUT inputData = Server.inputQueue.front();
			int bytes = 0;
			for (auto& client: Server.ClientsInformation)
			{
				//std::cout << "Searching for client" << std::endl;
				if (client.socket != INVALID_SOCKET)
				{
					//std::cout << "Sending..." << std::endl;
					bytes = send(client.socket, (char*)&inputData, sizeof(INPUT), 0);

					if (bytes == 0)
					{
						// client disconnected
						Log("Client number " + std::to_string(client.id) + " disconnected: " + std::to_string(client.socket) + "\nIP: " + client.ip);
						Server.nConnected--;
						closesocket(client.socket);
						client.socket = INVALID_SOCKET;
						client.id = -1;
						client.ip = "";
						Server.cond_listen.notify_all();
					}
				}
			}
			//lock.lock();
			Server.inputQueue.pop();
		}
	}
	Log("Sending thread - ended");
	return 0;
}
int MainWindow::ReceiveThread()
{
	while (Client.isConnected && Data.nMode == MODE::CLIENT)
	{
		int error = 1;
		error = ReceiveServer(Client.sktServer, Client.recvBuff);
		if (error == 0)
		{
			std::unique_lock<std::mutex> lock(Client.mu_input);
			Client.inputQueue.emplace(Client.recvBuff);
			lock.unlock();
			Client.cond_input.notify_all();
		}
		else
		{
			Client.isConnected = false;
			Log("No input received, disconnecting");
			PostMessage(m_hwnd, WM_COMMAND, MAKEWPARAM(BTN_DISCONNECT, BN_CLICKED), 0);
		}
	}
	return 0;
}
int MainWindow::OutputThread()
{
	while (Client.isConnected && Data.nMode == MODE::CLIENT)
	{
		std::unique_lock<std::mutex> lock(Client.mu_input);
		if (Client.inputQueue.empty())
		{
			Client.cond_input.wait(lock);
		}
		else
		{
			int sz = Client.inputQueue.size();
			INPUT* tInputs = new INPUT[sz];
			for (int i = 0; i < sz; ++i)
			{
				tInputs[i] = Client.inputQueue.front();
				Client.inputQueue.pop();
			}
			lock.unlock();
			UpdateInput();
			//std::cout << "sending input" << std::endl;
			SendInput(sz, tInputs, sizeof(INPUT));
			delete[] tInputs;
		}
	}
	Log("Receive thread - ended");
	return 0;
}

int main()
{
	HMENU hMenu;
	MainWindow win;

	if (!win.Create(nullptr, "Remote", WS_OVERLAPPEDWINDOW, 0, CW_USEDEFAULT, CW_USEDEFAULT, 477, 340, NULL))
	{
		std::cout << "error creating the main window: " << GetLastError() << std::endl;
		return 0;
	}

	ShowWindow(win.Window(), 1);

	// Run the message loop.

	MSG msg = { };
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}