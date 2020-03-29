#pragma once
#include <Windows.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <d3d11_4.h>
#include <thread>
#include "ScreenPicCommon.h"
#include "ScreenPicEncoder.h"
#pragma comment(lib,"D3D11.lib")
#define PRODUCER_RET_ERR_HANDLE_RETURN_FALSE(ret,info) RETURN_FALSE_IF_FAILED(FAILED(ret),info)
#define PRODUCER_RET_ERR_HANDLE_CLEAR(ret,info) GOTO_CLEAR_IF_FAILED(FAILED(ret),info)

class ScreenPicProducer
{
private:
	//d3d and dxgi resources:
	ID3D11DeviceContext*	p_d3dDeviceContext = NULL;
	ID3D11Device*			p_d3dDevice = NULL;
	IDXGIOutputDuplication* p_dxgiOutputDup = NULL;
	DXGI_OUTDUPL_DESC		m_dxgiOutDupDesc;
private:
	std::thread*		PicsProducingThread;
	ScreenPicEncoder	PicEncoder;
	int width;			int height;
	int	interval;//in milliseconds
public:
	bool InitProducer(BlockQueue<ScreenPicEncoder::ScreenPicPacket*>* videoPackets);
	bool StartProducingPics();
private:
	void ThreadFunc_ProducingPics();
};
