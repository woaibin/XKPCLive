//#include "ScreenLiveManager.h"
#include "ScreenPicProducer.h"
#include "ScreenPicTransporter.h"
BlockQueue<VideoPacket*>* VideoPackets;
ScreenPicProducer producer;
ScreenPicTransporter transporter;
int main()
{
	//Sleep(6000);
	//"rtmp://76466.livepush.myqcloud.com/live/76466_10483347?bizid=76466&txSecret=838cebefe8ebd178932fdd125f4aa16c&txTime=5E67544D"
	//rtmp://live.hexurong.xyz/live76466_10483347
	//rtmp://111.230.9.18/myapp/stream
	Sleep(4000);
	char pushUrl[255] = { 0 }; strcpy(pushUrl, "rtmp://127.0.0.1:1935/live/04D9F57DB0D5");
	VideoPackets = new BlockQueue<VideoPacket*>();
	if (!producer.InitProducer(VideoPackets))
	{
		printf("init producer failed!,return!\n");
		return -1;
	}
	if (!transporter.InitTransporter(pushUrl, VideoPackets))
	{
		printf("init transporter failed!,return !\n");
		return -1;
	}
	producer.StartProducingPics();
	transporter.StartTransport();
	while (1)
	{
		Sleep(1000);
	}
	return 0;
}