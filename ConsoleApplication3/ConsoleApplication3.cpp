#include <iostream>
#include <windows.h>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <string>
#include <shellapi.h>

// Define hotkey IDs
#define HOTKEY_LEFT_ALT 1
#define HOTKEY_RIGHT_ALT 2

using namespace std;

vector<HANDLE> mice;
HANDLE selectedMouse = nullptr;
atomic<bool> clickingLeft(false);
atomic<bool> clickingRight(false);
atomic<bool> isClickingLeft(true);
atomic<bool> isClickingRight(true);
atomic<int> currentCPS(0);

// Window handle for the overlay GUI
HWND overlayWindow = nullptr;
const wchar_t* OVERLAY_CLASS_NAME = L"DragClickerOverlay";
const wchar_t* OVERLAY_WINDOW_NAME = L"ClickerStatus";

// For alt key detection
atomic<bool> leftAltWasDown(false);
atomic<bool> rightAltWasDown(false);

// Function to show Windows notification
void ShowNotification(const wstring& title, const wstring& message) {
	NOTIFYICONDATA nid = { 0 };
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = overlayWindow;
	nid.uID = 1;
	nid.uFlags = NIF_INFO;
	nid.dwInfoFlags = NIIF_INFO;
	wcscpy_s(nid.szInfoTitle, title.c_str());
	wcscpy_s(nid.szInfo, message.c_str());
	Shell_NotifyIcon(NIM_ADD, &nid);
	Sleep(500); // Give time for notification to show
	Shell_NotifyIcon(NIM_DELETE, &nid);
}

// Random number generator for more realistic clicking
std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

// AutoClick function now includes enhanced randomization and pattern timing
void AutoClick(int button, atomic<bool>& clickingFlag) {
	if (button == VK_LBUTTON && !isClickingLeft.load()) {
		cout << " Skipping clicking button " << button << endl;
		return;
	}
	if (button == VK_RBUTTON && !isClickingRight.load()) {
		cout << " Skipping clicking button " << button << endl;
		return;
	}

	cout << "Clicking button " << button << endl;
	INPUT input = { 0 };
	input.type = INPUT_MOUSE;
	if (button == VK_LBUTTON) {
		input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;
	}
	else if (button == VK_RBUTTON) {
		input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP;
	}

	// Enhanced randomized clicking timing with greater variation
	std::uniform_int_distribution<int> delayDist(5, 45); // Wider range of delays
	std::uniform_int_distribution<int> burstDist(1, 4); // More variation in burst clicks
	std::uniform_int_distribution<int> clickingTimeDist(3000, 5000); // Random clicking time (3-5 seconds)
	std::uniform_int_distribution<int> pauseTimeDist(500, 900); // Random pause time (500-900ms)

	// Special distributions for more natural drag clicking patterns
	std::normal_distribution<float> normalDist(25.0f, 10.0f); // Normal distribution for more natural timing
	std::bernoulli_distribution skipDist(0.05); // Occasionally skip a click (5% chance) for realism

	int clickCount = 0;
	auto startTime = std::chrono::steady_clock::now();
	auto countdownStart = std::chrono::steady_clock::now();
	bool inClickingPhase = true;

	// Randomize initial clicking and pause times
	int currentClickingTime = clickingTimeDist(rng);
	int currentPauseTime = pauseTimeDist(rng);

	while (clickingFlag.load()) {
		auto now = std::chrono::steady_clock::now();

		// Manage the randomized clicking and pause pattern
		auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - countdownStart).count();

		if (inClickingPhase && elapsedMs >= currentClickingTime) {
			// Switch to pause phase with new random pause time
			inClickingPhase = false;
			countdownStart = now;
			currentPauseTime = pauseTimeDist(rng);
			continue;
		}
		else if (!inClickingPhase && elapsedMs >= currentPauseTime) {
			// Switch back to clicking phase with new random clicking time
			inClickingPhase = true;
			countdownStart = now;
			currentClickingTime = clickingTimeDist(rng);
		}

		// Only click during clicking phase
		if (inClickingPhase && clickingFlag.load()) {
			// Randomized burst clicking
			int burstClicks = burstDist(rng);
			for (int i = 0; i < burstClicks && clickingFlag.load(); i++) {
				// Occasionally skip a click for more realism
				if (!skipDist(rng)) {
					SendInput(1, &input, sizeof(INPUT));
					clickCount++;
				}

				// Add micro-delays between clicks in a burst for more realism
				if (i < burstClicks - 1) {
					std::uniform_int_distribution<int> microDelayDist(1, 5);
					this_thread::sleep_for(chrono::milliseconds(microDelayDist(rng)));
				}
			}

			// Update CPS counter every second
			auto secondsElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
			if (secondsElapsed >= 1) {
				currentCPS.store(clickCount);
				clickCount = 0;
				startTime = now;
			}

			// Use normal distribution for more natural timing with occasional outliers
			int delay = max(1, min(70, static_cast<int>(normalDist(rng))));
			this_thread::sleep_for(chrono::milliseconds(delay));
		}
		else if (!inClickingPhase) {
			// During pause phase, just wait a bit
			this_thread::sleep_for(chrono::milliseconds(50));
		}
	}

	// Reset CPS when clicking stops
	currentCPS.store(0);
}

// Handler for Alt key presses
void HandleAltToggle(bool isLeftAlt) {
	if (isLeftAlt) {
		isClickingLeft.store(!isClickingLeft.load());
		std::cout << "Left auto-clicking " << (isClickingLeft.load() ? "ENABLED" : "DISABLED") << std::endl;

		// Show notification
		wstring title = L"Left Auto-clicker";
		wstring message = isClickingLeft.load() ? L"ENABLED" : L"DISABLED";
		ShowNotification(title, message);
	}
	else {
		isClickingRight.store(!isClickingRight.load());
		std::cout << "Right auto-clicking " << (isClickingRight.load() ? "ENABLED" : "DISABLED") << std::endl;

		// Show notification
		wstring title = L"Right Auto-clicker";
		wstring message = isClickingRight.load() ? L"ENABLED" : L"DISABLED";
		ShowNotification(title, message);
	}

	// Force a redraw of the overlay
	if (overlayWindow) {
		InvalidateRect(overlayWindow, NULL, TRUE);
	}
}

// Check Alt keys directly
void CheckAltKeys() {
	// Left Alt key check
	bool leftAltIsDown = (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0;
	if (leftAltIsDown && !leftAltWasDown.load()) {
		HandleAltToggle(true);
	}
	leftAltWasDown.store(leftAltIsDown);

	// Right Alt key check
	bool rightAltIsDown = (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;
	if (rightAltIsDown && !rightAltWasDown.load()) {
		HandleAltToggle(false);
	}
	rightAltWasDown.store(rightAltIsDown);
}

// Window Procedure for the overlay window
LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		// Set background
		RECT rect;
		GetClientRect(hwnd, &rect);
		SetBkMode(hdc, TRANSPARENT);
		HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
		FillRect(hdc, &rect, bgBrush);
		DeleteObject(bgBrush);

		// Draw text for status
		SetTextColor(hdc, RGB(255, 255, 255));

		// Left click status
		std::wstring leftStatus = L"Left Click: ";
		leftStatus += isClickingLeft.load() ? L"ENABLED" : L"DISABLED";
		COLORREF leftColor = isClickingLeft.load() ? RGB(0, 255, 0) : RGB(255, 0, 0);
		SetTextColor(hdc, leftColor);
		TextOut(hdc, 10, 10, leftStatus.c_str(), leftStatus.length());

		// Right click status
		std::wstring rightStatus = L"Right Click: ";
		rightStatus += isClickingRight.load() ? L"ENABLED" : L"DISABLED";
		COLORREF rightColor = isClickingRight.load() ? RGB(0, 255, 0) : RGB(255, 0, 0);
		SetTextColor(hdc, rightColor);
		TextOut(hdc, 10, 30, rightStatus.c_str(), rightStatus.length());

		// Current CPS
		SetTextColor(hdc, RGB(255, 255, 255));
		std::wstring cpsText = L"CPS: " + std::to_wstring(currentCPS.load());
		TextOut(hdc, 10, 50, cpsText.c_str(), cpsText.length());

		EndPaint(hwnd, &ps);
		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Create the overlay window
void CreateOverlayWindow() {
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = OverlayProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = OVERLAY_CLASS_NAME;
	RegisterClass(&wc);

	overlayWindow = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
		OVERLAY_CLASS_NAME,
		OVERLAY_WINDOW_NAME,
		WS_POPUP | WS_VISIBLE,
		0, 0, 200, 80,
		nullptr, nullptr, GetModuleHandle(nullptr), nullptr
	);

	// Make window semi-transparent
	SetLayeredWindowAttributes(overlayWindow, 0, 180, LWA_ALPHA);

	// Make the window clickthrough
	LONG exStyle = GetWindowLong(overlayWindow, GWL_EXSTYLE);
	exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
	SetWindowLong(overlayWindow, GWL_EXSTYLE, exStyle);

	UpdateWindow(overlayWindow);
}

// Window Procedure handles raw mouse input events.
LRESULT CALLBACK MouseProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_INPUT) {
		RAWINPUT raw;
		UINT size = sizeof(RAWINPUT);
		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == -1)
			return DefWindowProc(hwnd, msg, wParam, lParam);

		if (raw.header.dwType == RIM_TYPEMOUSE) {
			HANDLE mouseHandle = raw.header.hDevice;
			if (mouseHandle == selectedMouse) {
				// Left mouse button handling:
				if (raw.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) {
					if (!clickingLeft.load()) {
						clickingLeft.store(true);
						cout << "Left button pressed, starting auto-click thread." << endl;
						thread(AutoClick, VK_LBUTTON, ref(clickingLeft)).detach();
					}
				}
				if (raw.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
					clickingLeft.store(false);
					cout << "Left button released, stopping auto-click thread." << endl;
				}
				// Right mouse button handling:
				if (raw.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
					if (!clickingRight.load()) {
						clickingRight.store(true);
						cout << "Right button pressed, starting auto-click thread." << endl;
						thread(AutoClick, VK_RBUTTON, ref(clickingRight)).detach();
					}
				}
				if (raw.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) {
					clickingRight.store(false);
					cout << "Right button released, stopping auto-click thread." << endl;
				}
			}
		}
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// List all connected mice.
void ListMice() {
	UINT nDevices;
	GetRawInputDeviceList(nullptr, &nDevices, sizeof(RAWINPUTDEVICELIST));
	RAWINPUTDEVICELIST* deviceList = new RAWINPUTDEVICELIST[nDevices];
	GetRawInputDeviceList(deviceList, &nDevices, sizeof(RAWINPUTDEVICELIST));

	for (UINT i = 0; i < nDevices; i++) {
		if (deviceList[i].dwType == RIM_TYPEMOUSE) {
			mice.push_back(deviceList[i].hDevice);
			cout << "[" << i << "] Mouse " << (i + 1) << endl;
		}
	}
	delete[] deviceList;
}

// Register raw input for the hidden window.
void RegisterRawInput(HWND hwnd) {
	RAWINPUTDEVICE rid;
	rid.usUsagePage = 0x01;
	rid.usUsage = 0x02;
	rid.dwFlags = RIDEV_INPUTSINK;
	rid.hwndTarget = hwnd;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

// Function to update the overlay window
void UpdateOverlay() {
	if (overlayWindow) {
		InvalidateRect(overlayWindow, NULL, TRUE);
	}
}

int main() {
	cout << "Detecting connected mice..." << endl;
	ListMice();
	if (mice.empty()) {
		cout << "No mice detected." << endl;
		return 1;
	}

	int choice;
	cout << "Select a mouse by number: ";
	cin >> choice;
	if (choice < 0 || choice >= mice.size()) {
		cout << "Invalid choice." << endl;
		return 1;
	}
	selectedMouse = mice[choice];
	cout << "Selected Mouse " << (choice + 1) << " for auto-clicking." << endl;
	cout << "Press LEFT ALT to toggle left auto-clicking." << endl;
	cout << "Press RIGHT ALT to toggle right auto-clicking." << endl;

	// Create the overlay window first
	CreateOverlayWindow();

	// Create a hidden window to receive raw input messages.
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = MouseProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = L"RawInputWindow";
	RegisterClass(&wc);
	HWND hwnd = CreateWindowEx(0, L"RawInputWindow", L"HiddenWindow", 0, 0, 0, 0, 0,
		nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
	RegisterRawInput(hwnd);

	MSG msg;
	auto lastUpdateTime = std::chrono::steady_clock::now();
	auto lastKeyCheckTime = std::chrono::steady_clock::now();

	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		// Check Alt keys every 10ms (more reliable than hooks for special keys)
		auto now = std::chrono::steady_clock::now();
		auto elapsedKeyCheckMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastKeyCheckTime).count();
		if (elapsedKeyCheckMs > 10) {
			CheckAltKeys();
			lastKeyCheckTime = now;
		}

		// Update overlay at a reasonable rate (every 100ms)
		auto elapsedOverlayMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdateTime).count();
		if (elapsedOverlayMs > 100) {
			UpdateOverlay();
			lastUpdateTime = now;
		}
	}

	return 0;
}