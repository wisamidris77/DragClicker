#include <iostream>
#include <windows.h>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

using namespace std;

vector<HANDLE> mice;
HANDLE selectedMouse = nullptr;
atomic<bool> clickingLeft(false);
atomic<bool> clickingRight(false);
atomic<bool> isClickingLeft(true);
atomic<bool> isClickingRight(true);

// AutoClick function now toggles on/off when the corresponding Alt key is clicked.
void AutoClick(int button, atomic<bool>& clickingFlag) {
	if (button == VK_LBUTTON && !isClickingLeft.load())
	{
		cout << " Skpping clicking button " + button;
		return;
	}
	if (button == VK_RBUTTON && !isClickingRight.load())
	{
		cout << " Skipping clicking button " + button;
		return;
	}
	cout << "Clicking button " + button;
	INPUT input = { 0 };
	input.type = INPUT_MOUSE;
	if (button == VK_LBUTTON) {
		input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;
	}
	else if (button == VK_RBUTTON) {
		input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP;
	}

	while (clickingFlag.load()) {
		if (clickingFlag.load()) {
			SendInput(1, &input, sizeof(INPUT));
			this_thread::sleep_for(chrono::milliseconds(25));  // Adjust speed as needed
		}
	}
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

// Function to check for Alt key presses and toggle auto-clicking
void CheckAltKeys() {
	static bool leftAltPreviouslyPressed = false;
	static bool rightAltPreviouslyPressed = false;

	bool leftAltPressed = (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0;
	bool rightAltPressed = (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;

	// Toggle left auto-clicker.
	if (leftAltPressed && !leftAltPreviouslyPressed) {
		isClickingLeft.store(!isClickingLeft.load());
		std::cout << "Left auto-clicking " << (isClickingLeft.load() ? "ENABLED" : "DISABLED") << std::endl;
	}
	leftAltPreviouslyPressed = leftAltPressed;

	// Toggle right auto-clicker.
	if (rightAltPressed && !rightAltPreviouslyPressed) {
		isClickingRight.store(!isClickingRight.load());
		std::cout << "Right auto-clicking " << (isClickingRight.load() ? "ENABLED" : "DISABLED") << std::endl;
	}
	rightAltPreviouslyPressed = rightAltPressed;
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
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		// Call the global key checking function
		CheckAltKeys();
	}
	return 0;
}