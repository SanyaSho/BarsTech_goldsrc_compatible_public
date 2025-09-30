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

#ifndef WEBMDEMUXER_HPP
#define WEBMDEMUXER_HPP

#include <stddef.h>

namespace mkvparser {
	class IMkvReader;
	class Segment;
	class Cluster;
	class Block;
	class BlockEntry;
	class VideoTrack;
	class AudioTrack;
	class MkvReader;
}

class WebMFrame
{
	WebMFrame(const WebMFrame &);
	void operator =(const WebMFrame &);
public:
	WebMFrame();
	~WebMFrame();

	inline bool isValid() const
	{
		return m_uBufferSize > 0;
	}

	long m_uBufferSize, m_uBufferCapacity;
	unsigned char *m_pBuffer;
	double m_flTime;
	bool m_bKey;
};

class WebMDemuxer
{
	WebMDemuxer(const WebMDemuxer &);
	void operator =(const WebMDemuxer &);
public:
	enum VIDEO_CODEC
	{
		NO_VIDEO,
		VIDEO_VP8,
		VIDEO_VP9
	};
	enum AUDIO_CODEC
	{
		NO_AUDIO,
		AUDIO_VORBIS,
		AUDIO_OPUS
	};

	WebMDemuxer(mkvparser::IMkvReader *reader, int videoTrack = 0, int audioTrack = 0);
	~WebMDemuxer();
	WebMDemuxer() = default;

	inline bool isOpen() const
	{
		return m_bIsOpen;
	}
	inline bool isEOS() const
	{
		return m_bEOS;
	}

	double getLength() const;

	VIDEO_CODEC getVideoCodec() const;
	int getWidth() const;
	int getHeight() const;

	AUDIO_CODEC getAudioCodec() const;
	const unsigned char *getAudioExtradata(size_t &size) const; // Needed for Vorbis
	double getSampleRate() const;
	int getChannels() const;
	int getAudioDepth() const;

	bool ReadFrame(WebMFrame *videoFrame, WebMFrame *audioFrame);

public:
	inline bool notSupportedTrackNumber(long videoTrackNumber, long audioTrackNumber) const;

	mkvparser::IMkvReader *m_pReader;
	mkvparser::Segment *m_pSegment;

	const mkvparser::Cluster *m_pCluster;
	const mkvparser::Block *m_pBlock;
	const mkvparser::BlockEntry *m_pBlockEntry;

	int m_nBlockFrameIndex;

	const mkvparser::VideoTrack *m_pVideoTrack;
	VIDEO_CODEC m_eVideoCodec;

	const mkvparser::AudioTrack *m_pAudioTrack;
	AUDIO_CODEC m_eAudioCodec;

	bool m_bIsOpen;
	bool m_bEOS;
};

#endif // WEBMDEMUXER_HPP
