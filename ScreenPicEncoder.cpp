#include "ScreenPicEncoder.h"
NV_ENCODE_API_FUNCTION_LIST ScreenPicEncoder::NVENC_API_List = { 0 };
void* ScreenPicEncoder::nvencoder = NULL;

void ScreenPicEncoder::ScreenPicPacket::GetAndJumpOverSPSPPS(unsigned char* source, int max_len, unsigned char** buff_out_sps, int* len_sps, unsigned char** buff_out_pps, int* len_pps, int* over_sps_pps)
{
	int index_sps = 0;
	int index_pps = 0;
	int zero_timer = 0;
	//find sps:
	index_sps = JumpOverNextStartCode(source, 0, max_len - 1);
	if (index_sps == -1 || index_sps == max_len - 1) return;
	//find pps:
	index_pps = JumpOverNextStartCode(source, index_sps, max_len - 1);
	if (index_pps == -1 || index_sps == max_len - 1) return;
	//find pps end:
	int pps_end = JumpOverNextStartCode(source, index_pps, max_len - 1);
	if (pps_end == -1) return;
	//cal sps and pps len:
	*len_sps = (index_pps - 5) - index_sps + 1;
	*len_pps = (pps_end - 5) - index_pps + 1;
	//remove emulation prevention bytes:
	*buff_out_sps = remove_emulation_prevention_bytes(source + index_sps, *len_sps, len_sps);
	*buff_out_pps = remove_emulation_prevention_bytes(source + index_pps, *len_pps, len_pps);
	//jump over sps and pps:
	*over_sps_pps = pps_end;
}

int ScreenPicEncoder::ScreenPicPacket::JumpOverNextStartCode(unsigned char* source, int start, int end)
{
	int zero_timer = 0;
	for (int index = start; index < end - start + 1; index++)
	{
		if ((!source[index]) && (!source[index + 1]) && (!source[index + 2]) && (source[index + 3] == 1))
			return index + 4;
		else
			continue;
	}
	return -1;
}

unsigned char* ScreenPicEncoder::ScreenPicPacket::remove_emulation_prevention_bytes(unsigned char* buff, int len, int* out_len)
{
	unsigned char* new_buff = (unsigned char*)malloc(len);
	if (!buff || !new_buff) return NULL;
	int out_len_temp = 0;
	for (int i = 0; i < len; i++)
	{
		if (buff[i] == 3 && !buff[i - 1] && !buff[i - 2] && (buff[i + 1] >= 0 && buff[i + 1] <= 3))
			continue;
		else
			new_buff[out_len_temp++] = buff[i];
	}
	*out_len = out_len_temp;
	return new_buff;
}

bool ScreenPicEncoder::InitCodec(PENCODER_OPT opt, BlockQueue<ScreenPicPacket*>* encoded_packets)
{
	//initialize nvencoder:
	NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS session_params = {0};
	NV_ENC_PRESET_CONFIG preset_config = { NV_ENC_PRESET_CONFIG_VER,{NV_ENC_CONFIG_VER} };
	NV_ENC_CONFIG config = {0};
	NV_ENC_INITIALIZE_PARAMS encoder_init_params = {0};

	NVENC_API_List.version = NV_ENCODE_API_FUNCTION_LIST_VER;
	session_params.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
	session_params.device = opt->p_DirectXDevice;
	session_params.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
	session_params.apiVersion = NVENCAPI_VERSION;
	ENCODER_RET_ERR_HANDLE_RETURN_FALSE(NvEncodeAPICreateInstance(&this->NVENC_API_List), "nvenc create api instance failed!");
	ENCODER_RET_ERR_HANDLE_RETURN_FALSE(NVENC_API_List.nvEncOpenEncodeSessionEx(&session_params, &this->nvencoder),"nvenc open session failed!");
	ENCODER_RET_ERR_HANDLE_RETURN_FALSE(NVENC_API_List.nvEncGetEncodePresetConfig(this->nvencoder, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_DEFAULT_GUID, &preset_config), "nvenc get preset config failed");
	memcpy(&config, &preset_config.presetCfg, sizeof(config));
	config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
	config.rcParams.averageBitRate = opt->bitrate;
	config.gopLength = opt->gop_size;
	encoder_init_params.encodeConfig = &config;
	encoder_init_params.encodeConfig->version = NV_ENC_CONFIG_VER;
	encoder_init_params.encodeGUID = NV_ENC_CODEC_H264_GUID;
	encoder_init_params.presetGUID = NV_ENC_PRESET_DEFAULT_GUID;
	encoder_init_params.version = NV_ENC_INITIALIZE_PARAMS_VER;
	encoder_init_params.encodeWidth = opt->width;
	encoder_init_params.encodeHeight = opt->height;
	encoder_init_params.darWidth = opt->width;
	encoder_init_params.darHeight = opt->height;
	encoder_init_params.frameRateNum = opt->frame_rate.num;
	encoder_init_params.frameRateDen = opt->frame_rate.den; this->frame_rate = opt->frame_rate;
	encoder_init_params.enablePTD = 1;
	encoder_init_params.maxEncodeWidth = opt->width;
	encoder_init_params.maxEncodeHeight = opt->height;
	ENCODER_RET_ERR_HANDLE_RETURN_FALSE(NVENC_API_List.nvEncInitializeEncoder(this->nvencoder, &encoder_init_params), "nvenc init params failed");
	this->width = opt->width; this->height = opt->height;
	this->encoded_packets = encoded_packets;
	return true;
}

void ScreenPicEncoder::EncodeD3D11Texture(ID3D11Texture2D* texture)
{
	NV_ENC_REGISTER_RESOURCE resource = { 0 };
	NV_ENC_PIC_PARAMS pic_params = { 0 };
	NV_ENC_MAP_INPUT_RESOURCE mapResource = { NV_ENC_MAP_INPUT_RESOURCE_VER };
	NV_ENC_CREATE_BITSTREAM_BUFFER BitstreamBuffer = { NV_ENC_CREATE_BITSTREAM_BUFFER_VER };
	NV_ENC_LOCK_BITSTREAM lockBitstreamData = { NV_ENC_LOCK_BITSTREAM_VER };
	unsigned char* out_data = NULL;
	unsigned char* out_data_last = NULL;
	int datasize = 0;
	ScreenPicPacket* videoPacket = NULL;
	resource.version = NV_ENC_REGISTER_RESOURCE_VER;
	resource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
	resource.bufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
	resource.bufferUsage = NV_ENC_INPUT_IMAGE;
	resource.resourceToRegister = texture;
	resource.width = this->width;
	resource.height = this->height;
	resource.pitch = 0;
	ENCODER_RET_ERR_HANDLE_CLEAR(NVENC_API_List.nvEncRegisterResource(this->nvencoder, &resource), "nvenc register texture resource failed!");
	mapResource.registeredResource = resource.registeredResource;
	ENCODER_RET_ERR_HANDLE_CLEAR(NVENC_API_List.nvEncMapInputResource(this->nvencoder, &mapResource), "nvenc mapresource failed!");
	ENCODER_RET_ERR_HANDLE_CLEAR(NVENC_API_List.nvEncCreateBitstreamBuffer(this->nvencoder, &BitstreamBuffer), "nvenc create bitstream buffer failed!");
	pic_params.version = NV_ENC_PIC_PARAMS_VER;
	pic_params.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
	pic_params.inputBuffer = mapResource.mappedResource;
	pic_params.bufferFmt = NV_ENC_BUFFER_FORMAT_ARGB;
	pic_params.inputWidth = 1920;
	pic_params.inputHeight = 1080;
	pic_params.outputBitstream = BitstreamBuffer.bitstreamBuffer;
	pic_params.inputTimeStamp = this->PresentTimeStamp; this->PresentTimeStamp += this->frame_rate.get();
	ENCODER_RET_ERR_HANDLE_CLEAR(NVENC_API_List.nvEncEncodePicture(this->nvencoder, &pic_params), "encode texture failed!");
	lockBitstreamData.outputBitstream = BitstreamBuffer.bitstreamBuffer;
	lockBitstreamData.doNotWait = 0;
	ENCODER_RET_ERR_HANDLE_CLEAR(NVENC_API_List.nvEncLockBitstream(this->nvencoder, &lockBitstreamData), "error occurred while getting encoded data!");
	datasize = lockBitstreamData.bitstreamSizeInBytes;
	out_data = (unsigned char*)malloc(datasize);
	memcpy(out_data, lockBitstreamData.bitstreamBufferPtr, datasize);
	//add to encoded queue:
	videoPacket = new ScreenPicPacket(out_data, datasize, lockBitstreamData.pictureType, lockBitstreamData.outputTimeStamp);
	this->encoded_packets->push(videoPacket);
clear:
	NVENC_API_List.nvEncUnlockBitstream(this->nvencoder, BitstreamBuffer.bitstreamBuffer);
	NVENC_API_List.nvEncUnmapInputResource(this->nvencoder, mapResource.mappedResource);
	NVENC_API_List.nvEncUnregisterResource(this->nvencoder, resource.registeredResource);
	NVENC_API_List.nvEncDestroyBitstreamBuffer(this->nvencoder, BitstreamBuffer.bitstreamBuffer);

	return; 
}

bool ScreenPicEncoder::DumpLastEncode()
{
	if (!this->last_packet)
		return false;
	int last_buffer_size = last_packet->getDataSize();
	last_packet->TimeStamp += 16;
	unsigned char* dumpBuffer = (unsigned char*)malloc(last_buffer_size);
	memcpy(dumpBuffer, last_packet->getData(), last_buffer_size);
	ScreenPicPacket* dumpPacket = new ScreenPicPacket(dumpBuffer,last_buffer_size,last_packet->getType(),last_packet->getTimeStamp());
	this->encoded_packets->push(dumpPacket);
	return true;
}
