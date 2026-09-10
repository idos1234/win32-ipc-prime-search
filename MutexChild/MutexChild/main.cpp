#include "header.h"

#define NUM_OF_THREADS 4

HANDLE threads[NUM_OF_THREADS];

bool isPrime(int n) {
	if (n <= 1) return false;
	for (int i = 2; i * i <= n; ++i) {
		if (n % i == 0) return false;
	}
	return true;
}

struct ThreadArgs {
	int start;
	int end;
	LPDWORD pBuffer;
	HANDLE mutex;
};

DWORD WINAPI checkPrimesInRange(LPVOID lpParam) {
	ThreadArgs* args = (ThreadArgs*)lpParam;

	for (int i = args->start; i <= args->end; ++i) {
		if (isPrime(i)) {
			WaitForSingleObject(args->mutex, INFINITE);
			args->pBuffer[args->pBuffer[80000]] = i;
			++args->pBuffer[80000];
			ReleaseMutex(args->mutex);
		}
	}

	delete args;

	return 0;
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		cerr << "Please provide the correct number of arguments." << endl;
		return 1;
	}

	//Get the file mapping object
	HANDLE hMapDataFile = OpenFileMappingA(
		FILE_MAP_ALL_ACCESS,
		FALSE,
		"Local\\MySharedMemory"
	);
	if (hMapDataFile == NULL) {
		cerr << "CreateFileMapping failed: " << GetLastError() << endl;
		return 1;
	}

	//Get the mutex
	HANDLE mutex = OpenMutexA(
		SYNCHRONIZE,
		FALSE,
		"Local\\MySharedMutex"
	);
	if (mutex == NULL) {
		cerr << "CreateMutex failed: " << GetLastError() << endl;
		CloseHandle(hMapDataFile);
		return 1;
	}

	//Get the shared memory buffer

	int bufferSize = sizeof(DWORD) * 80002; // 80000 primes + 1 for the index
	LPDWORD pBuffer = (LPDWORD)MapViewOfFile(
		hMapDataFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		bufferSize
	);
	if (pBuffer == NULL) {
		cerr << "MapViewOfFile failed: " << GetLastError() << endl;
		CloseHandle(hMapDataFile);
		CloseHandle(mutex);
		return 1;
	}


	WaitForSingleObject(mutex, INFINITE);
	int start = pBuffer[80001] * 10000 + 1;
	int end = (pBuffer[80001] + 1) * 10000;
	++pBuffer[80001];

	// Check if the start value is greater than the upper limit provided in argv[1]
	bool done = start > atoi(argv[1]);
	ReleaseMutex(mutex);
	while (!done) {
		for(int i = 0; i < NUM_OF_THREADS; ++i) {
			int blockSize = (end - start + 1) / NUM_OF_THREADS;
			ThreadArgs* args = new ThreadArgs();
			args->start = start + i * blockSize;
			args->end = args->start + blockSize - 1;
			args->pBuffer = pBuffer;
			args->mutex = mutex;

			threads[i] = CreateThread(
				NULL,
				0,
				(LPTHREAD_START_ROUTINE)checkPrimesInRange,
				args,
				0,
				NULL
			);
			if (threads[i] == NULL) {
				cerr << "CreateThread failed: " << GetLastError() << endl;
				return 1;
			}
		}

		// Get the next range of numbers to check
		WaitForSingleObject(mutex, INFINITE);
		start = pBuffer[80001] * 10000 + 1;
		end = (pBuffer[80001] + 1) * 10000;
		++pBuffer[80001];

		// Check if the start value is greater than the upper limit provided in argv[1]
		done = start > atoi(argv[1]);
		ReleaseMutex(mutex);

		// Wait for all threads to finish
		WaitForMultipleObjects(NUM_OF_THREADS, threads, TRUE, INFINITE);

		// Close thread handles
		for (int i = 0; i < NUM_OF_THREADS; ++i) {
			CloseHandle(threads[i]);
		}
	}

	return 0;
}