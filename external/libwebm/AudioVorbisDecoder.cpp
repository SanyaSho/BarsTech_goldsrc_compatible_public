/*
	MIT License

	Copyright (c) 2016 Błażej Szczygieł

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include "AudioVorbisDecoder.hpp"

#include "../ffmpeg/build_win32/include/vorbis/codec.h"
//#include <vorbis/codec.h>

#include <string.h>

struct VorbisDecoder
{
	vorbis_info info;
	vorbis_dsp_state dspState;
	vorbis_block block;
	ogg_packet op;

	bool hasDSPState, hasBlock;
};

/**/

AudioVorbisDecoder::AudioVorbisDecoder(const WebMDemuxer &demuxer) :
	m_pVorbis(NULL),
	m_nSamples(0)
{
	switch (demuxer.getAudioCodec())
	{
		case WebMDemuxer::AUDIO_VORBIS:
			m_nChannels = demuxer.getChannels();
			if (openVorbis(demuxer))
				return;
			break;
		case WebMDemuxer::AUDIO_OPUS:
			m_nChannels = demuxer.getChannels();
			break;
		default:
			return;
	}
	close();
}
AudioVorbisDecoder::~AudioVorbisDecoder()
{
	close();
}

bool AudioVorbisDecoder::isOpen() const
{
	return m_pVorbis;
}

bool AudioVorbisDecoder::getPCMS16(WebMFrame &frame, short *buffer, int &numOutSamples)
{
	if (m_pVorbis)
	{
		m_pVorbis->op.packet = frame.m_pBuffer;
		m_pVorbis->op.bytes = frame.m_uBufferSize;

		if (vorbis_synthesis(&m_pVorbis->block, &m_pVorbis->op))
			return false;
		if (vorbis_synthesis_blockin(&m_pVorbis->dspState, &m_pVorbis->block))
			return false;

		const int maxSamples = getBufferSamples();
		int samplesCount, count = 0;
		float **pcm;
		while ((samplesCount = vorbis_synthesis_pcmout(&m_pVorbis->dspState, &pcm)))
		{
			const int toConvert = samplesCount <= maxSamples ? samplesCount : maxSamples;
			for (int c = 0; c < m_nChannels; ++c)
			{
				float *samples = pcm[c];
				for (int i = 0, j = c; i < toConvert; ++i, j += m_nChannels)
				{
					int sample = samples[i] * 32767.0f;
					if (sample > 32767)
						sample = 32767;
					else if (sample < -32768)
						sample = -32768;
					buffer[count + j] = sample;
				}
			}
			vorbis_synthesis_read(&m_pVorbis->dspState, toConvert);
			count += toConvert;
		}

		numOutSamples = count;
		return true;
	}
	return false;
}

bool AudioVorbisDecoder::openVorbis(const WebMDemuxer &demuxer)
{
	size_t extradataSize = 0;
	const unsigned char *extradata = demuxer.getAudioExtradata(extradataSize);

	if (extradataSize < 3 || !extradata || extradata[0] != 2)
		return false;

	size_t headerSize[3] = {0};
	size_t offset = 1;

	/* Calculate three headers sizes */
	for (int i = 0; i < 2; ++i)
	{
		for (;;)
		{
			if (offset >= extradataSize)
				return false;
			headerSize[i] += extradata[offset];
			if (extradata[offset++] < 0xFF)
				break;
		}
	}
	headerSize[2] = extradataSize - (headerSize[0] + headerSize[1] + offset);

	if (headerSize[0] + headerSize[1] + headerSize[2] + offset != extradataSize)
		return false;

	ogg_packet op[3];
	memset(op, 0, sizeof op);

	op[0].packet = (unsigned char *)extradata + offset;
	op[0].bytes = headerSize[0];
	op[0].b_o_s = 1;

	op[1].packet = (unsigned char *)extradata + offset + headerSize[0];
	op[1].bytes = headerSize[1];

	op[2].packet = (unsigned char *)extradata + offset + headerSize[0] + headerSize[1];
	op[2].bytes = headerSize[2];

	m_pVorbis = new VorbisDecoder;
	m_pVorbis->hasDSPState = m_pVorbis->hasBlock = false;
	vorbis_info_init(&m_pVorbis->info);

	/* Upload three Vorbis headers into libvorbis */
	vorbis_comment vc;
	vorbis_comment_init(&vc);
	for (int i = 0; i < 3; ++i)
	{
		if (vorbis_synthesis_headerin(&m_pVorbis->info, &vc, &op[i]))
		{
			vorbis_comment_clear(&vc);
			return false;
		}
	}
	vorbis_comment_clear(&vc);

	if (vorbis_synthesis_init(&m_pVorbis->dspState, &m_pVorbis->info))
		return false;
	m_pVorbis->hasDSPState = true;

	if (m_pVorbis->info.channels != m_nChannels || m_pVorbis->info.rate != demuxer.getSampleRate())
		return false;

	if (vorbis_block_init(&m_pVorbis->dspState, &m_pVorbis->block))
		return false;
	m_pVorbis->hasBlock = true;

	memset(&m_pVorbis->op, 0, sizeof m_pVorbis->op);

	m_nSamples = 4096 / m_nChannels;

	return true;
}

void AudioVorbisDecoder::close()
{
	if (m_pVorbis)
	{
		if (m_pVorbis->hasBlock)
			vorbis_block_clear(&m_pVorbis->block);
		if (m_pVorbis->hasDSPState)
			vorbis_dsp_clear(&m_pVorbis->dspState);
		vorbis_info_clear(&m_pVorbis->info);
		delete m_pVorbis;
	}
}
