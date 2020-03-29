#include "ScreenPicProducer.h"
bool ScreenPicProducer::InitProducer(BlockQueue<ScreenPicEncoder::ScreenPicPacket*>* videoPackets)
{
	HRESULT hr;
	IDXGIDevice* p_dxgiDevice = NULL;
	IDXGIAdapter* p_dxgiAdapter = NULL;
	IDXGIOutput* p_dxgiOutput = NULL;
	IDXGIOutput1* p_dxgiOutput1 = NULL;
	ScreenPicEncoder::FRAMERATE framerate = { 60,1 };
	ScreenPicEncoder::PENCODER_OPT opt = new ScreenPicEncoder::ENCODER_OPT();
	//ID3D11Multithread *multithread;
	//get d3d and dxgi resources:
	PRODUCER_RET_ERR_HANDLE_CLEAR((hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &p_d3dDevice, NULL, &p_d3dDeviceContext)),"create d3d11 device failed!");
	//PRODUCER_RET_ERR_HANDLE_CLEAR((hr = this->p_d3dDevice->QueryInterface(IID_PPV_ARGS(&multithread))), "get d3d11 multithread failed!"); multithread->SetMultithreadProtected(true);
	PRODUCER_RET_ERR_HANDLE_CLEAR((hr =p_d3dDevice->QueryInterface(IID_PPV_ARGS(&p_dxgiDevice))), "get dxgi device failed!");
	PRODUCER_RET_ERR_HANDLE_CLEAR((hr =p_dxgiDevice->GetParent(IID_PPV_ARGS(&p_dxgiAdapter))), "get dxgi adapter failed!");
	PRODUCER_RET_ERR_HANDLE_CLEAR((hr =p_dxgiAdapter->EnumOutputs(0, &p_dxgiOutput)),"enum output failed");
	PRODUCER_RET_ERR_HANDLE_CLEAR((hr =p_dxgiOutput->QueryInterface(IID_PPV_ARGS(&p_dxgiOutput1))), "get output1 failed");
	PRODUCER_RET_ERR_HANDLE_CLEAR((hr =p_dxgiOutput1->DuplicateOutput(p_d3dDevice, &this->p_dxgiOutputDup)), "duplicate output failed");
	this->p_dxgiOutputDup->GetDesc(&this->m_dxgiOutDupDesc);
	//set encoder params and init encoder
	opt->bitrate = 10000000;
	opt->frame_rate = framerate; this->interval = ((float)framerate.den / framerate.num)*1000;
	opt->gop_size = 30;
	opt->width = this->m_dxgiOutDupDesc.ModeDesc.Width; this->width = opt->width;
	opt->height = this->m_dxgiOutDupDesc.ModeDesc.Height; this->height = opt->height;
	opt->max_b_frame = 1;
	opt->pix_fmt = DXGI_FORMAT_B8G8R8A8_UNORM;
	opt->p_DirectXDevice = this->p_d3dDevice;
	PRODUCER_RET_ERR_HANDLE_CLEAR(PicEncoder.InitCodec(opt,videoPackets), "init codec failed!");
clear:
	if (p_dxgiDevice)p_dxgiDevice->Release();
	if (p_dxgiAdapter)p_dxgiAdapter->Release();
	if (p_dxgiOutput)p_dxgiOutput->Release();
	if (p_dxgiOutput1)p_dxgiOutput1->Release();
	if (opt) delete opt;
	//sleep a while before starting producing screen pics,so that the first pic produced will not be black:
	Sleep(100);
	return SUCCEEDED(hr);
}

bool ScreenPicProducer::StartProducingPics()
{
	this->PicsProducingThread = new std::thread(&ScreenPicProducer::ThreadFunc_ProducingPics, this);
	return true;
}

void ScreenPicProducer::ThreadFunc_ProducingPics()
{
	DWORD start;
	int cost;
	ID3D11Texture2D* p_texture2d_last = NULL;
	D3D11_TEXTURE2D_DESC desc;
	bool isFirst = true;
	while (1)
	{
		start = GetTickCount();
		DXGI_OUTDUPL_FRAME_INFO frame_info;
		IDXGIResource* p_DesktopResource = NULL;
		ID3D11Texture2D* p_texture2d = NULL;
		UINT index_subres = 0;
		//get next frame:
		this->p_dxgiOutputDup->ReleaseFrame();
		if (this->p_dxgiOutputDup->AcquireNextFrame(0, &frame_info, &p_DesktopResource) == DXGI_ERROR_WAIT_TIMEOUT)
		{
			//GOTO_CLEAR_IF_FAILED(!this->PicEncoder.DumpLastEncode(), "dump last encode failed!");
			if(!p_texture2d_last) continue;
			PicEncoder.EncodeD3D11Texture(p_texture2d_last);
			goto clear;
		}
		else
		{
			p_DesktopResource->QueryInterface(IID_PPV_ARGS(&p_texture2d));
			//set as last:
			if (isFirst) 
			{
				p_texture2d->GetDesc(&desc);
				desc.BindFlags = D3D11_BIND_RENDER_TARGET;
				desc.MiscFlags = 0;
				this->p_d3dDevice->CreateTexture2D(&desc, NULL, &p_texture2d_last);
				isFirst = false; 
			}
			//if (p_texture2d_last) p_texture2d_last->Release();			
			
			this->p_d3dDeviceContext->CopyResource(p_texture2d_last, p_texture2d);
			PicEncoder.EncodeD3D11Texture(p_texture2d);
		}
	clear:
		if (p_texture2d) p_texture2d->Release();
		if(p_DesktopResource) p_DesktopResource->Release();
		cost = 16 - (GetTickCount() - start);
		Sleep(cost < 0 ? 0 : cost);
	}
}
