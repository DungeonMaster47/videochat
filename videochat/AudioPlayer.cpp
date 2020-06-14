#include "AudioPlayer.h"

DWORD WINAPI AudioPlayer::waveOutProc(LPVOID arg)
{
	MSG	msg;
	AudioPlayer* instance = (AudioPlayer*)arg;
	while (GetMessage(&msg, 0, 0, 0) == 1)
	{
		switch (msg.message)
		{
		case MM_WOM_CLOSE:
		{
			break;
		}
		case MM_WOM_DONE:
		{
			LPWAVEHDR lpWaveHeader = (LPWAVEHDR)msg.lParam;

			if (instance->m_playing)
			{
				waveOutUnprepareHeader(instance->m_hWaveOut, lpWaveHeader, sizeof(WAVEHDR));

				delete[] reinterpret_cast<short*>(lpWaveHeader->lpData);
				delete lpWaveHeader;

				instance->m_samplesQueue.pop();
			}

			continue;
		}
		case MM_WOM_OPEN:
		{
			continue;
		}
		default:
		{
			continue;
		}
		}
		break;
	}

	return (0);
}

AudioPlayer::AudioPlayer(size_t sampleRate) : m_sampleRate(sampleRate)
{

}

bool AudioPlayer::start()
{
	if (!isOutDeviceAvailable())
		throw std::exception("No recording device available");

	m_playing = true;
	m_hWaveOutThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)waveOutProc, this, 0, &m_dwThreadId);

	WAVEFORMATEX waveFormat = {};
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.wBitsPerSample = 16;
	waveFormat.nChannels = 1;
	waveFormat.nSamplesPerSec = m_sampleRate;
	waveFormat.nBlockAlign = waveFormat.nChannels*waveFormat.wBitsPerSample / 8;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec*waveFormat.nBlockAlign;

	MMRESULT result = waveOutOpen(&m_hWaveOut, 0, &waveFormat, m_dwThreadId, 0, CALLBACK_THREAD);

	if (result != MMSYSERR_NOERROR)
	{
		stop();
		return false;
	}

	return true;
}

void AudioPlayer::stop()
{
	if (m_hWaveOut == NULL)
		return;
	m_playing = false;
	waveOutReset(m_hWaveOut);

	PostThreadMessage(m_dwThreadId, MM_WOM_CLOSE, NULL, NULL);
	WaitForSingleObject(m_hWaveOutThread, INFINITE);

	while (!m_samplesQueue.empty())
	{
		waveOutUnprepareHeader(m_hWaveOut, m_samplesQueue.front(), sizeof(WAVEHDR));
		delete[] reinterpret_cast<short*>(m_samplesQueue.front()->lpData);
		delete m_samplesQueue.front();

		m_samplesQueue.pop();
	}

	while (waveOutClose(m_hWaveOut) == WAVERR_STILLPLAYING);

	CloseHandle(m_hWaveOutThread);
	m_hWaveOut = NULL;
}

bool AudioPlayer::addSamples(const std::vector<short>& samples)
{
	if (!m_playing)
		return false;
	LPWAVEHDR lpWaveHeader = new WAVEHDR;
	lpWaveHeader->dwBufferLength = samples.size() * sizeof(short);
	lpWaveHeader->lpData = reinterpret_cast<LPSTR>(new short[samples.size()]);
	lpWaveHeader->dwFlags = 0L;
	lpWaveHeader->dwLoops = 0L;
	if (lpWaveHeader->lpData == NULL)
	{
		delete[] reinterpret_cast<short*>(lpWaveHeader->lpData);
		return false;
	}

	memcpy(lpWaveHeader->lpData, samples.data(), samples.size() * sizeof(short));

	std::scoped_lock lock(m_queueLock);
	m_samplesQueue.push(lpWaveHeader);
	MMRESULT res;
	if (res = waveOutPrepareHeader(m_hWaveOut, lpWaveHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
	{
		delete[] reinterpret_cast<short*>(lpWaveHeader->lpData);
		delete lpWaveHeader;
		return false;
	}

	if (res = waveOutWrite(m_hWaveOut, lpWaveHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
	{
		delete[] reinterpret_cast<short*>(lpWaveHeader->lpData);
		delete lpWaveHeader;
		return false;
	}

	return true;
}

bool AudioPlayer::isOutDeviceAvailable()
{
	return waveOutGetNumDevs() > 0;
}

bool AudioPlayer::isPlaying()
{
	return m_playing;
}

AudioPlayer::~AudioPlayer()
{
	stop();
}