#include "WaitWindow.h"
#include "resource.h"

// Creates the thread with an RAII pattern
OperationThread::OperationThread(LPTHREAD_START_ROUTINE operation, OperationContext& context) :
	m_hThread(NULL),
	m_nThreadId(0)
{
	m_hThread = CreateThread(
		NULL,
		0,
		operation,
		&context,
		0,
		&m_nThreadId);

	if (m_hThread == NULL)
		throw std::exception("Could not create operation thread!");
}

// Clean up the thread handle
OperationThread::~OperationThread() { CloseHandle(m_hThread); }

WaitWindow::WaitWindow(LPCWSTR szMessage, LPTHREAD_START_ROUTINE pOperation, LPVOID pArgument) :
	m_szMessage(szMessage),
	m_hWnd(NULL),
	m_pOperation(pOperation)
{
	m_context.pArgument = pArgument;
}

void WaitWindow::InitializeWindow(HWND hWnd)
{
	m_hWnd = hWnd;
	SetDlgItemText(m_hWnd, IDC_MESSAGE, m_szMessage);
	m_context.hWaitWindow = m_hWnd;
	m_context.hCancel = m_mreCancel.GetHandle();
	m_thread = std::make_unique<OperationThread>(m_pOperation, m_context); // with an auto_ptr this will be "m_thread.reset(new OperationThread(...));"
}

int WaitWindow::DoModal(HINSTANCE hInstance, HWND hParent)
{
	// This will pass the this pointer as the lParam to WM_INITDIALOG, ensuring that our
	// static WindowProc can save it in the user long-pointer slot for use by later messages.
	return DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_WAIT), hParent, StaticWindowProc, (LPARAM)this);
}

INT_PTR CALLBACK WaitWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	// Note: WM_INITDIALOG handled by static proc
	switch (message)
	{
	case WM_COMMAND: // Handle button clicks
		// The dialog template doesn't actually contain an OK button, but 
		// we send a message with IDOK when the long-running operation completes.
		if (LOWORD(wParam) == IDOK)
		{
			OutputDebugString(L"Long running operation completed\n");
			EndDialog(m_hWnd, LOWORD(wParam)); // Close the dialog and return control to whoever called DoModal.
			return TRUE;
		}

		// If the user got impatient and clicked the cancel button...
		if (LOWORD(wParam) == IDCANCEL)
		{
			OutputDebugString(L"Cancelling long running operation...\n");
			m_mreCancel.Set(); // Let the thread know we want to cancel it
			SetTimer(m_hWnd, 1, 2000, NULL); // Give the thread 2 seconds to exit or we do it forcefully
			return TRUE;
		}
		break;
	case WM_TIMER:
		m_thread->Terminate();
		OutputDebugString(L"Thread did not respond in time, forcefully terminated.\n");
		EndDialog(m_hWnd, LOWORD(wParam)); // Close the dialog and return control to whoever called DoModal.
		return TRUE;

	default: break;
	}

	return FALSE; // Let Windows know that we didn't handle this particular message, so it should apply its default behaviours.
}

INT_PTR CALLBACK WaitWindow::StaticWindowProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_INITDIALOG) 
	{
		// First message for the dialog, use it to setup the this pointer in a Windows storage location.
		if (lParam == NULL)
		{
			MessageBox(hDlg,
				L"No instance pointer passed when constructing wait dialog",
				L"Error",
				MB_OK | MB_ICONERROR);
			return EndDialog(hDlg, 0);
		}

		WaitWindow* window = (WaitWindow*)lParam;
		SetWindowLongPtr(hDlg, DWLP_USER, lParam);
		window->InitializeWindow(hDlg);
		return TRUE;
	}

	// For any other message, try and get the this-pointer then delegate to the instance window proc
	WaitWindow* window = (WaitWindow*)GetWindowLongPtr(hDlg, DWLP_USER);
	if (window != NULL)
		return window->WindowProc(message, wParam, lParam);

	// Occasionally a message is sent before the WM_INITDIALOG, in those
	// cases we should return FALSE to let Windows apply its default behaviour.
	return FALSE;
}
