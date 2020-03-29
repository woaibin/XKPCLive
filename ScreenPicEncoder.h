#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <nvcuvid.h>
#include <nvEncodeAPI.h>
#include <thread>
#include "BlockQueue.h"
#pragma comment(lib,"nvcuvid.lib")
#pragma comment(lib,"nvencodeapi.lib")
#include "ScreenPicCommon.h"
#define ENCODER_RET_ERR_HANDLE_RETURN_FALSE(ret,info) RETURN_FALSE_IF_FAILED((ret),info)
#define ENCODER_RET_ERR_HANDLE_CLEAR(ret,info) GOTO_CLEAR_IF_FAILED((ret),info)

class ScreenPicEncoder
{
public:
	class ScreenPicPacket
	{
	public:
		#define PIC_TYPE_I_SLICE NV_ENC_PIC_TYPE_I
		#define PIC_TYPE_P_SLICE NV_ENC_PIC_TYPE_P
		#define PIC_TYPE_IDR_SLICE NV_ENC_PIC_TYPE_IDR
		#define PIC_TYPE_OTHER_SLICE -1
		void* data = NULL;
		int size;
		int PictureType;
		int TimeStamp;
	public:
		ScreenPicPacket(void* data, int size,int PictureType, int TimeStamp)
		{ 
			this->data = data; this->size = size; this->PictureType = PictureType; this->TimeStamp = TimeStamp;
		}
		unsigned char* getData() const { return (unsigned char*)this->data; }
		int getDataSize() const { return this->size; }
		int getType()const { return this->PictureType; }
		int getTimeStamp()const { return this->TimeStamp; }
		~ScreenPicPacket()
		{
			free(data);
		}
		static void GetAndJumpOverSPSPPS(unsigned char* source, int max_len, unsigned char** buff_out_sps, int* len_sps, unsigned char** buff_out_pps, int* len_pps, int* over_sps_pps);
		static int JumpOverNextStartCode(unsigned char* source, int start, int end);
		static unsigned char* remove_emulation_prevention_bytes(unsigned char* buff, int len, int* out_len);
	};
	typedef struct
	{
		int num;
		int den;
		int get()const { return (den / (float)num) * 1000; }
	}FRAMERATE;
	typedef struct 
	{
		int width; int height;
		int bitrate;
		FRAMERATE frame_rate;
		int gop_size;
		int max_b_frame;
		DXGI_FORMAT pix_fmt;
		ID3D11Device* p_DirectXDevice;
	}ENCODER_OPT, * PENCODER_OPT;
	typedef struct  encode_resource
	{
		NV_ENC_REGISTERED_PTR registeredResource;
		NV_ENC_INPUT_PTR mappedResource;
		NV_ENC_OUTPUT_PTR outputBitStream;
		HANDLE Event;
		encode_resource(NV_ENC_REGISTERED_PTR registeredResource, NV_ENC_INPUT_PTR mappedResource, NV_ENC_OUTPUT_PTR outputBitStream, HANDLE Event)
		{
			this->registeredResource = registeredResource;
			this->mappedResource = mappedResource;
			this->outputBitStream = outputBitStream;
			this->Event = Event;
		}
	}ENCODE_RESOURCE,*PENCODE_RESOURCE;
private:
	static NV_ENCODE_API_FUNCTION_LIST NVENC_API_List;
	static void* nvencoder;
	int width = 0, height = 0; FRAMERATE frame_rate;
	float PresentTimeStamp = 0;
	BlockQueue<PENCODE_RESOURCE> encoding_resources;
	BlockQueue<ScreenPicPacket*>*encoded_packets;
	ScreenPicPacket* last_packet = NULL;
	std::thread *querythread;
public:
	bool InitCodec(PENCODER_OPT opt, BlockQueue<ScreenPicPacket*>* encoded_packets);
	void EncodeD3D11Texture(ID3D11Texture2D* texture);
	bool DumpLastEncode();
};