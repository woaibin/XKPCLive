#include "SoundEncoder.h"

bool SoundEncoder::InitEncoder(PENCODER_OPT opt, BlockQueue<SoundPacket*>* encoded_packets)
{
	//use fdk-aac encoder:
	FFMPEG_OBJ_ERR_HANDLE_RETURN_FALSE((this->codec = avcodec_find_encoder_by_name("libfdk_aac")), "failed to get fdk-aac encoder");
	FFMPEG_OBJ_ERR_HANDLE_RETURN_FALSE((this->codecContext = avcodec_alloc_context3(this->codec)), "failed to alloc fdk-aac codec context");
	//config encoder params:
	this->codecContext->bit_rate = 256000;
	this->codecContext->sample_fmt = AV_SAMPLE_FMT_FLT;
	this->codecContext->sample_rate = 44100;
	this->codecContext->channel_layout = AV_CH_LAYOUT_STEREO;
	this->codecContext->channels = 2;
	FFMPEG_OBJ_ERR_HANDLE_RETURN_FALSE(avcodec_open2(this->codecContext, this->codec, NULL), "failed to open fdk-aac codec");
	return true;
}

void SoundEncoder::EncodeSound(unsigned char* sound_data, int size)
{
	AVFrame* frame = av_frame_alloc();
	frame->nb_samples = this->codecContext->frame_size;
	frame->format = this->codecContext->sample_fmt;
	frame->channel_layout = this->codecContext->channel_layout;
	int requeiresize = av_samples_fill_arrays(frame->data, frame->linesize, sound_data, 2, this->codecContext->frame_size, this->codecContext->sample_fmt, 0);
	if (requeiresize != size)
	{
		printf("calc audio frame size failed!\n");
		exit(1);
	}
	AVPacket packet;
	av_init_packet(&packet);
	packet.data = NULL;
	packet.size = 0;

	static FILE* file = fopen("outputmusic.aac", "wb");
	FFMPEG_RET_ERR_HANDLE_CLEAR(avcodec_send_frame(this->codecContext, frame), "sorry,sending frame to encode failed!");
	while (avcodec_receive_packet(this->codecContext, &packet) != AVERROR(EAGAIN))
	{
		fwrite(packet.data, 1, packet.size, file);
	}
clear:
	av_frame_free(&frame);
}
