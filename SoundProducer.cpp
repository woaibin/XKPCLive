#include "SoundProducer.h"
bool SoundProducer::InitProducer(BlockQueue<SoundEncoder::SoundPacket*>* encoded_packets)
{
	IMMDeviceEnumerator* pEnumerator = NULL;
	SoundEncoder::PENCODER_OPT opt = new SoundEncoder::ENCODER_OPT();
	//init sound sampler:
	int samples_per_frame = 1024;
	PRODUCER_RET_ERR_HANDLE_RETURN_FALSE(CoInitialize(NULL), "coinit failed!");
	PRODUCER_RET_ERR_HANDLE_RETURN_FALSE(CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator)), "cocreate enumerator failed!");
	PRODUCER_RET_ERR_HANDLE_RETURN_FALSE(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &this->pDevide_Loopback), "get default loopback device failed!"); if (pEnumerator) pEnumerator->Release();
	PRODUCER_RET_ERR_HANDLE_RETURN_FALSE(this->pDevide_Loopback->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&this->pAudioClient_Loopback), "loop back device activate failed!");
	PRODUCER_RET_ERR_HANDLE_RETURN_FALSE(this->pAudioClient_Loopback->GetMixFormat(&this->pWFX_Loopback), "loop back device get mix format failed!");
	PRODUCER_RET_ERR_HANDLE_RETURN_FALSE(this->pAudioClient_Loopback->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, samples_per_frame/this->pWFX_Loopback->nSamplesPerSec, 0, this->pWFX_Loopback, NULL), "loop back device init failed!");
	PRODUCER_RET_ERR_HANDLE_RETURN_FALSE(this->pAudioClient_Loopback->GetBufferSize(&this->bufferFrameCount), "loop back device get buffer size failed!"); this->duration = this->bufferFrameCount / this->pWFX_Loopback->nSamplesPerSec;
	PRODUCER_RET_ERR_HANDLE_RETURN_FALSE(this->pAudioClient_Loopback->GetService(IID_PPV_ARGS(&this->pAudioCapClient_Loopback)), "loop back device get cap client failed!");
	//init sound encoder:
	opt->nb_channel = 2;
	opt->sample_rate = this->pWFX_Loopback->nSamplesPerSec;
	encoder.InitEncoder(opt, encoded_packets);
	return true;
}

bool SoundProducer::StartProducingSound()
{
	SoundProducingThread = new std::thread(&SoundProducer::ThreadFunc_ProducingSound, this);
	return true;
}

void SoundProducer::ThreadFunc_ProducingSound()
{
	while (1)
	{

	}
}
