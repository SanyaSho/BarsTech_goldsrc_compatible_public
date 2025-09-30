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

#include "WebMDemuxer.hpp"

#include "mkvparser/mkvparser.hpp"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

WebMFrame::WebMFrame() :
	m_uBufferSize(0), m_uBufferCapacity(0),
	m_pBuffer(NULL),
	m_flTime(0),
	m_bKey(false)
{}
WebMFrame::~WebMFrame()
{
	free(m_pBuffer);
}

/**/

WebMDemuxer::WebMDemuxer(mkvparser::IMkvReader *reader, int videoTrack, int audioTrack) :
	m_pReader(reader),
	m_pSegment(NULL),
	m_pCluster(NULL), m_pBlock(NULL), m_pBlockEntry(NULL),
	m_nBlockFrameIndex(0),
	m_pVideoTrack(NULL), m_eVideoCodec(NO_VIDEO),
	m_pAudioTrack(NULL), m_eAudioCodec(NO_AUDIO),
	m_bIsOpen(false),
	m_bEOS(false)
{
	long long pos = 0;
	if (mkvparser::EBMLHeader().Parse(m_pReader, pos))
		return;

	if (mkvparser::Segment::CreateInstance(m_pReader, pos, m_pSegment))
		return;

	if (m_pSegment->Load() < 0)
		return;

	const mkvparser::Tracks *tracks = m_pSegment->GetTracks();
	const unsigned long tracksCount = tracks->GetTracksCount();
	int currVideoTrack = -1, currAudioTrack = -1;
	for (unsigned long i = 0; i < tracksCount; ++i)
	{
		const mkvparser::Track *track = tracks->GetTrackByIndex(i);
		if (const char *codecId = track->GetCodecId())
		{
			if ((!m_pVideoTrack || currVideoTrack != videoTrack) && track->GetType() == mkvparser::Track::kVideo)
			{
				if (!strcmp(codecId, "V_VP8"))
					m_eVideoCodec = VIDEO_VP8;
				else if (!strcmp(codecId, "V_VP9"))
					m_eVideoCodec = VIDEO_VP9;
				if (m_eVideoCodec != NO_VIDEO)
					m_pVideoTrack = static_cast<const mkvparser::VideoTrack *>(track);
				++currVideoTrack;
			}
			if ((!m_pAudioTrack || currAudioTrack != audioTrack) && track->GetType() == mkvparser::Track::kAudio)
			{
				if (!strcmp(codecId, "A_VORBIS"))
					m_eAudioCodec = AUDIO_VORBIS;
				else if (!strcmp(codecId, "A_OPUS"))
					m_eAudioCodec = AUDIO_OPUS;
				if (m_eAudioCodec != NO_AUDIO)
					m_pAudioTrack = static_cast<const mkvparser::AudioTrack *>(track);
				++currAudioTrack;
			}
		}
	}
	if (!m_pVideoTrack && !m_pAudioTrack)
		return;

	m_bIsOpen = true;
}
WebMDemuxer::~WebMDemuxer()
{
	if (m_pSegment != NULL)
		delete m_pSegment;
//	delete m_pReader;
}

double WebMDemuxer::getLength() const
{
	return m_pSegment->GetDuration() / 1e9;
}

WebMDemuxer::VIDEO_CODEC WebMDemuxer::getVideoCodec() const
{
	return m_eVideoCodec;
}
int WebMDemuxer::getWidth() const
{
	return m_pVideoTrack->GetWidth();
}
int WebMDemuxer::getHeight() const
{
	return m_pVideoTrack->GetHeight();
}

WebMDemuxer::AUDIO_CODEC WebMDemuxer::getAudioCodec() const
{
	return m_eAudioCodec;
}
const unsigned char *WebMDemuxer::getAudioExtradata(size_t &size) const
{
	return m_pAudioTrack->GetCodecPrivate(size);
}
double WebMDemuxer::getSampleRate() const
{
	return m_pAudioTrack->GetSamplingRate();
}
int WebMDemuxer::getChannels() const
{
	return m_pAudioTrack->GetChannels();
}
int WebMDemuxer::getAudioDepth() const
{
	return m_pAudioTrack->GetBitDepth();
}

bool WebMDemuxer::ReadFrame(WebMFrame *videoFrame, WebMFrame *audioFrame)
{
	const long videoTrackNumber = (videoFrame && m_pVideoTrack) ? m_pVideoTrack->GetNumber() : 0;
	const long audioTrackNumber = (audioFrame && m_pAudioTrack) ? m_pAudioTrack->GetNumber() : 0;
	bool blockEntryEOS = false;

	if (videoFrame)
		videoFrame->m_uBufferSize = 0;
	if (audioFrame)
		audioFrame->m_uBufferSize = 0;

	if (videoTrackNumber == 0 && audioTrackNumber == 0)
		return false;

	if (m_bEOS)
		return false;

	if (!m_pCluster)
		m_pCluster = m_pSegment->GetFirst();

	do
	{
		bool getNewBlock = false;
		long status = 0;
		if (!m_pBlockEntry && !blockEntryEOS)
		{
			status = m_pCluster->GetFirst(m_pBlockEntry);
			getNewBlock = true;
		}
		else if (blockEntryEOS || m_pBlockEntry->EOS())
		{
			m_pCluster = m_pSegment->GetNext(m_pCluster);
			if (!m_pCluster || m_pCluster->EOS())
			{
				m_bEOS = true;
				return false;
			}
			status = m_pCluster->GetFirst(m_pBlockEntry);
			blockEntryEOS = false;
			getNewBlock = true;
		}
		else if (!m_pBlock || m_nBlockFrameIndex == m_pBlock->GetFrameCount() || notSupportedTrackNumber(videoTrackNumber, audioTrackNumber))
		{
			status = m_pCluster->GetNext(m_pBlockEntry, m_pBlockEntry);
			if (!m_pBlockEntry  || m_pBlockEntry->EOS())
			{
				blockEntryEOS = true;
				continue;
			}
			getNewBlock = true;
		}
		if (status || !m_pBlockEntry)
			return false;
		if (getNewBlock)
		{
			m_pBlock = m_pBlockEntry->GetBlock();
			m_nBlockFrameIndex = 0;
		}
	} while (blockEntryEOS || notSupportedTrackNumber(videoTrackNumber, audioTrackNumber));

	WebMFrame *frame = NULL;

	const long trackNumber = m_pBlock->GetTrackNumber();
	if (trackNumber == videoTrackNumber)
		frame = videoFrame;
	else if (trackNumber == audioTrackNumber)
		frame = audioFrame;
	else
	{
		//Should not be possible
		assert(trackNumber == videoTrackNumber || trackNumber == audioTrackNumber);
		return false;
	}

	const mkvparser::Block::Frame &blockFrame = m_pBlock->GetFrame(m_nBlockFrameIndex++);
	if (blockFrame.len > frame->m_uBufferCapacity)
	{
		unsigned char *newBuff = (unsigned char *)realloc(frame->m_pBuffer, frame->m_uBufferCapacity = blockFrame.len);
		if (newBuff)
			frame->m_pBuffer = newBuff;
		else // Out of memory
			return false;
	}
	frame->m_uBufferSize = blockFrame.len;

	frame->m_flTime = m_pBlock->GetTime(m_pCluster) / 1e9;
	frame->m_bKey  = m_pBlock->IsKey();

	return !blockFrame.Read(m_pReader, frame->m_pBuffer);
}

inline bool WebMDemuxer::notSupportedTrackNumber(long videoTrackNumber, long audioTrackNumber) const
{
	const long trackNumber = m_pBlock->GetTrackNumber();
	return (trackNumber != videoTrackNumber && trackNumber != audioTrackNumber);
}
