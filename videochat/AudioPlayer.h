#pragma once
#include <vector>
#include <queue>
#include <mutex>
#include <windows.h>
#pragma comment(lib, "Winmm")

class AudioPlayer
{
private:
	const size_t m_sampleRate;
	bool m_playing = false;
	HWAVEOUT m_hWaveOut = NULL;
	HANDLE m_hWaveOutThread = NULL;
	DWORD m_dwThreadId = NULL;
	std::mutex m_queueLock;
	std::queue<LPWAVEHDR> m_samplesQueue;
	static DWORD WINAPI waveOutProc(LPVOID arg);

	AudioPlayer(const AudioPlayer& rec) = delete;
	AudioPlayer(const AudioPlayer&& rec) = delete;
	AudioPlayer& operator=(const AudioPlayer& rec) = delete;
	AudioPlayer& operator=(const AudioPlayer&& rec) = delete;

public:
	AudioPlayer(size_t sampleRate = 44100);

	bool start();

	void stop();

	bool addSamples(const std::vector<short>& samples);

	bool isOutDeviceAvailable();

	bool isPlaying();

	~AudioPlayer();
};