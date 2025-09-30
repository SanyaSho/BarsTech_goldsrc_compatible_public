/*
* thanks to:		ffmpeg for ogg, vorbis, vpx
*					WebM project authors for libwebm
*					Blazej Szczygiel for libsimplewebm
*					valve for game of the year'99
*					me for adaption to half-life
* -- barspinOff. 23.11.2023
*/
#include "quakedef.h"
#include "console.h"
#include "sound.h"

#include <SDL2/SDL.h>

#ifndef GLQUAKE
#include <GL/glew.h>
#endif

#include <libwebm/mkvparser/mkvparser.hpp>
#include <libwebm/mkvparser/mkvreader.hpp>

#include <vorbis/codec.h>

#include <vpx/vpx_decoder.h>
#include <vpx/vpx_codec.h>
#include <vpx/vp8dx.h>

#include <yuv2rgb.h>

#include <stddef.h>

#include <assert.h>
#include <string.h>

#include "webm_video.h"


/* 
WEBM DECODER CLASSES
*/

class WebMFrame
{
	WebMFrame(const WebMFrame&);
	void operator =(const WebMFrame&);
public:
	WebMFrame();
	~WebMFrame();

	inline bool IsValid() const
	{
		return m_uBufferSize > 0;
	}

	long m_uBufferSize, m_uBufferCapacity;
	unsigned char* m_pBuffer;
	double m_flTime;
	bool m_bKey;
};

class WebMDemuxer
{
	WebMDemuxer(const WebMDemuxer&);
	void operator =(const WebMDemuxer&);
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

	WebMDemuxer(mkvparser::IMkvReader* reader, int videoTrack = 0, int audioTrack = 0);
	~WebMDemuxer();
	WebMDemuxer() = default;

	inline bool IsOpen() const
	{
		return m_bIsOpen;
	}
	inline bool IsEOS() const
	{
		return m_bEOS;
	}

	double GetLength() const;

	VIDEO_CODEC GetVideoCodec() const;
	int GetWidth() const;
	int GetHeight() const;

	AUDIO_CODEC GetAudioCodec() const;
	const unsigned char* GetAudioExtradata(size_t& size) const; // Needed for Vorbis
	double GetSampleRate() const;
	int GetChannels() const;
	int GetAudioDepth() const;

	bool ReadFrame(WebMFrame* videoFrame, WebMFrame* audioFrame);

public:
	inline bool NotSupportedTrackNumber(long videoTrackNumber, long audioTrackNumber) const;

	mkvparser::IMkvReader* m_pReader;
	mkvparser::Segment* m_pSegment;

	const mkvparser::Cluster* m_pCluster;
	const mkvparser::Block* m_pBlock;
	const mkvparser::BlockEntry* m_pBlockEntry;

	int m_nBlockFrameIndex;

	const mkvparser::VideoTrack* m_pVideoTrack;
	VIDEO_CODEC m_eVideoCodec;

	const mkvparser::AudioTrack* m_pAudioTrack;
	AUDIO_CODEC m_eAudioCodec;

	bool m_bIsOpen;
	bool m_bEOS;
};

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

WebMDemuxer::WebMDemuxer(mkvparser::IMkvReader* reader, int videoTrack, int audioTrack) :
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

	const mkvparser::Tracks* tracks = m_pSegment->GetTracks();
	const unsigned long tracksCount = tracks->GetTracksCount();
	int currVideoTrack = -1, currAudioTrack = -1;
	for (unsigned long i = 0; i < tracksCount; ++i)
	{
		const mkvparser::Track* track = tracks->GetTrackByIndex(i);
		if (const char* codecId = track->GetCodecId())
		{
			if ((!m_pVideoTrack || currVideoTrack != videoTrack) && track->GetType() == mkvparser::Track::kVideo)
			{
				if (!strcmp(codecId, "V_VP8"))
					m_eVideoCodec = VIDEO_VP8;
				else if (!strcmp(codecId, "V_VP9"))
					m_eVideoCodec = VIDEO_VP9;
				if (m_eVideoCodec != NO_VIDEO)
					m_pVideoTrack = static_cast<const mkvparser::VideoTrack*>(track);
				++currVideoTrack;
			}
			if ((!m_pAudioTrack || currAudioTrack != audioTrack) && track->GetType() == mkvparser::Track::kAudio)
			{
				if (!strcmp(codecId, "A_VORBIS"))
					m_eAudioCodec = AUDIO_VORBIS;
				else if (!strcmp(codecId, "A_OPUS"))
					m_eAudioCodec = AUDIO_OPUS;
				if (m_eAudioCodec != NO_AUDIO)
					m_pAudioTrack = static_cast<const mkvparser::AudioTrack*>(track);
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
	// MARK: not needed for stack-stored object
	//	delete m_pReader;
}

double WebMDemuxer::GetLength() const
{
	return m_pSegment->GetDuration() / 1e9;
}

WebMDemuxer::VIDEO_CODEC WebMDemuxer::GetVideoCodec() const
{
	return m_eVideoCodec;
}
int WebMDemuxer::GetWidth() const
{
	return m_pVideoTrack->GetWidth();
}
int WebMDemuxer::GetHeight() const
{
	return m_pVideoTrack->GetHeight();
}

WebMDemuxer::AUDIO_CODEC WebMDemuxer::GetAudioCodec() const
{
	return m_eAudioCodec;
}
const unsigned char* WebMDemuxer::GetAudioExtradata(size_t& size) const
{
	return m_pAudioTrack->GetCodecPrivate(size);
}
double WebMDemuxer::GetSampleRate() const
{
	return m_pAudioTrack->GetSamplingRate();
}
int WebMDemuxer::GetChannels() const
{
	return m_pAudioTrack->GetChannels();
}
int WebMDemuxer::GetAudioDepth() const
{
	return m_pAudioTrack->GetBitDepth();
}

bool WebMDemuxer::ReadFrame(WebMFrame* videoFrame, WebMFrame* audioFrame)
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
		else if (!m_pBlock || m_nBlockFrameIndex == m_pBlock->GetFrameCount() || NotSupportedTrackNumber(videoTrackNumber, audioTrackNumber))
		{
			status = m_pCluster->GetNext(m_pBlockEntry, m_pBlockEntry);
			if (!m_pBlockEntry || m_pBlockEntry->EOS())
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
	} while (blockEntryEOS || NotSupportedTrackNumber(videoTrackNumber, audioTrackNumber));

	WebMFrame* frame = NULL;

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

	const mkvparser::Block::Frame& blockFrame = m_pBlock->GetFrame(m_nBlockFrameIndex++);
	if (blockFrame.len > frame->m_uBufferCapacity)
	{
		unsigned char* newBuff = (unsigned char*)realloc(frame->m_pBuffer, frame->m_uBufferCapacity = blockFrame.len);
		if (newBuff)
			frame->m_pBuffer = newBuff;
		else // Out of memory
			return false;
	}
	frame->m_uBufferSize = blockFrame.len;

	frame->m_flTime = m_pBlock->GetTime(m_pCluster) / 1e9;
	frame->m_bKey = m_pBlock->IsKey();

	return !blockFrame.Read(m_pReader, frame->m_pBuffer);
}

inline bool WebMDemuxer::NotSupportedTrackNumber(long videoTrackNumber, long audioTrackNumber) const
{
	const long trackNumber = m_pBlock->GetTrackNumber();
	return (trackNumber != videoTrackNumber && trackNumber != audioTrackNumber);
}


struct VorbisDecoder
{
	vorbis_info info;
	vorbis_dsp_state dspState;
	vorbis_block block;
	ogg_packet op;

	bool hasDSPState, hasBlock;
};

class AudioVorbisDecoder
{
	AudioVorbisDecoder(const AudioVorbisDecoder&);
	void operator =(const AudioVorbisDecoder&);
public:
	AudioVorbisDecoder(const WebMDemuxer& demuxer);
	~AudioVorbisDecoder();
	AudioVorbisDecoder() = default;

	bool IsOpen() const;

	inline int GetBufferSamples() const
	{
		return m_nSamples;
	}

	bool GetPCMS16(WebMFrame& frame, short* buffer, int& numOutSamples);

public:
	bool OpenVorbis(const WebMDemuxer& demuxer);

	void Close();

	VorbisDecoder* m_pVorbis;

	int m_nSamples;
	int m_nChannels;

};

/**/

AudioVorbisDecoder::AudioVorbisDecoder(const WebMDemuxer& demuxer) :
	m_pVorbis(NULL),
	m_nSamples(0)
{
	switch (demuxer.GetAudioCodec())
	{
	case WebMDemuxer::AUDIO_VORBIS:
		m_nChannels = demuxer.GetChannels();
		if (OpenVorbis(demuxer))
			return;
		break;
	case WebMDemuxer::AUDIO_OPUS:
		m_nChannels = demuxer.GetChannels();
		break;
	default:
		return;
	}
	Close();
}
AudioVorbisDecoder::~AudioVorbisDecoder()
{
	Close();
}

bool AudioVorbisDecoder::IsOpen() const
{
	return m_pVorbis;
}

bool AudioVorbisDecoder::GetPCMS16(WebMFrame& frame, short* buffer, int& numOutSamples)
{
	if (m_pVorbis)
	{
		m_pVorbis->op.packet = frame.m_pBuffer;
		m_pVorbis->op.bytes = frame.m_uBufferSize;

		if (vorbis_synthesis(&m_pVorbis->block, &m_pVorbis->op))
			return false;
		if (vorbis_synthesis_blockin(&m_pVorbis->dspState, &m_pVorbis->block))
			return false;

		const int maxSamples = GetBufferSamples();
		int samplesCount, count = 0;
		float** pcm;
		while ((samplesCount = vorbis_synthesis_pcmout(&m_pVorbis->dspState, &pcm)))
		{
			const int toConvert = samplesCount <= maxSamples ? samplesCount : maxSamples;
			for (int c = 0; c < m_nChannels; ++c)
			{
				float* samples = pcm[c];
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

bool AudioVorbisDecoder::OpenVorbis(const WebMDemuxer& demuxer)
{
	size_t extradataSize = 0;
	const unsigned char* extradata = demuxer.GetAudioExtradata(extradataSize);

	if (extradataSize < 3 || !extradata || extradata[0] != 2)
		return false;

	size_t headerSize[3] = { 0 };
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

	op[0].packet = (unsigned char*)extradata + offset;
	op[0].bytes = headerSize[0];
	op[0].b_o_s = 1;

	op[1].packet = (unsigned char*)extradata + offset + headerSize[0];
	op[1].bytes = headerSize[1];

	op[2].packet = (unsigned char*)extradata + offset + headerSize[0] + headerSize[1];
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

	if (m_pVorbis->info.channels != m_nChannels || m_pVorbis->info.rate != demuxer.GetSampleRate())
		return false;

	if (vorbis_block_init(&m_pVorbis->dspState, &m_pVorbis->block))
		return false;
	m_pVorbis->hasBlock = true;

	memset(&m_pVorbis->op, 0, sizeof m_pVorbis->op);

	m_nSamples = 4096 / m_nChannels;

	return true;
}

void AudioVorbisDecoder::Close()
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

class VPXDecoder
{
	VPXDecoder(const VPXDecoder&);
	void operator =(const VPXDecoder&);
public:
	class Image
	{
	public:
		int GetWidth(int plane) const;
		int GetHeight(int plane) const;

		int w, h;
		int cs;
		int chromaShiftW, chromaShiftH;
		unsigned char* planes[3];
		int linesize[3];
	};

	enum IMAGE_ERROR
	{
		VPX_UNSUPPORTED_FRAME = -1,
		VPX_NO_ERROR,
		VPX_NO_FRAME
	};

	VPXDecoder(const WebMDemuxer& demuxer, unsigned threads = 1);
	~VPXDecoder();
	VPXDecoder() = default;

	inline bool IsOpen() const
	{
		return (bool)m_pCtx;
	}

	inline int GetFramesDelay() const
	{
		return m_nDelay;
	}

	bool Decode(const WebMFrame& frame);
	IMAGE_ERROR GetImage(Image& image); //The data is NOT copied! Only 3-plane, 8-bit images are supported.

public:
	vpx_codec_ctx* m_pCtx;
	const void* m_pIter;
	int m_nDelay;
	int m_nLastSpace;
};

VPXDecoder::VPXDecoder(const WebMDemuxer& demuxer, unsigned threads) :
	m_pCtx(NULL),
	m_pIter(NULL),
	m_nDelay(0),
	m_nLastSpace(VPX_CS_UNKNOWN)
{
	if (threads > 8)
		threads = 8;
	else if (threads < 1)
		threads = 1;

	const vpx_codec_dec_cfg_t codecCfg = {
		threads,
		0,
		0
	};
	vpx_codec_iface_t* codecIface = NULL;

	switch (demuxer.GetVideoCodec())
	{
	case WebMDemuxer::VIDEO_VP8:
		codecIface = vpx_codec_vp8_dx();
		break;
	case WebMDemuxer::VIDEO_VP9:
		codecIface = vpx_codec_vp9_dx();
		m_nDelay = threads - 1;
		break;
	default:
		return;
	}

	m_pCtx = new vpx_codec_ctx_t;
	if (vpx_codec_dec_init(m_pCtx, codecIface, &codecCfg, m_nDelay > 0 ? VPX_CODEC_USE_FRAME_THREADING : 0))
	{
		delete m_pCtx;
		m_pCtx = NULL;
	}
}
VPXDecoder::~VPXDecoder()
{
	if (m_pCtx)
	{
		vpx_codec_destroy(m_pCtx);
		delete m_pCtx;
	}
}

bool VPXDecoder::Decode(const WebMFrame& frame)
{
	m_pIter = NULL;
	return !vpx_codec_decode(m_pCtx, frame.m_pBuffer, frame.m_uBufferSize, NULL, 0);
}
VPXDecoder::IMAGE_ERROR VPXDecoder::GetImage(Image& image)
{
	IMAGE_ERROR err = VPX_NO_FRAME;
	if (vpx_image_t* img = vpx_codec_get_frame(m_pCtx, &m_pIter))
	{
		// It seems to be a common problem that UNKNOWN comes up a lot, yet FFMPEG is somehow getting accurate colour-space information.
		// After checking FFMPEG code, *they're* getting colour-space information, so I'm assuming something like this is going on.
		// It appears to work, at least.
		if (img->cs != VPX_CS_UNKNOWN)
			m_nLastSpace = img->cs;
		if ((img->fmt & VPX_IMG_FMT_PLANAR) && !(img->fmt & (VPX_IMG_FMT_HAS_ALPHA | VPX_IMG_FMT_HIGHBITDEPTH)))
		{
			if (img->stride[0] && img->stride[1] && img->stride[2])
			{
				const int uPlane = !!(img->fmt & VPX_IMG_FMT_UV_FLIP) + 1;
				const int vPlane = !(img->fmt & VPX_IMG_FMT_UV_FLIP) + 1;

				image.w = img->d_w;
				image.h = img->d_h;
				image.cs = m_nLastSpace;
				image.chromaShiftW = img->x_chroma_shift;
				image.chromaShiftH = img->y_chroma_shift;

				image.planes[0] = img->planes[0];
				image.planes[1] = img->planes[uPlane];
				image.planes[2] = img->planes[vPlane];

				image.linesize[0] = img->stride[0];
				image.linesize[1] = img->stride[uPlane];
				image.linesize[2] = img->stride[vPlane];

				err = VPX_NO_ERROR;
			}
		}
		else
		{
			err = VPX_UNSUPPORTED_FRAME;
		}
	}
	return err;
}

/**/

static inline int ceilRshift(int val, int shift)
{
	return (val + (1 << shift) - 1) >> shift;
}

int VPXDecoder::Image::GetWidth(int plane) const
{
	if (!plane)
		return w;
	return ceilRshift(w, chromaShiftW);
}
int VPXDecoder::Image::GetHeight(int plane) const
{
	if (!plane)
		return h;
	return ceilRshift(h, chromaShiftH);
}

/*
WEBM DECODER END
*/

void WebMPlayer::PlayVideo(const char* szPath, SDL_Window* pSDLWindow)
{
	float flVideoFrameDelay;
	int width, height;
	uint8_t* pOutFrame, * y_ptr, * u_ptr, *v_ptr;
	double flStartTime;
	short* pcm;
#ifndef GLQUAKE
	SDL_Renderer* pRenderer;
	SDL_Texture* pTexture;
	SDL_Rect videoRect;
#endif
	double flVideoPos;
	bool bCreatedRenderer;
	const char* pszPrevScale;
	int nWindowW, nWindowH;
	mkvparser::MkvReader reader;
	WebMFrame videoFrame;
	WebMFrame audioFrame;
	SDL_Event event;
#ifdef GLQUAKE
	GLuint texture;
	int nSrcVideoH, nSrcVideoW;
#endif

	Con_Printf(const_cast<char*>("WebMPlayer::PlayVideo %s\n"), szPath);

	if (reader.Open(szPath) != 0)
		return Con_Printf(const_cast<char*>("failed to find video file %s\n"), szPath);

	WebMDemuxer demuxer(&reader);

	if (!demuxer.IsOpen() || demuxer.m_pVideoTrack->GetWidth() <= 0 || demuxer.m_pVideoTrack->GetHeight() <= 0)
		return;

	sfx_t* snd = S_PrecacheSound(const_cast<char*>("UI/valve_sound.wav"));

	if (snd != NULL)
		S_StartDynamicSound(0, CHAN_ITEM, snd, vec3_t{}, VOL_NORM, 1.0f, 0, PITCH_NORM);
	else
		Con_DPrintf(const_cast<char*>("invalid sound %s\n"), "UI/valve_sound.wav");

	SDL_GetWindowSize(pSDLWindow, &nWindowW, &nWindowH);
#ifndef GLQUAKE
	pRenderer = SDL_GetRenderer(pSDLWindow);
	bCreatedRenderer = 0;

	if (!pRenderer)
	{
		pRenderer = SDL_CreateRenderer(pSDLWindow, -1, 0);
		bCreatedRenderer = 1;
	}

	pszPrevScale = SDL_GetHint("SDL_RENDER_SCALE_QUALITY");
	SDL_SetHint("SDL_RENDER_SCALE_QUALITY", "2");

	pTexture = (SDL_Texture*)SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_BGR888, SDL_TEXTUREACCESS_TARGET, demuxer.m_pVideoTrack->GetWidth(), demuxer.m_pVideoTrack->GetHeight());
#endif

#ifdef GLQUAKE
	glGenTextures(1, &texture);
	glEnable(GL_TEXTURE_RECTANGLE);
	glBindTexture(GL_TEXTURE_RECTANGLE, texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glDisable(GL_BLEND);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, (GLdouble)nWindowW, (GLdouble)nWindowH, 0.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glViewport(0, 0, nWindowW, nWindowH);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
#endif

	int nVideoW = demuxer.m_pVideoTrack->GetWidth();
	int nVideoH = demuxer.m_pVideoTrack->GetHeight();

	float flAspect = (float)nVideoW / (float)nVideoH;

	if (nWindowW >= nVideoW)
	{
		if (nWindowH < nVideoH)
		{
			nVideoH = nWindowH;

			nVideoW = flAspect * (float)nWindowH;
		}

		if (nWindowW <= nVideoW)
		{
			if (nWindowH > nVideoH)
			{
				nVideoW = nWindowW;
				nVideoH = (float)nWindowW / flAspect;
			}
		}
		else
		{
			nVideoH = nWindowH;
			nVideoW = flAspect * (float)nWindowH;
		}
	}
	else
	{
		nVideoH = nWindowH;

		if (nWindowH < int((float)nWindowW / flAspect))
		{
			nVideoW = flAspect * (float)nWindowH;
		}
		else
			nVideoW = nWindowW;
	}

#ifndef GLQUAKE
	videoRect.y = nWindowH / 2 - nVideoH / 2;
	videoRect.w = nVideoW;
	videoRect.h = nVideoH;
	videoRect.x = nWindowW / 2 - nVideoW / 2;

	SDL_SetRenderDrawColor(pRenderer, 0, 0, 0, 0);
	SDL_RenderClear(pRenderer);
	SDL_RenderPresent(pRenderer);
#endif

#ifdef GLQUAKE
	nSrcVideoW = nWindowW / 2 - nVideoW / 2;
	nSrcVideoH = nWindowH / 2 - nVideoH / 2;
#endif

	audioFrame.m_uBufferCapacity = 0;
	audioFrame.m_pBuffer = 0;
	audioFrame.m_uBufferSize = 8;

	VPXDecoder videoDec(demuxer, 8);
	AudioVorbisDecoder audioDec(demuxer);

	memset(&videoFrame, 0, 21);
	memset(&audioFrame, 0, 21);

	if (audioDec.m_pVorbis != NULL)
		pcm = new short[demuxer.m_pAudioTrack->GetChannels() * audioDec.GetBufferSamples()];
	else
		pcm = NULL;

	// allocate rgba buffer
	pOutFrame = new uint8_t[demuxer.m_pVideoTrack->GetWidth() * demuxer.m_pVideoTrack->GetHeight() * 4];

	Con_Printf(const_cast<char*>("Length: %f\n"), demuxer.GetLength());

	flStartTime = Sys_FloatTime();

	if (demuxer.ReadFrame(&videoFrame, &audioFrame))
	{

		float dt = 0.0f;
		bool skipIntention = false;
		int nOutSamples;
		bool skipped;
#ifdef GLQUAKE
		bool bFirstFrame = true;
#endif
		flVideoFrameDelay = flVideoPos = 0.0f;

		while (true)
		{

			if (dt + flVideoFrameDelay >= videoFrame.m_flTime)
			{
				if (videoDec.m_pCtx != NULL && videoFrame.m_uBufferSize > 0)
					//	break;
				{
					videoDec.m_pIter = NULL;
					// ...
					if (vpx_codec_decode(videoDec.m_pCtx, videoFrame.m_pBuffer, videoFrame.m_uBufferSize, NULL, 0) != 0)
					{
						Con_Printf(const_cast<char*>("Video decode error\n"));
						break;
						// destruct'n'return
					}

					while (true)
					{
						vpx_image_t* pframe = vpx_codec_get_frame(videoDec.m_pCtx, &videoDec.m_pIter);

						if (pframe == NULL)
							break; // returnto GetPCMS16 condition

						if (pframe->cs != NULL)
							videoDec.m_nLastSpace = pframe->cs;

						if ((pframe->fmt & (VPX_IMG_FMT_HIGHBITDEPTH | VPX_IMG_FMT_HAS_ALPHA | VPX_IMG_FMT_PLANAR))
							!= VPX_IMG_FMT_PLANAR)
							break;

						if (pframe->stride[0] == NULL || pframe->stride[1] == NULL || pframe->stride[2] == NULL)
							break;

						int32_t uv = pframe->stride[(pframe->fmt & VPX_IMG_FMT_UV_FLIP) == 0 ? 1 : 2];

						y_ptr = pframe->planes[0];
						width = pframe->d_w;
						height = pframe->d_h;
						v_ptr = pframe->planes[(pframe->fmt & VPX_IMG_FMT_UV_FLIP) == 0 ? 1 : 2];
						u_ptr = pframe->planes[(pframe->fmt & VPX_IMG_FMT_UV_FLIP) == 0 ? 2 : 1];

						if (pframe->x_chroma_shift == 1)
						{
							if (pframe->y_chroma_shift == 1)
								yuv420_2_rgb8888(pOutFrame, y_ptr, u_ptr, v_ptr, width, height, pframe->stride[0], uv, 4 * width, yuv2rgb565_table, 0);
							else if (pframe->y_chroma_shift == 0)
								yuv420_2_rgb8888(pOutFrame, y_ptr, u_ptr, v_ptr, width, height, pframe->stride[0], uv, 4 * width, yuv2rgb565_table, 0);
							else continue;

#ifdef GLQUAKE
							if (bFirstFrame)
								glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pOutFrame);
							else
								glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pOutFrame);
#endif
						}
						else
						{
							if (pframe->x_chroma_shift == 0 || pframe->y_chroma_shift == 0)
							{
								yuv444_2_rgb8888(pOutFrame, y_ptr, u_ptr, v_ptr, width, height, pframe->stride[0], uv, 4 * width, yuv2rgb565_table, 0);
							}
							else continue;

#ifdef GLQUAKE
							if (bFirstFrame)
								glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pOutFrame);
							else
								glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pOutFrame);
#endif
						}

#ifdef GLQUAKE
						glClear(GL_LIGHT0);
						glBegin(GL_QUADS);
						glTexCoord2i(0, 0);
						glVertex3f((GLfloat)nSrcVideoW, (GLfloat)nSrcVideoH, 0.0f);
						glTexCoord2i(width, 0);
						glVertex3f((GLfloat)(nSrcVideoW + nVideoW), (GLfloat)nSrcVideoH, 0.0f);
						glTexCoord2i(width, height);
						glVertex3f((GLfloat)(nSrcVideoH + nVideoW), (GLfloat)(nSrcVideoH + nVideoH), 0.0f);
						glTexCoord2i(0, height);
						glVertex3f(nSrcVideoW, (GLfloat)(nSrcVideoH + nVideoH), 0.0f);
						glEnd();
						bFirstFrame = false;
						SDL_GL_SwapWindow(pSDLWindow);
#else
						SDL_UpdateTexture(pTexture, NULL, pOutFrame, 4 * width);
						SDL_RenderClear(pRenderer);
						SDL_RenderCopy(pRenderer, pTexture, NULL, &videoRect);
						SDL_RenderPresent(pRenderer);
#endif
					}
				}

				if (audioDec.m_pVorbis && audioFrame.m_uBufferSize > 0 &&
					audioDec.GetPCMS16(audioFrame, pcm, nOutSamples) == false)
				{
					Con_Printf(const_cast<char*>("Audio decode error\n"));
					break;
					// destruct'n'return
				}

				if (demuxer.ReadFrame(&videoFrame, &audioFrame) == false)
				{
					break;
					// destruct'n'return
				}

				if (videoFrame.m_flTime >= demuxer.GetLength())
				{
					break;
					// destruct'n'return
				}

				flVideoFrameDelay = videoFrame.m_flTime - flVideoPos;
				flVideoPos = videoFrame.m_flTime;
			}

			// ...

			S_Update(vec3_origin, vec3_origin, vec3_origin, vec3_origin);

			while (SDL_PollEvent(&event))
			{
				if (event.type == SDL_QUIT ||
					event.type == SDL_KEYDOWN &&
					(event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_SPACE))
				{
					skipIntention = true;
				}
			}

			SDL_PumpEvents();
			Sys_Sleep(1);

			if (skipIntention)
			{
				Con_Printf(const_cast<char*>("VIDEO PLAYBACK: user skipped playback.\n"));

				sfx_t* dummy = S_PrecacheSound(const_cast<char*>("common/null.wav"));
				if (dummy != NULL)
					S_StartDynamicSound(0, CHAN_ITEM, dummy, vec3_t{}, VOL_NORM, 1.0f, 0, PITCH_NORM);
				skipped = true;
			}
			else
				skipped = false;

			dt = Sys_FloatTime() - flStartTime;

			if (skipped)
				// destruct'n'return
				break;
		}
	}

#ifndef GLQUAKE
	SDL_SetHint("SDL_RENDER_SCALE_QUALITY", pszPrevScale);
	SDL_DestroyTexture(pTexture);
	if (bCreatedRenderer)
		SDL_DestroyRenderer(pRenderer);
#endif

#ifdef GLQUAKE
	glBindTexture(GL_TEXTURE_RECTANGLE, 0);
	glDisable(GL_TEXTURE_RECTANGLE);
	glDeleteTextures(1, &texture);
#endif

	if (pOutFrame != NULL)
		delete[] pOutFrame;
	if (pcm != NULL)
		delete[] pcm;

}