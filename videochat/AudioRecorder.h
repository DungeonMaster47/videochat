#pragma once
#include <future>
#include <mutex>
#include <functional>
#include <vector>
#include <queue>
#include <windows.h>
#pragma comment(lib, "Winmm")

class AudioRecorder
{
private:
	const size_t m_bufNum;
	const size_t m_sampleRate;
	const size_t m_bufSize;
	bool m_recording = false;
	HWAVEIN m_hWaveIn = NULL;
	short** m_buffers;
	WAVEHDR* m_waveHeaders;
	mutable std::mutex m_queueLock;
	std::queue<std::vector<short>> m_samplesQueue;
	HANDLE m_hWaveInThread = NULL;
	DWORD m_dwThreadId = NULL;
	std::function<void(std::vector<short>)> m_processingCallback = nullptr;
	static DWORD WINAPI waveInProc(LPVOID arg);

	AudioRecorder(const AudioRecorder& rec) = delete;
	AudioRecorder(const AudioRecorder&& rec) = delete;
	AudioRecorder& operator=(const AudioRecorder& rec) = delete;
	AudioRecorder& operator=(const AudioRecorder&& rec) = delete;

public:
	AudioRecorder(size_t bufNum = 2, size_t sampleRate = 44100);

	bool start();

	void stop();

	bool isEmpty() const;

	std::vector<short> getSamples();

	void setProcessingCallback(std::function<void(std::vector<short>)> callback);

	bool isRecordingDeviceAvailable();

	bool isRecording();

	~AudioRecorder();
};