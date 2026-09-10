#include <Windows.h>
#include <iostream>
#include <string>
#include <chrono>

using namespace std;
using namespace std::chrono;

#define EXE_FILENAME "C:\\Users\\sulim\\source\\repos\\MutexChild\\x64\\Debug\\MutexChild.exe"

#define FULL_SIZE 1000000
#define NUM_PROCESSES 4

int main() {   
    // ****************************************************************
	//Create the shared data - the last parameter is the current index of the shared data
    // ****************************************************************
    SIZE_T dataSize = 80002 * sizeof(DWORD);

	// Create a file mapping object for shared memory
    HANDLE hMapDataFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        dataSize,
        "Local\\MySharedMemory"
	);

    if (hMapDataFile == NULL) {
        cerr << "CreateFileMapping failed: " << GetLastError() << endl;
        return 1;
    }

	// Create one mutex for all the shared data
    HANDLE mutex = CreateMutexA(
        NULL, // Default security attributes
        FALSE, // Initially not owned
        "Local\\MySharedMutex"
    );

    if(mutex == NULL) {
        cerr << "CreateMutex failed: " << GetLastError() << endl;
        CloseHandle(hMapDataFile);
        return 1;
	}

	// Map the shared memory into the process's address space
    LPDWORD pBuffer = (LPDWORD)MapViewOfFile(
        hMapDataFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
		dataSize
    );

    if (pBuffer == NULL) {
        cerr << "MapViewOfFile failed: " << GetLastError() << endl;
        CloseHandle(hMapDataFile);
        return 1;
    }
	pBuffer[80000] = 0; // Initialize the current index to 0
	pBuffer[80001] = 0; // Initialize the current block index to 0

    

    // ****************************************************************
	// Create child processes to write to the shared memory
    // ****************************************************************

    auto start = high_resolution_clock::now();


    STARTUPINFOA si;
    PROCESS_INFORMATION pi[NUM_PROCESSES];
    
    for (int i = 0; i < NUM_PROCESSES; ++i)
    {
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si); // Must set the structure size
        ZeroMemory(&pi[i], sizeof(pi[i]));

        char param[256];
        sprintf_s(param, sizeof(param), "%s %d", EXE_FILENAME, FULL_SIZE);
        bool success = CreateProcessA(
            NULL,           // Application name (NULL means use command line string)
            param,          // Command line arguments
            NULL,           // Process security attributes (NULL = default)
            NULL,           // Thread security attributes (NULL = default)
            FALSE,          // Handle inheritance (FALSE = child doesn't inherit handles)
            0,              // Process creation flags (e.g., CREATE_NO_WINDOW)
            NULL,           // Environment block (NULL = inherit parent environment)
            NULL,           // Current directory (NULL = inherit parent directory)
            &si,            // Pointer to STARTUPINFO
            &pi[i]             // Pointer to PROCESS_INFORMATION (receives handles)
        );

        if (!success) {
            cout << "CreateProcess failed: " << GetLastError() << std::endl;
        }

    }

	// Wait for all child processes to finish
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        WaitForSingleObject(pi[i].hProcess, INFINITE);
    }

    auto stop = high_resolution_clock::now();    
    auto duration = duration_cast<microseconds>(stop - start);
    std::cout << "Execution time: " << duration.count() << " microseconds\n";


	// Cleanup: Close handles to child processes and threads
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        CloseHandle(pi[i].hProcess);
        CloseHandle(pi[i].hThread);
    }

	// Cleanup: Close mutex handles
	CloseHandle(mutex);

	// Cleanup: Unmap the shared memory and close the file mapping handle
    UnmapViewOfFile(pBuffer);
	CloseHandle(hMapDataFile);

	return 0;
}