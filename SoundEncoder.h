#pragma once
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include "BlockQueue.h"
#include "ScreenPicCommon.h"
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avcodec.lib")
#define FFMPEG_RET_ERR_HANDLE_RETURN_FALSE(ret,info) RETURN_FALSE_IF_FAILED((ret<0),info)
#define FFMPEG_RET_ERR_HANDLE_CLEAR(ret,info) GOTO_CLEAR_IF_FAILED((ret<0),info)
#define FFMPEG_OBJ_ERR_HANDLE_RETURN_FALSE(obj,info) RETURN_FALSE_IF_FAILED((!obj),info)
#define FFMPEG_OBJ_ERR_HANDLE_CLEAR(obj,info) GOTO_CLEAR_IF_FAILED((!obj),info)
class SoundEncoder
{
public:
	class SoundPacket
	{
	private:
		AVFrame* audioFrame = NULL;
		unsigned char* data = NULL;
		int data_size = 0;
		int timestamp = 0;
	public:
		~SoundPacket() { av_frame_free(&this->audioFrame); }
		SoundPacket(unsigned char* data, int data_size) { this->data = data; this->data_size = data_size; }
		unsigned char* getdata()const { return this->data; }
		int getdatasize()const { return this->data_size; }
	};
	typedef struct
	{
		int sample_rate;
		int nb_channel;
	}ENCODER_OPT, * PENCODER_OPT;
private:
	BlockQueue<SoundPacket*>* encoded_packets = NULL;
	AVCodec* codec = NULL;
	AVCodecContext* codecContext =  NULL;
public:
	bool InitEncoder(PENCODER_OPT opt, BlockQueue<SoundPacket*>* encoded_packets);
	void EncodeSound(unsigned char* sound_data,int size);
	int getframesize() { return this->codecContext->frame_size; }
};

