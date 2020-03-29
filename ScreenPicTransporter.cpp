#include "ScreenPicTransporter.h"
#define RTMP_HEAD_SIZE   (sizeof(RTMPPacket)+RTMP_MAX_HEADER_SIZE)
bool ScreenPicTransporter::InitTransporter(char* transportUrl, BlockQueue<VideoPacket*>* videoPackets)
{
	//init windows net libraries:
	WSADATA wsaData;
	GOTO_CLEAR_IF_FAILED(WSAStartup(MAKEWORD(2, 0), &wsaData), "transporter wsa startup failed!");
	//init rtmp and connect to rtmp server:
	this->rtmp_session = RTMP_Alloc();
	RTMP_Init(this->rtmp_session);
	this->rtmp_session->Link.timeout = 10;
	this->rtmp_session->Link.lFlags |= RTMP_LF_LIVE;
	TRANSPORTER_RET_ERR_HANDLE_CLEAR(RTMP_SetupURL(this->rtmp_session, transportUrl),"transporter setup rtmp url failed!");
	RTMP_EnableWrite(this->rtmp_session);
	TRANSPORTER_RET_ERR_HANDLE_CLEAR(RTMP_Connect(this->rtmp_session, NULL), "transporter connect rtmp server failed!");
	TRANSPORTER_RET_ERR_HANDLE_CLEAR(RTMP_ConnectStream(this->rtmp_session, 0), "transporter connect stream failed!");
	this->videoPackets = videoPackets;
	return true;
clear:
	WSACleanup();
	if (this->rtmp_session)
	{
		RTMP_Close(this->rtmp_session);
		RTMP_Free(this->rtmp_session);
	}
	return false;
}

bool ScreenPicTransporter::StartTransport()
{
	TransportThread = new std::thread(&ScreenPicTransporter::TransportThreadFunc, this);
	if (TransportThread) return false;
	return true;
}

void ScreenPicTransporter::TransportThreadFunc()
{
	while (1)
	{
		//send data....
		SendVideoPacket();
	}
}

void WriteTest(unsigned char* data, int size)
{
	static bool finish = false;
	static int timer = 0;
	static FILE* file = fopen("out.h264", "wb");
	if (finish)return;
	fwrite(data, 1, size, file);
	fflush(file);
	if (++timer > 2000)
	{
		fclose(file);
		finish = true;
		printf("finish!");
	}
}

void ScreenPicTransporter::SendVideoPacket()
{
	VideoPacket* packet = videoPackets->get();
	//WriteTest(packet->getData(), packet->getDataSize());
	int videosize = packet->getDataSize();
	int timeStamp = packet->getTimeStamp();
	unsigned char* videoData = packet->getData();
	unsigned char* sendData = (unsigned char*)malloc(videosize + 9); memset(sendData, 0, sizeof(videosize + 9));
	if (packet->PictureType==PIC_TYPE_IDR_SLICE)
	{
		//IDR slice,we need to extract sps and pps data,and send it first:
		int main_index = 0;
		if (!this->sps_data || !this->pps_data)
		{
			packet->GetAndJumpOverSPSPPS(videoData, videosize, &this->sps_data, &this->len_sps, &this->pps_data, &this->len_pps, &main_index);		//send sps and pps data:
			SendMetaData();
		}
		else
			main_index = packet->JumpOverNextStartCode(videoData, 0, videosize - 1);
		if (main_index == -1||!this->sps_data||!this->pps_data|| !this->len_sps||!this->len_pps)
		{
			printf("video packet(IDR) error!\n");
			return;
		}
		videoData += main_index;
		videosize -= main_index;
		sendData[0] = 0x17;
		SendVideoSPSPPS(this->sps_data, this->len_sps, this->pps_data, this->len_pps, packet->TimeStamp);
	}
	else
	{
		//other slice,just send it:
		int main_index = packet->JumpOverNextStartCode(videoData, 0, videosize - 1);
		videoData += main_index;
		videosize -= main_index;
		if (main_index == -1)
		{
			printf("video packet(not IDR) error!\n");
			return;
		}
		sendData[0] = 0x27;
		//NALU data:
		memcpy(sendData + 9, videoData, videosize);
	}
	sendData[1] = 0x01;
	sendData[2] = 0x00;
	sendData[3] = 0x00;
	sendData[4] = 0x00;

	//NALU size:
	sendData[5] = videosize >> 24 & 0xff;
	sendData[6] = videosize >> 16 & 0xff;
	sendData[7] = videosize >> 8  & 0xff;
	sendData[8] = videosize & 0xff;

	memcpy(sendData + 9, videoData, videosize);
	SendPacket(RTMP_PACKET_TYPE_VIDEO, sendData, videosize + 9, timeStamp);
	free(sendData);
	delete packet;
}


void ScreenPicTransporter::SendPacket(int PacketType, unsigned char* data, int size, int nTimestamp)
{
	RTMPPacket* packet;
	packet = (RTMPPacket*)malloc(RTMP_HEAD_SIZE + size);
	memset(packet, 0, RTMP_HEAD_SIZE);
	packet->m_body = (char*)packet + RTMP_HEAD_SIZE;
	packet->m_nBodySize = size;
	memcpy(packet->m_body, data, size);
	packet->m_hasAbsTimestamp = 1;
	packet->m_packetType = PacketType; /*此处为类型有两种一种是音频,一种是视频*/
	packet->m_nInfoField2 = this->rtmp_session->m_stream_id;
	packet->m_nChannel = 0x04;
	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
	if (RTMP_PACKET_TYPE_AUDIO == PacketType && size != 4)
	{
		packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	}
	packet->m_nTimeStamp = nTimestamp;
	/*发送*/
	int nRet = 0;
	if (RTMP_IsConnected(this->rtmp_session))
	{
		nRet = RTMP_SendPacket(this->rtmp_session, packet, FALSE); /*TRUE为放进发送队列,FALSE是不放进发送队列,直接发送*/
	}
	/*释放内存*/
	free(packet);
	return;
}

void ScreenPicTransporter::SendVideoSPSPPS(unsigned char* sps, int sps_len, unsigned char* pps, int pps_len,int TimeStamp)
{
	RTMPPacket* packet = NULL;//rtmp包结构
	unsigned char* body = NULL;
	int i;
	packet = (RTMPPacket*)malloc(RTMP_HEAD_SIZE + 1024);
	//RTMPPacket_Reset(packet);//重置packet状态
	memset(packet, 0, RTMP_HEAD_SIZE + 1024);
	packet->m_body = (char*)packet + RTMP_HEAD_SIZE;
	body = (unsigned char*)packet->m_body;
	i = 0;
	body[i++] = 0x17;
	body[i++] = 0x00;

	body[i++] = 0x00;
	body[i++] = 0x00;
	body[i++] = 0x00;

	/*AVCDecoderConfigurationRecord*/
	body[i++] = 0x01;
	body[i++] = sps[1];
	body[i++] = sps[2];
	body[i++] = sps[3];
	body[i++] = 0xff;

	/*sps*/
	body[i++] = 0xe1;
	body[i++] = (sps_len >> 8) & 0xff;
	body[i++] = sps_len & 0xff;
	memcpy(&body[i], sps, sps_len);
	i += sps_len;

	/*pps*/
	body[i++] = 0x01;
	body[i++] = (pps_len >> 8) & 0xff;
	body[i++] = (pps_len) & 0xff;
	memcpy(&body[i], pps, pps_len);
	i += pps_len;

	packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
	packet->m_nBodySize = i;
	packet->m_nChannel = 0x04;
	packet->m_nTimeStamp = TimeStamp;
	packet->m_hasAbsTimestamp = 1;
	packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet->m_nInfoField2 = this->rtmp_session->m_stream_id;

	/*调用发送接口*/
	int nRet = RTMP_SendPacket(this->rtmp_session, packet, FALSE);
	free(packet);    //释放内存
	return;
}

void ScreenPicTransporter::SendMetaData()
{
	RTMPPacket* packet = NULL;//rtmp包结构
	unsigned char* body = NULL;
	int i;
	packet = (RTMPPacket*)malloc(RTMP_HEAD_SIZE + 1024);
	memset(packet, 0, RTMP_HEAD_SIZE + 1024);
	packet->m_body = (char*)packet + RTMP_HEAD_SIZE;
	body = (unsigned char*)packet->m_body;
	packet->m_nChannel = 0x03;
	packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet->m_packetType = RTMP_PACKET_TYPE_INFO;
	packet->m_nTimeStamp = 0;
	packet->m_nInfoField2 = this->rtmp_session->m_stream_id;
	packet->m_hasAbsTimestamp = 0;
	char* enc = packet->m_body; char* enc_end = packet->m_body+1024;
	AVal setDataFrame;
	AVal onMetaData;
	AMFObject object;
	setDataFrame.av_val = (char*)malloc(20); memset(setDataFrame.av_val, 0, 20);
	setDataFrame.av_len = strlen("@setDataFrame");
	onMetaData.av_val = (char*)malloc(20); memset(onMetaData.av_val, 0, 20);
	onMetaData.av_len = strlen("onMetaData");
	strcpy(setDataFrame.av_val, "@setDataFrame");
	strcpy(onMetaData.av_val, "onMetaData");
	enc = AMF_EncodeString(enc, enc_end, &setDataFrame);
	enc = AMF_EncodeString(enc, enc_end, &onMetaData);
	object.o_num = 0;
	object.o_props = (AMFObjectProperty*)malloc(2*sizeof(AMFObjectProperty));
	AMFObjectProperty objProp_duration = makeObjectIntProp("duration", 0);
	AMF_AddProp(&object, &objProp_duration);
	AMFObjectProperty objProp_fileSize = makeObjectIntProp("fileSize", 0);
	AMF_AddProp(&object, &objProp_fileSize);
	AMFObjectProperty objProp_Width = makeObjectIntProp("width", 1920);
	AMF_AddProp(&object, &objProp_Width);
	AMFObjectProperty objProp_Height = makeObjectIntProp("height", 1080);
	AMF_AddProp(&object, &objProp_Height);
	AMFObjectProperty objProp_videocodecid = makeObjectStringProp("videocodecid", "avc1");
	AMF_AddProp(&object, &objProp_videocodecid);
	AMFObjectProperty objProp_videodatarate = makeObjectIntProp("videodatarate", 100000);
	AMF_AddProp(&object, &objProp_videodatarate);
	AMFObjectProperty objProp_framerate = makeObjectIntProp("framerate", 60);
	AMF_AddProp(&object, &objProp_framerate);
	AMFObjectProperty objProp_2_1 = makeObjectBooleanProp("2.1", FALSE);
	AMF_AddProp(&object, &objProp_2_1);
	AMFObjectProperty objProp_3_1 = makeObjectBooleanProp("3.1", FALSE);
	AMF_AddProp(&object, &objProp_3_1);
	AMFObjectProperty objProp_4_0 = makeObjectBooleanProp("4.0", FALSE);
	AMF_AddProp(&object, &objProp_4_0);
	AMFObjectProperty objProp_4_1 = makeObjectBooleanProp("4.1", FALSE);
	AMF_AddProp(&object, &objProp_4_1);
	AMFObjectProperty objProp_5_1 = makeObjectBooleanProp("5.1", FALSE);
	AMF_AddProp(&object, &objProp_5_1);
	AMFObjectProperty objProp_7_1 = makeObjectBooleanProp("7.1", FALSE);
	AMF_AddProp(&object, &objProp_7_1);
	AMFObjectProperty objProp_encoder = makeObjectStringProp("encoder", "obs-output module (libobs version 24.0.0)");
	AMF_AddProp(&object, &objProp_encoder);
	AMFObjectProperty objProp_DouyuTVBroadcasterVersion = makeObjectStringProp("DouyuTVBroadcasterVersion", "5.1.6.0");
	AMF_AddProp(&object, &objProp_DouyuTVBroadcasterVersion);
	enc = AMF_Encode(&object, enc, enc_end);
	RTMP_SendPacket(this->rtmp_session, packet, FALSE);
	free(packet);
}

AMFObjectProperty ScreenPicTransporter::makeObjectStringProp(const char* name, const char*  val)
{
	AMFObjectProperty prop;
	AVal p_name; p_name.av_val = (char*)malloc(25); memset(p_name.av_val, 0, 25); p_name.av_len = strlen(name); strcpy(p_name.av_val, name);
	AVal p_val; p_val.av_val = (char*)malloc(25); memset(p_val.av_val, 0, 25); p_val.av_len = strlen(val); strcpy(p_val.av_val, val);
	prop.p_name = p_name;
	prop.p_type = AMF_STRING;
	prop.p_vu.p_aval = p_val;
	prop.p_UTCoffset = 0;
	return prop;
}

AMFObjectProperty ScreenPicTransporter::makeObjectIntProp(const char* name, int val)
{
	AMFObjectProperty prop;
	AVal p_name; p_name.av_val = (char*)malloc(25); memset(p_name.av_val, 0, 25); p_name.av_len = strlen(name); strcpy(p_name.av_val, name);
	prop.p_name = p_name;
	prop.p_type = AMF_NUMBER;
	prop.p_vu.p_number = val;
	prop.p_UTCoffset = 0;
	return prop;
}

AMFObjectProperty ScreenPicTransporter::makeObjectBooleanProp(const char* name, bool val)
{
	AMFObjectProperty prop;
	AVal p_name; p_name.av_val = (char*)malloc(25); memset(p_name.av_val, 0, 25); p_name.av_len = strlen(name); strcpy(p_name.av_val, name);
	prop.p_name = p_name;
	prop.p_type = AMF_BOOLEAN;
	prop.p_vu.p_number = val;
	prop.p_UTCoffset = 0;
	return prop;
}

