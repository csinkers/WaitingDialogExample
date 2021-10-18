#pragma once
#include "framework.h"
#include <memory>

class ManualResetEvent // We should already have a CEvent class in a couple of places that does this stuff.
{
	HANDLE m_hEvent;
public:
	ManualResetEvent() : m_hEvent(CreateEvent(NULL, TRUE, FALSE, NULL)) { }
	~ManualResetEvent() { CloseHandle(m_hEvent); }
	HANDLE GetHandle() const { return m_hEvent; }
	DWORD Wait(DWORD timeoutMilliseconds) const { return WaitForSingleObject(m_hEvent, timeoutMilliseconds); }
	void Set() { SetEvent(m_hEvent); }
	void Reset() { ResetEvent(m_hEvent); }
};

// Holds info the long-running operation requires to notify the window when it's done, as well as receive cancellation requests.
struct OperationContext 
{
	OperationContext() : hWaitWindow(NULL), pArgument(NULL), hCancel(NULL) { }

	HWND hWaitWindow; // The handle of the wait window

	// The parameter to the long-running operation. Typically a pointer to a struct or whatever
	// caller needs to ensure it matches up with what the particular operation function expects, and is
	// responsible for cleanup.
	LPVOID pArgument; 

	// The handle of a manual reset event that the wait window will set to request cancellation of the long-running operation.
	// If the operation uses blocking APIs that prevent the use of WaitForMultipleObjects etc to wait for completion + cancellation then
	// the wait window will forcibly terminate the thread a couple of seconds after clicking cancel.
	HANDLE hCancel;

	// Called by the long-running operation, sends a message back to the wait window telling it we're done.
	void Complete() const { PostMessage(hWaitWindow, WM_COMMAND, IDOK, NULL); }
};

class OperationThread // RAII class to ensure the thread handle is always closed when the wait window is disposed
{
	HANDLE m_hThread;
	DWORD m_nThreadId;
public:
	OperationThread(LPTHREAD_START_ROUTINE operation, OperationContext& context);
	void Terminate() { TerminateThread(m_hThread, 0); }
	virtual ~OperationThread();
};

class WaitWindow
{
	HWND m_hWnd;
	LPCWSTR m_szMessage; // The message to show while waiting
	OperationContext m_context; // The context that is passed to the long-running operation

	// A smart-pointer to the worker thread (in VS2008 a std::auto_ptr will have to be used instead as unique_ptr is new in C++11)
	std::unique_ptr<OperationThread> m_thread;

	LPTHREAD_START_ROUTINE m_pOperation; // A pointer to the function that implements the long-running operation.
	ManualResetEvent m_mreCancel;

	void InitializeWindow(HWND hWnd); // Called when the WM_INITDIALOG message is received and we find out what our window handle is. Kicks off the worker thread.
	INT_PTR CALLBACK WindowProc(UINT message, WPARAM wParam, LPARAM lParam); // Handle all messages other than WM_INITDIALOG
	static INT_PTR CALLBACK StaticWindowProc( // Handle WM_INITDIALOG and the mapping of hWnds to WaitWindow instances.
		HWND hDlg,
		UINT message,
		WPARAM wParam,
		LPARAM lParam);

public:
	WaitWindow(LPCWSTR szMessage, LPTHREAD_START_ROUTINE pOperation, LPVOID pArgument);
	// Actually displays the window and waits for it to close (but keeps pumping messages for other top-level windows in the background)
	int DoModal(HINSTANCE hInstance, HWND hParent); 
};

