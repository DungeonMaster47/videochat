#include "AudioRecorder.h"

DWORD WINAPI AudioRecorder::waveInProc(LPVOID arg)
{
	MSG	msg;
	AudioRecorder* instance = (AudioRecorder*)arg;
	std::queue <std::future<void>> futures;
	while (GetMessage(&msg, 0, 0, 0) == 1)
	{
		switch (msg.message)
		{
		case MM_WIM_CLOSE:
		{
			break;
		}
		case MM_WIM_DATA:
		{
			LPWAVEHDR lpWaveHeader = (LPWAVEHDR)msg.lParam;
			HWAVEIN hwi = (HWAVEIN)msg.wParam;
			std::vector<short> samples;

			samples.resize(lpWaveHeader->dwBytesRecorded / sizeof(short));
			memcpy(samples.data(), lpWaveHeader->lpData, lpWaveHeader->dwBytesRecorded);

			if (instance->m_recording)
			{
				waveInPrepareHeader(hwi, lpWaveHeader, sizeof(WAVEHDR));
				waveInAddBuffer(hwi, lpWaveHeader, sizeof(WAVEHDR));
			}

			if (instance->m_processingCallback == nullptr)
			{
				std::scoped_lock lock(instance->m_queueLock);
				instance->m_samplesQueue.push(std::move(samples));
			}
			else
			{
				futures.push(std::async(std::launch::async, instance->m_processingCallback, samples));

				if (futures.front().wait_for(std::chrono::seconds(0)) == std::future_status::ready)
				{
					futures.pop();
				}
			}

			continue;
		}
		case MM_WIM_OPEN:
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

AudioRecorder::AudioRecorder(size_t bufNum, size_t sampleRate) : m_bufNum(bufNum), m_sampleRate(sampleRate), m_bufSize(sampleRate/10)
{
	m_buffers = new short*[m_bufNum];
	m_waveHeaders = new WAVEHDR[m_bufNum];

	for (size_t i = 0; i < m_bufNum; ++i)
	{
		m_buffers[i] = new short[m_bufSize];

		m_waveHeaders[i] = { };
		m_waveHeaders[i].dwBufferLength = m_bufSize * sizeof(short);
		m_waveHeaders[i].lpData = (LPSTR)m_buffers[i];
	}
}

bool AudioRecorder::start()
{
	if (!isRecordingDeviceAvailable())
		throw std::exception("No recording device available");

	m_recording = true;
	m_hWaveInThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)waveInProc, this, 0, &m_dwThreadId);
	WAVEFORMATEX waveFormat = {};
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.wBitsPerSample = 16;
	waveFormat.nChannels = 1;
	waveFormat.nSamplesPerSec = m_sampleRate;
	waveFormat.nBlockAlign = waveFormat.nChannels*waveFormat.wBitsPerSample / 8;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec*waveFormat.nBlockAlign;

	MMRESULT result = waveInOpen(&m_hWaveIn, 0, &waveFormat, (DWORD_PTR)m_dwThreadId, 0, CALLBACK_THREAD);

	if (result != MMSYSERR_NOERROR)
	{
		stop();
		return false;
	}

	for (size_t i = 0; i < m_bufNum; ++i)
	{
		MMRESULT result = waveInPrepareHeader(m_hWaveIn, &m_waveHeaders[i], sizeof(WAVEHDR));

		if (result != MMSYSERR_NOERROR)
		{
			stop();
			return false;
		}

		result = waveInAddBuffer(m_hWaveIn, &m_waveHeaders[i], sizeof(WAVEHDR));

		if (result != MMSYSERR_NOERROR)
		{
			stop();
			return false;
		}
	}

	if (result != MMSYSERR_NOERROR)
	{
		stop();
		return false;
	}

	result = waveInStart(m_hWaveIn);

	if (result != MMSYSERR_NOERROR)
	{
		stop();
		return false;
	}

	return true;
}

void AudioRecorder::stop()
{
	if (m_hWaveIn == NULL)
		return;
	m_recording = false;
	waveInReset(m_hWaveIn);

	PostThreadMessage(m_dwThreadId, MM_WIM_CLOSE, NULL, NULL);
	WaitForSingleObject(m_hWaveInThread, INFINITE);

	for (size_t i = 0; i < m_bufNum; ++i)
	{
		waveInUnprepareHeader(m_hWaveIn, &m_waveHeaders[i], sizeof(WAVEHDR));
	}

	while (waveInClose(m_hWaveIn) == WAVERR_STILLPLAYING);

	CloseHandle(m_hWaveInThread);
	m_hWaveIn = NULL;
}

bool AudioRecorder::isEmpty() const
{
	std::scoped_lock lock(m_queueLock);
	return m_samplesQueue.empty();
}

std::vector<short> AudioRecorder::getSamples()
{
	std::scoped_lock lock(m_queueLock);
	auto samples = m_samplesQueue.front();
	m_samplesQueue.pop();
	return samples;
}

void AudioRecorder::setProcessingCallback(std::function<void(std::vector<short>)> callback)
{
	m_processingCallback = callback;
}

bool AudioRecorder::isRecordingDeviceAvailable()
{
	return (waveInGetNumDevs() > 0);
}

bool AudioRecorder::isRecording()
{
	return m_recording;
}

AudioRecorder::~AudioRecorder()
{
	stop();
	for (size_t i = 0; i < m_bufNum; ++i)
	{
		delete[] m_buffers[i];
	}
	delete[] m_buffers;
	delete[] m_waveHeaders;
}