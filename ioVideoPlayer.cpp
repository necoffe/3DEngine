//----------------------------------------------------------------------------------
/*
Filename	: ioVideoPlayer.cpp
Author		: NOVERA
Date		: 2026.07.18
Desc		: '.mp4' file player via Media Foundation (IMFMediaSession + EVR).
*/
//----------------------------------------------------------------------------------

#include "stdafx.h"
#include "ioVideoPlayer.h"
#include "ioMemoryPool.h"
#include "ioRenderSystem.h"
#include "ioUIRenderer.h"
#include "ioXMLElement.h"
#include "ioStringConverter.h"
#include "ioMouse.h"
#include "ioResourceLoader.h"
#include "ioStream.h"

//----------------------------------------------------------------------------------
/*
ioIQVideoPlayer - UI quad pool object (mirrors ioIQFlashPlayer in ioFlashPlayer.cpp)
*/
//----------------------------------------------------------------------------------
class ioIQVideoPlayer : public IUIQuad, public ioPoolObject< ioIQVideoPlayer, 512 >
{
public:
	ioIQVideoPlayer() {}
	ioIQVideoPlayer( const D3DXVECTOR2& pos, const D3DXVECTOR2& size, IDirect3DTexture9* tex, DWORD color )
		: m_pos( pos )
		, m_size( size )
		, m_tex( tex )
		, m_color( color )
	{}
	virtual ~ioIQVideoPlayer() {}

	virtual int				GetQuadCount() const { return 1; }
	virtual void			AddInfo( ioQuadRenderArray *pQuadArray )
	{
		pQuadArray->AddRenderQuadInfo( m_pos, m_size, m_tex, D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 1.0f ), UI_RENDER_NORMAL, TFO_BILINEAR, m_color, 0 );
	}

public:
	D3DXVECTOR2				m_pos;
	D3DXVECTOR2				m_size;
	IDirect3DTexture9*		m_tex;
	DWORD					m_color;
};


//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
/*
VideoPlayer - Media Foundation glue
*/
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------

// Static registry of live VideoPlayer instances for device-lost broadcasting.
// Touched only on the main thread (UI wnd create/destroy + ioRenderSystem reset),
// so no synchronization is required.
#include <set>
static std::set< VideoPlayer* > g_liveVideoPlayers;

// MFStartup/Shutdown refcounted across all VideoPlayer instances.
static LONG g_sMFRefCount = 0;

void VideoPlayer::EnsureMFStartup()
{
	if ( g_sMFRefCount == 0 )
	{
		HRESULT hr = MFStartup( MF_VERSION, MFSTARTUP_FULL );
		if ( FAILED( hr ) )
		{
			LOG.PrintTimeAndLog( 0, "VideoPlayer::MFStartup Failed(%x)", hr );
		}
	}
	InterlockedIncrement( &g_sMFRefCount );
}

void VideoPlayer::EnsureMFShutdown()
{
	if ( g_sMFRefCount == 0 )
		return;

	if ( InterlockedDecrement( &g_sMFRefCount ) == 0 )
	{
		MFShutdown();
	}
}

//----------------------------------------------------------------------------------
/*
Construction / Destruction
*/
VideoPlayer::VideoPlayer()
	: m_pSession( NULL )
	, m_pDisplay( NULL )
	, m_pAudioVolume( NULL )
	, m_pByteStream( NULL )
	, m_hClipWnd( NULL )
	, m_pTexture( NULL )
	, m_bTextureLost( false )
	, m_uWidth( 0 )
	, m_uHeight( 0 )
	, m_uVideoW( 0 )
	, m_uVideoH( 0 )
	, m_bLoop( false )
	, m_fVolume( 1.0f )
	, m_bMute( false )
	, m_color( 0xFFFFFFFF )
	, m_eState( STATE_IDLE )
	, m_llLastFrameStamp( 0 )
	, m_dwLastUpdateMs( 0 )
	, m_uMinUpdateIntervalMs( 33 )		// default cap: 30fps pull (MP4 source fps typical)
{
	g_liveVideoPlayers.insert( this );
}

VideoPlayer::~VideoPlayer()
{
	g_liveVideoPlayers.erase( this );
	Destroy();
}

//----------------------------------------------------------------------------------
/*
Init / Destroy
*/
bool VideoPlayer::Init( IDirect3DDevice9* device, unsigned int width, unsigned int height )
{
	Destroy();

	EnsureMFStartup();

	m_uWidth  = width;
	m_uHeight = height;

	// Create message-only window for windowless EVR clipping.
	if ( m_hClipWnd == NULL )
	{
		m_hClipWnd = CreateWindowExW( 0, L"Static", L"ioVideoPlayerClip",
			WS_POPUP, 0, 0, 0, 0, HWND_MESSAGE, NULL,
			GetModuleHandleW( NULL ), NULL );
		if ( m_hClipWnd == NULL )
		{
			LOG.PrintTimeAndLog( 0, "VideoPlayer::Init - CreateWindow Failed(%x)", GetLastError() );
			return false;
		}
	}

	// Create the Media Session.
	HRESULT hr = MFCreateMediaSession( NULL, &m_pSession );
	if ( FAILED( hr ) || m_pSession == NULL )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::MFCreateMediaSession Failed(%x)", hr );
		Destroy();
		return false;
	}

	// Query audio volume service (available before topology is set).
	IMFGetService* pGS = NULL;
	hr = m_pSession->QueryInterface( IID_PPV_ARGS( &pGS ) );
	if ( SUCCEEDED( hr ) && pGS )
	{
		pGS->GetService( MR_STREAM_VOLUME_SERVICE, IID_PPV_ARGS( &m_pAudioVolume ) );
		SAFERELEASE( pGS );
	}

	return true;
}

void VideoPlayer::Destroy()
{
	DestroySession();

	ReleaseTexture();

	if ( m_hClipWnd )
	{
		DestroyWindow( m_hClipWnd );
		m_hClipWnd = NULL;
	}

	EnsureMFShutdown();

	m_eState = STATE_IDLE;
	m_strFilename.clear();
	m_uWidth = m_uHeight = 0;
	m_uVideoW = m_uVideoH = 0;
}

void VideoPlayer::DestroySession()
{
	if ( m_pSession )
	{
		m_pSession->Stop();
		// Allow session to flush async state.
		HRESULT hr = m_pSession->Close();
		if ( SUCCEEDED( hr ) )
		{
			IMFMediaEvent* pEvent = NULL;
			m_pSession->GetEvent( 0, &pEvent ); // MESessionClosed
			SAFERELEASE( pEvent );
		}
		m_pSession->Shutdown();
	}

	SAFERELEASE( m_pAudioVolume );
	SAFERELEASE( m_pDisplay );
	// Byte stream release after session shutdown: the source (held by session)
	// had its own ref on the byte stream; session shutdown -> source release -> byte
	// stream refcount drops. Our ref here is the last keepalive for the HGLOBAL/IStream.
	SAFERELEASE( m_pByteStream );
	SAFERELEASE( m_pSession );
}

//----------------------------------------------------------------------------------
/*
Resize (draw quad size only; texture stays at video native size)
*/
void VideoPlayer::Resize( IDirect3DDevice9* device, unsigned int width, unsigned int height )
{
	m_uWidth  = width;
	m_uHeight = height;

	if ( m_pDisplay )
	{
		RECT rc = { 0, 0, (LONG)width, (LONG)height };
		m_pDisplay->SetVideoPosition( NULL, &rc );
	}

	(void)device;
}

//----------------------------------------------------------------------------------
/*
Load .mp4 file (build topology: source -> EVR (video) + SAR (audio))
*/
bool VideoPlayer::Load( const char* filename )
{
	if ( !m_pSession || !filename )
		return false;

	m_strFilename = filename;

	// Reset any prior pack byte stream (Reload from disk after pack, etc.)
	SAFERELEASE( m_pByteStream );

	DestroySession();

	HRESULT hr = MFCreateMediaSession( NULL, &m_pSession );
	if ( FAILED( hr ) || m_pSession == NULL )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::Load - MFCreateMediaSession Failed(%x)", hr );
		return false;
	}

	// Re-query audio volume service for the new session.
	IMFGetService* pGS = NULL;
	hr = m_pSession->QueryInterface( IID_PPV_ARGS( &pGS ) );
	if ( SUCCEEDED( hr ) && pGS )
	{
		pGS->GetService( MR_STREAM_VOLUME_SERVICE, IID_PPV_ARGS( &m_pAudioVolume ) );
		SAFERELEASE( pGS );
	}

	// Branch: pack mode (in-memory IMFByteStream) vs dev/disk mode (file:// URL).
	IMFMediaSource* pSource = NULL;

	if ( g_ResourceLoader.IsLoadInPack() && g_ResourceLoader.IsFileExist( filename, true ) )
	{
		if ( !CreateSourceFromPack_( filename, &pSource ) )
		{
			LOG.PrintTimeAndLog( 0, "VideoPlayer::Load - CreateSourceFromPack_ Failed(%s)", filename );
			return false;
		}
	}
	else
	{
		// Convert CP949/MBCS path to wide. Use system codepage (Korean 949).
		int wlen = MultiByteToWideChar( CP_ACP, 0, filename, -1, NULL, 0 );
		if ( wlen <= 0 )
			return false;
		wchar_t* wszURL = new wchar_t[ wlen ];
		MultiByteToWideChar( CP_ACP, 0, filename, -1, wszURL, wlen );

		// Resolve relatif -> absolut (MF CreateObjectFromURL tidak resolve relative file URL).
		wchar_t wszAbsPath[ MAX_PATH * 2 ] = { 0 };
		DWORD dwLen = GetFullPathNameW( wszURL, MAX_PATH * 2, wszAbsPath, NULL );
		SAFEDELETEARRAY( wszURL );
		if ( dwLen == 0 || dwLen > MAX_PATH * 2 )
		{
			LOG.PrintTimeAndLog( 0, "VideoPlayer::Load - GetFullPathNameW Failed(%s)", filename );
			return false;
		}
		// Normalisasi backslash -> slash untuk validitas file:// URL.
		for ( wchar_t* p = wszAbsPath; *p; ++p )
		{
			if ( *p == L'\\' ) *p = L'/';
		}

		// Convert to file:// URL form required by MF source resolver.
		wchar_t wszFileURL[ MAX_PATH * 4 ] = { 0 };
		StringCchPrintfW( wszFileURL, MAX_PATH * 4, L"file:///%s", wszAbsPath );

		if ( !CreateSourceFromURL_( wszFileURL, &pSource ) )
		{
			LOG.PrintTimeAndLog( 0, "VideoPlayer::Load - CreateSourceFromURL_ Failed(%s)", filename );
			return false;
		}
	}

	bool bOk = BuildTopology( pSource );
	SAFERELEASE( pSource );   // topology/source node holds its own ref
	if ( !bOk )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::Load - BuildTopology Failed(%s)", filename );
		return false;
	}

	Play();

	return true;
}

bool VideoPlayer::Reload()
{
	if ( m_strFilename.empty() )
		return false;
	return Load( m_strFilename.c_str() );
}

//----------------------------------------------------------------------------------
/*
Build MF topology from an already-created media source.
Connects video stream -> EVR, audio stream -> SAR.
Caller owns pSource (releases after this returns). The topology/source node
holds its own ref (via SetUnknown) for the session lifetime.
*/
bool VideoPlayer::BuildTopology( IMFMediaSource* pSource )
{
	if ( !m_pSession || !pSource )
		return false;

	IMFPresentationDescriptor* pPD = NULL;
	HRESULT hr = pSource->CreatePresentationDescriptor( &pPD );
	if ( FAILED( hr ) || !pPD )
		return false;

	// 2. Create EVR media sink via MFCreateVideoRenderer (MF-native, no DirectShow CLSID).
	IMFMediaSink* pVideoSink = NULL;
	hr = MFCreateVideoRenderer( IID_PPV_ARGS( &pVideoSink ) );
	if ( FAILED( hr ) || !pVideoSink )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::MFCreateVideoRenderer Failed(%x)", hr );
		SAFERELEASE( pPD );
		return false;
	}

	IMFGetService* pVSvc = NULL;
	pVideoSink->QueryInterface( IID_PPV_ARGS( &pVSvc ) );
	if ( pVSvc )
	{
		pVSvc->GetService( MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS( &m_pDisplay ) );
		SAFERELEASE( pVSvc );
	}
	if ( !m_pDisplay )
	{
		SAFERELEASE( pVideoSink );
		SAFERELEASE( pPD );
		return false;
	}

	m_pDisplay->SetVideoWindow( m_hClipWnd );
	RECT rc = { 0, 0, (LONG)m_uWidth, (LONG)m_uHeight };
	m_pDisplay->SetVideoPosition( NULL, &rc );
	m_pDisplay->SetAspectRatioMode(MFVideoARMode_None);

	// Get EVR's stream sink (single stream at index 0).
	IMFStreamSink* pVideoStreamSink = NULL;
	pVideoSink->GetStreamSinkByIndex( 0, &pVideoStreamSink );

	// 3. Create SAR (audio renderer) media sink. Skip entirely when muted at Load
	//    time - saves the audio decoder MFT + SAR pipeline (mute was the goal anyway).
	//    Note: SetMute(false) AFTER Load will NOT retroactively rebuild audio; callers
	//    wanting audio must leave m_bMute=false (the default) before calling Load.
	IMFMediaSink* pAudioSink = NULL;
	IMFStreamSink* pAudioStreamSink = NULL;
	if ( !m_bMute )
	{
		hr = MFCreateAudioRenderer( NULL, &pAudioSink );
		if ( SUCCEEDED( hr ) && pAudioSink )
		{
			pAudioSink->GetStreamSinkByIndex( 0, &pAudioStreamSink );
		}
	}

	// 4. Build topology: iterate streams, select video+audio, connect to sinks.
	IMFTopology* pTopology = NULL;
	MFCreateTopology( &pTopology );
	if ( !pTopology )
	{
		SAFERELEASE( pAudioStreamSink );
		SAFERELEASE( pAudioSink );
		SAFERELEASE( pVideoStreamSink );
		SAFERELEASE( pVideoSink );
		SAFERELEASE( pPD );
		return false;
	}

	DWORD cStreams = 0;
	pPD->GetStreamDescriptorCount( &cStreams );
	bool bHasVideo = false;
	bool bHasAudio = false;

	for ( DWORD i = 0; i < cStreams; i++ )
	{
		BOOL bSelected = FALSE;
		IMFStreamDescriptor* pSD = NULL;
		hr = pPD->GetStreamDescriptorByIndex( i, &bSelected, &pSD );
		if ( FAILED( hr ) || !pSD )
			continue;

		IMFMediaTypeHandler* pMT = NULL;
		pSD->GetMediaTypeHandler( &pMT );
		if ( !pMT )
		{
			SAFERELEASE( pSD );
			continue;
		}

		GUID guidMajor = GUID_NULL;
		pMT->GetMajorType( &guidMajor );

		bool bConnectVideo = ( guidMajor == MFMediaType_Video ) && !bHasVideo && ( pVideoStreamSink != NULL );
		bool bConnectAudio = ( guidMajor == MFMediaType_Audio ) && !bHasAudio && ( pAudioStreamSink != NULL );

		SAFERELEASE( pMT );

		if ( !bConnectVideo && !bConnectAudio )
		{
			SAFERELEASE( pSD );
			continue;
		}

		// Select the stream.
		pPD->SelectStream( i );

		// Source node.
		IMFTopologyNode* pSourceNode = NULL;
		MFCreateTopologyNode( MF_TOPOLOGY_SOURCESTREAM_NODE, &pSourceNode );
		if ( pSourceNode )
		{
			pSourceNode->SetUnknown( MF_TOPONODE_SOURCE, pSource );
			pSourceNode->SetUnknown( MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD );
			pSourceNode->SetUnknown( MF_TOPONODE_STREAM_DESCRIPTOR, pSD );
			pTopology->AddNode( pSourceNode );
		}

		// Output node. Stream ID is queried from the sink itself (EVR/SAR).
		IMFTopologyNode* pOutputNode = NULL;
		MFCreateTopologyNode( MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode );
		if ( pOutputNode )
		{
			IMFStreamSink* pSink = bConnectVideo ? pVideoStreamSink : pAudioStreamSink;
			DWORD dwStreamId = 0;
			if ( pSink ) pSink->GetIdentifier( &dwStreamId );
			pOutputNode->SetObject( pSink );
			pOutputNode->SetUINT32( MF_TOPONODE_STREAMID, dwStreamId );
			// Skip preroll buffering - lower startup latency, less decoder warm-up work.
			pOutputNode->SetUINT32( MF_TOPONODE_DISABLE_PREROLL, TRUE );
			pTopology->AddNode( pOutputNode );
		}

		if ( pSourceNode && pOutputNode )
		{
			pSourceNode->ConnectOutput( 0, pOutputNode, 0 );
		}

		if ( bConnectVideo ) bHasVideo = true;
		if ( bConnectAudio ) bHasAudio = true;

		SAFERELEASE( pSourceNode );
		SAFERELEASE( pOutputNode );
		SAFERELEASE( pSD );
	}

	// Release our local refs to the sinks; the topology/output nodes hold their own
	// refs (via SetObject), and the session owns shutting them down.
	SAFERELEASE( pAudioStreamSink );
	SAFERELEASE( pAudioSink );
	SAFERELEASE( pVideoStreamSink );
	SAFERELEASE( pVideoSink );
	SAFERELEASE( pPD );

	if ( !bHasVideo )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::BuildTopology - No video stream found" );
		SAFERELEASE( pTopology );
		return false;
	}

	// 5. Set topology on session (async; topology resolves on session thread).
	hr = m_pSession->SetTopology( MFSESSION_SETTOPOLOGY_IMMEDIATE, pTopology );

	SAFERELEASE( pTopology );

	if ( FAILED( hr ) )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::SetTopology Failed(%x)", hr );
		return false;
	}

	// Source is held by the source node (SetUnknown AddRef'd it) for session lifetime.
	// Caller releases its own ref after we return.

	ApplyVolumeAndMute();

	return true;
}

//----------------------------------------------------------------------------------
/*
Create media source from a file:// URL (disk/dev mode). Caller releases *ppSource.
*/
bool VideoPlayer::CreateSourceFromURL_( const wchar_t* wszFileURL, IMFMediaSource** ppSource )
{
	if ( !ppSource )
		return false;
	*ppSource = NULL;

	IMFSourceResolver* pResolver = NULL;
	HRESULT hr = MFCreateSourceResolver( &pResolver );
	if ( FAILED( hr ) || !pResolver )
		return false;

	IUnknown* pSourceUnk = NULL;
	MF_OBJECT_TYPE objType = MF_OBJECT_INVALID;
	hr = pResolver->CreateObjectFromURL( wszFileURL, MF_RESOLUTION_MEDIASOURCE,
		NULL, &objType, &pSourceUnk );
	SAFERELEASE( pResolver );
	if ( FAILED( hr ) || !pSourceUnk )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::CreateObjectFromURL Failed(%x)", hr );
		return false;
	}

	pSourceUnk->QueryInterface( IID_PPV_ARGS( ppSource ) );
	SAFERELEASE( pSourceUnk );
	if ( *ppSource == NULL )
		return false;

	return true;
}

//----------------------------------------------------------------------------------
/*
Create media source from an in-memory IMFByteStream backed by a pack (.iop) file.
Caller releases *ppSource. The byte stream is kept alive in m_pByteStream for the
session lifetime (released in DestroySession after session shutdown).
*/
bool VideoPlayer::CreateSourceFromPack_( const char* szInPackPath, IMFMediaSource** ppSource )
{
	if ( !ppSource || !szInPackPath )
		return false;
	*ppSource = NULL;

	// 1. Read packed file into memory via g_ResourceLoader.
	ioBinaryStream kStream;
	if ( !g_ResourceLoader.LoadStream( szInPackPath, &kStream ) )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::CreateSourceFromPack_ - LoadStream Failed(%s)", szInPackPath );
		return false;
	}

	BYTE* pBuf = kStream.GetPtr();
	int iSize = kStream.GetSize();
	if ( !pBuf || iSize <= 0 )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::CreateSourceFromPack_ - Empty stream(%s)", szInPackPath );
		return false;
	}

	// 2. Copy into an HGLOBAL. COM IStream needs stable memory independent of the
	//    pack buffer (which is freed when kStream destructs at function exit).
	HGLOBAL hMem = GlobalAlloc( GMEM_MOVEABLE, (SIZE_T)iSize );
	if ( !hMem )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::CreateSourceFromPack_ - GlobalAlloc Failed(%d)", iSize );
		return false;
	}
	void* pMem = GlobalLock( hMem );
	if ( !pMem )
	{
		GlobalFree( hMem );
		return false;
	}
	memcpy( pMem, pBuf, (size_t)iSize );
	GlobalUnlock( hMem );

	// 3. Wrap HGLOBAL in IStream (TRUE => free hMem on final release), then in IMFByteStream.
	IStream* pStream = NULL;
	HRESULT hr = CreateStreamOnHGlobal( hMem, TRUE, &pStream );
	if ( FAILED( hr ) || !pStream )
	{
		GlobalFree( hMem );
		LOG.PrintTimeAndLog( 0, "VideoPlayer::CreateSourceFromPack_ - CreateStreamOnHGlobal Failed(%x)", hr );
		return false;
	}

	hr = MFCreateMFByteStreamOnStream( pStream, &m_pByteStream );
	SAFERELEASE( pStream );   // m_pByteStream AddRef'd it; release our local ref
	if ( FAILED( hr ) || !m_pByteStream )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::CreateSourceFromPack_ - MFCreateMFByteStreamOnStream Failed(%x)", hr );
		return false;
	}

	// 4. Resolve the byte stream into an IMFMediaSource.
	IMFSourceResolver* pResolver = NULL;
	hr = MFCreateSourceResolver( &pResolver );
	if ( FAILED( hr ) || !pResolver )
		return false;

	IUnknown* pSourceUnk = NULL;
	MF_OBJECT_TYPE objType = MF_OBJECT_INVALID;

	// MF source resolver selects the byte-stream handler by the URL's file
	// extension (MP4 handler is registered on .mp4, does NOT byte-sniff). NULL
	// pwszURL -> no handler match -> MF_E_UNSUPPORTED_FORMAT (0xC00D36C4). Pass the
	// pack path (ends in .mp4) as hint; resolver does not open the URL, it only
	// reads the filename/extension. Byte data still comes from m_pByteStream.
	wchar_t wszUrlHint[ MAX_PATH * 2 ] = { 0 };
	MultiByteToWideChar( CP_ACP, 0, szInPackPath, -1, wszUrlHint, MAX_PATH * 2 );

	hr = pResolver->CreateObjectFromByteStream( m_pByteStream, wszUrlHint,
		MF_RESOLUTION_MEDIASOURCE, NULL, &objType, &pSourceUnk );
	SAFERELEASE( pResolver );
	if ( FAILED( hr ) || !pSourceUnk )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::CreateSourceFromPack_ - CreateObjectFromByteStream Failed(%x)", hr );
		return false;
	}

	pSourceUnk->QueryInterface( IID_PPV_ARGS( ppSource ) );
	SAFERELEASE( pSourceUnk );
	if ( *ppSource == NULL )
		return false;

	return true;
}

//----------------------------------------------------------------------------------
/*
Control
*/
void VideoPlayer::Play()
{
	if ( !m_pSession )
		return;

	// Start from beginning (PROPVARIANT VT_I8 = 0 means position 0 in 100ns units).
	PROPVARIANT var;
	PropVariantInit( &var );
	var.vt = VT_I8;
	var.hVal.QuadPart = 0;

	HRESULT hr = m_pSession->Start( &GUID_NULL, &var );

	if ( SUCCEEDED( hr ) )
		m_eState = STATE_PLAYING;
	else
		LOG.PrintTimeAndLog( 0, "VideoPlayer::Play Start Failed(%x)", hr );
}

void VideoPlayer::Stop()
{
	if ( !m_pSession )
		return;

	HRESULT hr = m_pSession->Stop();
	if ( SUCCEEDED( hr ) )
		m_eState = STATE_STOPPED;
}

void VideoPlayer::Pause()
{
	if ( !m_pSession )
		return;

	HRESULT hr = m_pSession->Pause();
	if ( SUCCEEDED( hr ) )
		m_eState = STATE_PAUSED;
}

void VideoPlayer::Resume()
{
	if ( !m_pSession )
		return;

	// Resume from current position.
	PROPVARIANT var;
	PropVariantInit( &var );
	HRESULT hr = m_pSession->Start( NULL, &var );

	if ( SUCCEEDED( hr ) )
		m_eState = STATE_PLAYING;
}

void VideoPlayer::SetLoop( bool bLoop )
{
	m_bLoop = bLoop;
}

void VideoPlayer::SetVolume( float fVolume )
{
	if ( fVolume < 0.0f ) fVolume = 0.0f;
	if ( fVolume > 1.0f ) fVolume = 1.0f;
	m_fVolume = fVolume;
	ApplyVolumeAndMute();
}

void VideoPlayer::SetMute( bool bMute )
{
	m_bMute = bMute;
	ApplyVolumeAndMute();
}

void VideoPlayer::ApplyVolumeAndMute()
{
	if ( !m_pAudioVolume )
		return;

	if ( m_bMute )
	{
		m_pAudioVolume->SetMute( TRUE );
	}
	else
	{
		m_pAudioVolume->SetMute( FALSE );
		m_pAudioVolume->SetMasterVolume( m_fVolume );
	}
}

void VideoPlayer::SetMaxUpdateFps( DWORD fps )
{
	// 0 = disable throttle (pull every render frame). Otherwise cap at 1000/fps ms.
	m_uMinUpdateIntervalMs = ( fps == 0 ) ? 0 : (DWORD)( 1000 / ( fps < 1 ? 1 : fps ) );
}

//----------------------------------------------------------------------------------
/*
Poll session events for loop / state transitions.
*/
void VideoPlayer::PollSessionEvents()
{
	if ( !m_pSession )
		return;

	IMFMediaEventGenerator* pGen = NULL;
	if ( FAILED( m_pSession->QueryInterface( IID_PPV_ARGS( &pGen ) ) ) || !pGen )
		return;

	for ( ;; )
	{
		IMFMediaEvent* pEvent = NULL;
		HRESULT hr = pGen->GetEvent( MF_EVENT_FLAG_NO_WAIT, &pEvent );
		if ( FAILED( hr ) || !pEvent )
			break;

		MediaEventType met = MEUnknown;
		pEvent->GetType( &met );

		if ( met == MESessionEnded )
		{
			if ( m_bLoop )
			{
				// Restart from beginning.
				PROPVARIANT var;
				PropVariantInit( &var );
				var.vt = VT_I8;
				var.hVal.QuadPart = 0;
				m_pSession->Start( &GUID_NULL, &var );
			}
			else
			{
				m_eState = STATE_STOPPED;
			}
		}
		else if ( met == MESessionStopped )
		{
			m_eState = STATE_STOPPED;
		}
		else if ( met == MESessionStarted )
		{
			m_eState = STATE_PLAYING;
		}
		else if ( met == MESessionPaused )
		{
			m_eState = STATE_PAUSED;
		}

		SAFERELEASE( pEvent );
	}

	SAFERELEASE( pGen );
}

//----------------------------------------------------------------------------------
/*
Texture management
*/
bool VideoPlayer::CreateTexture( IDirect3DDevice9* device, unsigned int videoW, unsigned int videoH )
{
	if ( !device || videoW == 0 || videoH == 0 )
		return false;

	ReleaseTexture();

	HRESULT hr = device->CreateTexture( videoW, videoH, 1, D3DUSAGE_DYNAMIC,
		D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_pTexture, NULL );
	if ( FAILED( hr ) || !m_pTexture )
	{
		LOG.PrintTimeAndLog( 0, "VideoPlayer::CreateTexture Failed(%x) %dx%d", hr, videoW, videoH );
		return false;
	}

	m_uVideoW = videoW;
	m_uVideoH = videoH;
	m_bTextureLost = false;
	return true;
}

void VideoPlayer::ReleaseTexture()
{
	SAFERELEASE( m_pTexture );
	m_bTextureLost = false;
}

//----------------------------------------------------------------------------------
/*
Update: poll events + pull latest frame from EVR -> dynamic texture.
Call once per render frame.
*/
void VideoPlayer::Update( IDirect3DDevice9* device )
{
	if ( !m_pDisplay )
		return;

	PollSessionEvents();

	// Throttle GetCurrentImage + texture upload: cap pull rate to m_uMinUpdateIntervalMs.
	// PollSessionEvents still runs every frame (loop/state detection must be responsive),
	// but the expensive GPU->CPU readback + memcpy into dynamic texture only runs when
	// enough time has elapsed (default 33ms = 30fps). Source fps typically <= 30, so on
	// a 60fps render loop this halves the readback work with no visual change.
	DWORD nowMs = timeGetTime();
	if ( m_uMinUpdateIntervalMs > 0 && nowMs - m_dwLastUpdateMs < m_uMinUpdateIntervalMs )
		return;
	m_dwLastUpdateMs = nowMs;

	// Lazy-recreate texture after device reset.
	if ( m_bTextureLost && device && m_uVideoW > 0 && m_uVideoH > 0 )
	{
		CreateTexture( device, m_uVideoW, m_uVideoH );
	}

	// Pull current image from EVR. Signature (evr.h):
	//   HRESULT GetCurrentImage(BITMAPINFOHEADER *pBih [in,out],
	//                            BYTE **ppDib [out, CoTaskMemAlloc'd],
	//                            DWORD *pcbDib [out],
	//                            LONGLONG *pTimeStamp [optional, in,out]);
	// ppDib contains pixel data only (header is in *pBih).
	BITMAPINFOHEADER bih;
	ZeroMemory( &bih, sizeof( bih ) );
	bih.biSize = sizeof( BITMAPINFOHEADER );
	BYTE* pDib = NULL;
	DWORD cbDib = 0;
	LONGLONG llTimeStamp = 0;
	HRESULT hr = m_pDisplay->GetCurrentImage( &bih, &pDib, &cbDib, &llTimeStamp );
	if ( FAILED( hr ) || !pDib )
		return;

	// Dedup: EVR returns the same frame until the next source frame is decoded. If the
	// timestamp hasn't advanced since our last successful pull, the texture already holds
	// this frame - skip the LockRect + memcpy (saves the row-flip copy + upload).
	if ( llTimeStamp != 0 && llTimeStamp == m_llLastFrameStamp )
	{
		CoTaskMemFree( pDib );
		return;
	}
	m_llLastFrameStamp = llTimeStamp;

	LONG srcW = bih.biWidth;
	LONG srcH = bih.biHeight;
	bool bBottomUp = ( srcH > 0 );
	LONG absH = bBottomUp ? srcH : -srcH;
	BYTE* pPixels = pDib;	// pixel data only (header is in bih)

	if ( srcW <= 0 || absH <= 0 )
	{
		CoTaskMemFree( pDib );
		return;
	}

	// (Re)create texture if size changed or not yet created.
	if ( !m_pTexture || (LONG) m_uVideoW != srcW || (LONG) m_uVideoH != absH )
	{
		if ( device )
		{
			if ( !CreateTexture( device, (unsigned int) srcW, (unsigned int) absH ) )
			{
				CoTaskMemFree( pDib );
				return;
			}
		}
	}

	if ( !m_pTexture )
	{
		CoTaskMemFree( pDib );
		return;
	}

	// Copy DIB pixels into dynamic texture (flip vertically if bottom-up BMP).
	D3DLOCKED_RECT lr;
	ZeroMemory( &lr, sizeof( lr ) );
	hr = m_pTexture->LockRect( 0, &lr, NULL, D3DLOCK_DISCARD );
	if ( SUCCEEDED( hr ) && lr.pBits )
	{
		DWORD copyW = (DWORD) min( srcW, (LONG) m_uVideoW );
		DWORD copyH = (DWORD) min( absH, (LONG) m_uVideoH );
		DWORD rowBytes = copyW * 4;

		BYTE* pDst = (BYTE*) lr.pBits;
		for ( DWORD y = 0; y < copyH; y++ )
		{
			DWORD srcY = bBottomUp ? (DWORD)( absH - 1 - (LONG) y ) : y;
			BYTE* pSrcRow = pPixels + srcY * ( srcW * 4 );
			BYTE* pDstRow = pDst + y * lr.Pitch;
			memcpy( pDstRow, pSrcRow, rowBytes );
		}

		m_pTexture->UnlockRect( 0 );
	}
	else if ( hr == D3DERR_DEVICELOST )
	{
		// Device lost: release the stale default-pool texture; flag for recreate.
		SAFERELEASE( m_pTexture );
		m_bTextureLost = true;
	}

	CoTaskMemFree( pDib );
}

//----------------------------------------------------------------------------------
/*
Render: draw quad at draw size using current texture (GPU scales).
*/
void VideoPlayer::Render(IDirect3DDevice9* device)
{
	if (!m_pTexture || !device)
		return;

	m_vertices[0].Set(0.0f, 0.0f, 0.0f, 1.0f, m_color, 0.0f, 0.0f);
	m_vertices[1].Set((float)m_uWidth, 0.0f, 0.0f, 1.0f, m_color, 1.0f, 0.0f);
	m_vertices[2].Set(0.0f, (float)m_uHeight, 0.0f, 1.0f, m_color, 0.0f, 1.0f);
	m_vertices[3].Set((float)m_uWidth, (float)m_uHeight, 0.0f, 1.0f, m_color, 1.0f, 1.0f);

	// Save ALL fixed-function state we mutate. ioRenderSystem keeps a shadow
	// cache (s_RenderStateDesc) and only Set's on cache-miss; we restore the
	// device EXACTLY so the cache stays in sync. Previous version saved only a
	// subset and never set COLOROP -> video quad rendered solid white when the
	// previous draw left COLOROP non-texture (e.g. after a colored UI line).
	DWORD dwCull, dwBlend, dwSrcBlend, dwDestBlend, dwAlphaTest, dwLighting, dwZEnable, dwZWrite, dwFog;
	DWORD dwColorOp0, dwColorArg1_0, dwColorArg2_0, dwAlphaOp0, dwAlphaArg1_0, dwAlphaArg2_0;
	DWORD dwColorOp1, dwAlphaOp1;
	IDirect3DBaseTexture9* pOldTex = NULL;
	DWORD dwFVF = 0;

	device->GetRenderState(D3DRS_CULLMODE, &dwCull);
	device->GetRenderState(D3DRS_ALPHABLENDENABLE, &dwBlend);
	device->GetRenderState(D3DRS_SRCBLEND, &dwSrcBlend);
	device->GetRenderState(D3DRS_DESTBLEND, &dwDestBlend);
	device->GetRenderState(D3DRS_ALPHATESTENABLE, &dwAlphaTest);
	device->GetRenderState(D3DRS_LIGHTING, &dwLighting);
	device->GetRenderState(D3DRS_ZENABLE, &dwZEnable);
	device->GetRenderState(D3DRS_ZWRITEENABLE, &dwZWrite);
	device->GetRenderState(D3DRS_FOGENABLE, &dwFog);
	device->GetTextureStageState(0, D3DTSS_COLOROP, &dwColorOp0);
	device->GetTextureStageState(0, D3DTSS_COLORARG1, &dwColorArg1_0);
	device->GetTextureStageState(0, D3DTSS_COLORARG2, &dwColorArg2_0);
	device->GetTextureStageState(0, D3DTSS_ALPHAOP, &dwAlphaOp0);
	device->GetTextureStageState(0, D3DTSS_ALPHAARG1, &dwAlphaArg1_0);
	device->GetTextureStageState(0, D3DTSS_ALPHAARG2, &dwAlphaArg2_0);
	device->GetTextureStageState(1, D3DTSS_COLOROP, &dwColorOp1);
	device->GetTextureStageState(1, D3DTSS_ALPHAOP, &dwAlphaOp1);
	device->GetTexture(0, &pOldTex);	// AddRefs
	device->GetFVF(&dwFVF);

	// Full pipeline: pre-transformed, textured, alpha-blended quad.
	// MODULATE routes the EVR frame (TEXTURE) tinted by m_color (DIFFUSE);
	// alpha = frame.a * m_color.a so SetColor(alpha) fade-in actually works
	// (was dead before because ALPHABLENDENABLE was FALSE).
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);
	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_FOGENABLE, FALSE);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	device->SetTexture(0, m_pTexture);
	device->SetFVF(TLVERTEX::FVF);
	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, m_vertices, sizeof(TLVERTEX));

	// Restore -> device matches ioRenderSystem's shadow cache again.
	device->SetTexture(0, pOldTex);
	if (pOldTex) pOldTex->Release();				// balance GetTexture AddRef
	device->SetTextureStageState(1, D3DTSS_ALPHAOP, dwAlphaOp1);
	device->SetTextureStageState(1, D3DTSS_COLOROP, dwColorOp1);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG2, dwAlphaArg2_0);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, dwAlphaArg1_0);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, dwAlphaOp0);
	device->SetTextureStageState(0, D3DTSS_COLORARG2, dwColorArg2_0);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, dwColorArg1_0);
	device->SetTextureStageState(0, D3DTSS_COLOROP, dwColorOp0);
	device->SetRenderState(D3DRS_FOGENABLE, dwFog);
	device->SetRenderState(D3DRS_ZWRITEENABLE, dwZWrite);
	device->SetRenderState(D3DRS_ZENABLE, dwZEnable);
	device->SetRenderState(D3DRS_LIGHTING, dwLighting);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, dwAlphaTest);
	device->SetRenderState(D3DRS_DESTBLEND, dwDestBlend);
	device->SetRenderState(D3DRS_SRCBLEND, dwSrcBlend);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, dwBlend);
	device->SetRenderState(D3DRS_CULLMODE, dwCull);
	device->SetFVF(dwFVF);
}

//----------------------------------------------------------------------------------
/*
Device lost / reset
*/
void VideoPlayer::OnLostDevice()
{
	// Dynamic D3DPOOL_DEFAULT textures are invalidated on device lost.
	ReleaseTexture();
	m_bTextureLost = true;
}

void VideoPlayer::OnResetDevice( IDirect3DDevice9* device )
{
	// Texture will be lazily recreated on next Update() once we know video size.
	(void)device;
	m_bTextureLost = true;
}


//----------------------------------------------------------------------------------
/*
Device lost / reset - broadcast to all live VideoPlayer instances.
Called from ioRenderSystem::Reset3DEnvironment.
*/
void ioVideoPlayer_OnLostDeviceAll()
{
	for ( std::set< VideoPlayer* >::iterator it = g_liveVideoPlayers.begin();
		  it != g_liveVideoPlayers.end(); ++it )
	{
		( *it )->OnLostDevice();
	}
}

void ioVideoPlayer_OnResetDeviceAll( IDirect3DDevice9* device )
{
	for ( std::set< VideoPlayer* >::iterator it = g_liveVideoPlayers.begin();
		  it != g_liveVideoPlayers.end(); ++it )
	{
		( *it )->OnResetDevice( device );
	}
}


//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
/*
ioVideoPlayer - ioWndEX wrapper
*/
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------

ioVideoPlayer::ioVideoPlayer()
	: ioWndEX( IWEXT_VIDEOPLAYER )
	, m_color( 0xFFFFFFFF )
	, m_ismouseover( false )
{
	m_videoPlayer = new VideoPlayer();
	m_videoPlayer->Init( RenderSystem().GetDevice(), GetWidth(), GetHeight() );
}

ioVideoPlayer::ioVideoPlayer( IOWNDEX_TYPE type )
	: ioWndEX( type )
	, m_color( 0xFFFFFFFF )
	, m_ismouseover( false )
{
	m_videoPlayer = new VideoPlayer();
	m_videoPlayer->Init( RenderSystem().GetDevice(), GetWidth(), GetHeight() );
}

ioVideoPlayer::~ioVideoPlayer()
{
	if ( m_videoPlayer )
	{
		m_videoPlayer->Destroy();
	}
	SAFEDELETE( m_videoPlayer );
}

//----------------------------------------------------------------------------------
/*
ioWnd override
*/
void ioVideoPlayer::SetSize( int width, int height )
{
	ioWndEX::SetSize( width, height );

	if ( m_videoPlayer )
	{
		m_videoPlayer->Resize( RenderSystem().GetDevice(), width, height );
	}
}

//----------------------------------------------------------------------------------
/*
ioWnd Event override - input not routed to video; pass-through only.
*/
void ioVideoPlayer::iwm_lbuttondown( const ioMouse& mouse ) { ioWndEX::iwm_lbuttondown( mouse ); }
void ioVideoPlayer::iwm_lbuttonup( const ioMouse& mouse )   { ioWndEX::iwm_lbuttonup( mouse ); }
void ioVideoPlayer::iwm_rbuttondown( const ioMouse& mouse ) { ioWndEX::iwm_rbuttondown( mouse ); }
void ioVideoPlayer::iwm_rbuttonup( const ioMouse& mouse )   { ioWndEX::iwm_rbuttonup( mouse ); }
void ioVideoPlayer::iwm_mouseover( const ioMouse& mouse )   { ioWndEX::iwm_mouseover( mouse ); m_ismouseover = true; }
void ioVideoPlayer::iwm_mousemove( const ioMouse& mouse )   { ioWndEX::iwm_mousemove( mouse ); }
void ioVideoPlayer::iwm_mouseleave( const ioMouse& mouse )  { ioWndEX::iwm_mouseleave( mouse ); m_ismouseover = false; }

//----------------------------------------------------------------------------------
/*
ioWnd render interface
*/
void ioVideoPlayer::OnRender()
{
	__super::OnRender();

	if ( !m_videoPlayer )
		return;

	// Pull latest frame into texture for this frame.
	m_videoPlayer->Update( RenderSystem().GetDevice() );

	IDirect3DTexture9* pTex = m_videoPlayer->GetCurrentTexture();
	if ( !pTex )
		return;	// texture not yet created (first frames / device lost)

	g_UIRenderer.AddUIQuad(
		new ioIQVideoPlayer
			(
				D3DXVECTOR2( GetDerivedPosX(), GetDerivedPosY() ),
				D3DXVECTOR2( GetWidth(), GetHeight() ),
				pTex,
				m_color
			)
	);
}

void ioVideoPlayer::CheckFrameReSize( int iWidth, int iHeight, int iPrevWidth, int iPrevHeight )
{
	__super::CheckFrameReSize( iWidth, iHeight, iPrevWidth, iPrevHeight );

	if ( m_videoPlayer )
	{
		m_videoPlayer->Resize( RenderSystem().GetDevice(), iWidth, iHeight );
	}
}

//----------------------------------------------------------------------------------
/*
Tool / XML hooks
*/
void ioVideoPlayer::ParseExtraInfo( ioXMLElement& extxml )
{
	ioWndEX::_ParseExtraInfo( extxml );

	if ( !m_videoPlayer )
		return;

	m_color = ioStringConverter::ParseColor( extxml.GetStringAttribute( "Color" ) );

	m_videoPlayer->SetLoop( extxml.GetBoolAttribute( "Loop" ) );
	float fVol = extxml.GetFloatAttribute( "Volume" );
	if ( fVol <= 0.0f ) fVol = 1.0f;	// default full volume when attribute absent (returns 0)
	m_videoPlayer->SetVolume( fVol );
	m_videoPlayer->SetMute( extxml.GetBoolAttribute( "Mute" ) );

	int vW = extxml.GetIntAttribute( "VideoWidth" );
	int vH = extxml.GetIntAttribute( "VideoHeight" );
	if ( vW <= 0 ) vW = GetWidth();
	if ( vH <= 0 ) vH = GetHeight();
	m_videoPlayer->Resize( RenderSystem().GetDevice(), vW, vH );

	m_videoPlayer->Load( extxml.GetStringAttribute( "MP4" ) );
}

void ioVideoPlayer::Copy( ioWnd* wnd )
{
	ioWndEX::Copy( wnd );

	ioVideoPlayer* pOther = dynamic_cast< ioVideoPlayer* >( wnd );
	if ( !pOther )
		return;

	pOther->m_color = m_color;
	// Video players are not cloneable (MF session state); recreate fresh.
	if ( m_videoPlayer )
	{
		SAFEDELETE( pOther->m_videoPlayer );
		pOther->m_videoPlayer = new VideoPlayer();
		pOther->m_videoPlayer->Init( RenderSystem().GetDevice(),
			pOther->GetWidth(), pOther->GetHeight() );
		const char* szFile = m_videoPlayer->GetFileName();
		if ( szFile && szFile[0] != '\0' )
		{
			pOther->m_videoPlayer->SetLoop( m_videoPlayer->IsLoop() );
			pOther->m_videoPlayer->SetVolume( m_videoPlayer->GetVolume() );
			pOther->m_videoPlayer->SetMute( m_videoPlayer->IsMute() );
			pOther->m_videoPlayer->Load( szFile );
		}
	}
}

void ioVideoPlayer::_SetSaveData( ioXMLElement& extxml )
{
	ioWndEX::_SetSaveData( extxml );

	if ( m_videoPlayer )
	{
		extxml.SetStringAttribute( "Color", ioWnd::ConvertToColorString( m_color ).c_str() );
		extxml.SetBoolAttribute( "Loop", m_videoPlayer->IsLoop() );
		extxml.SetFloatAttribute( "Volume", m_videoPlayer->GetVolume() );
		extxml.SetBoolAttribute( "Mute", m_videoPlayer->IsMute() );
		extxml.SetIntAttribute( "VideoWidth", (int) m_videoPlayer->GetWidth() );
		extxml.SetIntAttribute( "VideoHeight", (int) m_videoPlayer->GetHeight() );
		extxml.SetStringAttribute( "MP4", m_videoPlayer->GetFileName() );
	}
}

//----------------------------------------------------------------------------------
/*
Get
*/
const char* ioVideoPlayer::GetMP4FileName() const
{
	if ( !m_videoPlayer ) return "";
	return m_videoPlayer->GetFileName();
}

bool ioVideoPlayer::IsPlaying() const
{
	if ( !m_videoPlayer ) return false;
	return m_videoPlayer->IsPlaying();
}

bool ioVideoPlayer::IsLoop() const
{
	if ( !m_videoPlayer ) return false;
	return m_videoPlayer->IsLoop();
}

bool ioVideoPlayer::IsMute() const
{
	if ( !m_videoPlayer ) return false;
	return m_videoPlayer->IsMute();
}

float ioVideoPlayer::GetVolume() const
{
	if ( !m_videoPlayer ) return 0.0f;
	return m_videoPlayer->GetVolume();
}

//----------------------------------------------------------------------------------
/*
Public API wrappers (for LSClient / external callers)
*/
bool ioVideoPlayer::Load( const char* filename )
{
	if ( !m_videoPlayer ) return false;
	return m_videoPlayer->Load( filename );
}

void ioVideoPlayer::Play()
{
	if ( m_videoPlayer ) m_videoPlayer->Play();
}

void ioVideoPlayer::Stop()
{
	if ( m_videoPlayer ) m_videoPlayer->Stop();
}

void ioVideoPlayer::Pause()
{
	if ( m_videoPlayer ) m_videoPlayer->Pause();
}

void ioVideoPlayer::Resume()
{
	if ( m_videoPlayer ) m_videoPlayer->Resume();
}

void ioVideoPlayer::SetLoop( bool bLoop )
{
	if ( m_videoPlayer ) m_videoPlayer->SetLoop( bLoop );
}

void ioVideoPlayer::SetVolume( float fVolume )
{
	if ( m_videoPlayer ) m_videoPlayer->SetVolume( fVolume );
}

void ioVideoPlayer::SetMute( bool bMute )
{
	if ( m_videoPlayer ) m_videoPlayer->SetMute( bMute );
}
