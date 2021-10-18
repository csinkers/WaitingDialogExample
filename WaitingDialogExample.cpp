#include "framework.h"
#include "WaitWindow.h"
#include "resource.h"

HINSTANCE gInstance;

// Our long-running function to be run on a worker thread
DWORD WINAPI LongRunningOperation(LPVOID param)
{
	const OperationContext& context = *(OperationContext*)param;
	// Cast the argument. This needs to match up with whatever type was actually passed as an argument to the wait window.
	// Note: pArgument doesn't have to be a BOOL*, it could be a pointer to some arbitrary struct/class that we can fill with any data required.
	BOOL *pArgument = (BOOL*)context.pArgument; 
	if (pArgument == NULL)
	{
		context.Complete();
		return 0; // Return value doesn't really matter, just need to return something.
	}

	OutputDebugString(L"Starting long running operation...\n"); // Just so we can see what's happening in the debug output window

	// Simulate running something for 10 seconds
	BOOL result = FALSE;
	for (int i = 0; i < 10; i++)
	{
		// WaitForSingleObject / WaitForMultipleObjects can be used if our long-running operation spends most of its
		// time on async-style function calls or doing looping computation work. If it spends most of its time in
		// long-running blocking operations then we can't do graceful cancellation handling and we'll have to
		// settle for the wait window forcibly terminating the thread if the user wants to cancel.
		result = WAIT_OBJECT_0 != WaitForSingleObject(context.hCancel, 1000) ? TRUE : FALSE;

		if (!result)
		{
			// Sleep(5000); // Uncomment to show forced cancellation / thread termination behaviour
			break;
		}

		wchar_t buf[3]; // Count the seconds in the debugger output window
		_itow_s(i, buf, 10);
		buf[2] = 0;
		OutputDebugString(buf);
		OutputDebugString(L"...\n");
	}

	OutputDebugString(L"Finishing long running operation\n");
	*pArgument = result; // Save the result so the caller knows if we completed normally or if it was cancelled.
	context.Complete();
	return 0;
}

// Message handler for main window.
INT_PTR CALLBACK MainWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND: // Handle button presses etc
		// Handle OK button
		if (LOWORD(wParam) == IDOK)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}

		// Handle Cancel button
		if (LOWORD(wParam) == IDC_START)
		{
			BOOL result = FALSE;
			WaitWindow wait(L"Running a 10 second operation...", LongRunningOperation, &result);

			// This function will only return when the operation completes or is cancelled, but in the meantime the main thread
			// will continue pumping messages on an inner, modal-dialog message pump - ensuring that any other top-level windows in
			// the application will continue responding, and no wait-cursor / greyed out windows will be shown.
			wait.DoModal(gInstance, hDlg); 

			const LPCWSTR text = result ? L"Success" : L"Fail";
			MessageBox(hDlg, text, text, MB_OK); // Pop-up to tell the user if it completed or was cancelled.
			return TRUE;
		}
		break;

	default: break;
	}

	return FALSE; // Return FALSE to tell Windows wew didn't handle this particular message and to apply default behaviour.
}

// The entry point to the program.
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	gInstance = hInstance; // Save the instance in a global for convenience
	// Show the main dialog
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, MainWndProc);
	return 0;
}

