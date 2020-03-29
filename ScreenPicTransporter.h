#pragma once
#include <winsock2.h>
#include <Windows.h>
#include "ScreenPicCommon.h"
#include <thread>
#include <queue>
#include "ScreenPicEncoder.h"
#pragma comment(lib,"WS2_32.lib")   
#pragma comment(lib,"winmm.lib")  
extern "C"
{
#include <rtmp_sys.h>
#include <rtmp.h>
#include <amf.h>
#pragma comment(lib,"libeay64.lib")
#pragma comment(lib,"ssleay64.lib")
#pragma comment(lib,"zlibstat64.lib")
#pragma comment(lib,"librtmp64.lib")
}

#define TRANSPORTER_RET_ERR_HANDLE_RETURN_FALSE(ret,info) RETURN_FALSE_IF_FAILED((!ret),info)
#define TRANSPORTER_RET_ERR_HANDLE_CLEAR(ret,info) GOTO_CLEAR_IF_FAILED((!ret),info)
typedef ScreenPicEncoder::ScreenPicPacket VideoPacket;
class ScreenPicTransporter
{
private:
	RTMP* rtmp_session = NULL;
	std::thread* TransportThread = NULL;
private:
	BlockQueue<VideoPacket*>* videoPackets = NULL;
	unsigned char* sps_data = NULL;
	unsigned char* pps_data = NULL;
	int len_sps = 0, len_pps = 0;
public:
	bool InitTransporter(char* transportUrl, BlockQueue<VideoPacket*>* videoPackets);
	bool StartTransport();
public:
	void TransportThreadFunc();
	void SendVideoPacket();
	void SendPacket(int PacketType,unsigned char* data,int size,int nTimestamp);
	void SendVideoSPSPPS(unsigned char* sps,int len_sps,unsigned char* pps,int len_pps,int TimeStamp);
	void SendMetaData();
	AMFObjectProperty makeObjectStringProp(const char* name, const char* val);
	AMFObjectProperty makeObjectIntProp(const char* name, int val);
	AMFObjectProperty makeObjectBooleanProp(const char* name, bool val);
};

