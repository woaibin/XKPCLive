#pragma once
#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <audiopolicy.h>
#include <devicetopology.h>
#include <devicetopology.h>
#include <AudioSessionTypes.h>
#include <thread>
#include "ScreenPicCommon.h"
#include "SoundEncoder.h"
#define PRODUCER_RET_ERR_HANDLE_RETURN_FALSE(ret,info) RETURN_FALSE_IF_FAILED(FAILED(ret),info)
#define PRODUCER_RET_ERR_HANDLE_CLEAR(ret,info) GOTO_CLEAR_IF_FAILED(FAILED(ret),info)
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000
class SoundProducer
{
private:
	IMMDevice* pDevice_VoiceInput = NULL;
	IMMDevice* pDevide_Loopback = NULL;
	IAudioClient* pAudioClient_VoiceInput = NULL;
	IAudioClient* pAudioClient_Loopback = NULL;
	IAudioCaptureClient* pAudioCapClient_VoiceInput = NULL;
	IAudioCaptureClient* pAudioCapClient_Loopback = NULL;
	WAVEFORMATEX* pWFX_VoiceInput = NULL;
	WAVEFORMATEX* pWFX_Loopback = NULL;
	UINT32 bufferFrameCount;
	std::thread *SoundProducingThread;
	SoundEncoder encoder;
	int duration;
public:
	bool InitProducer(BlockQueue<SoundEncoder::SoundPacket*>* encoded_packets);
	bool StartProducingSound();
private:
	void ThreadFunc_ProducingSound();
}; 

