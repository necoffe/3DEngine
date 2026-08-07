//----------------------------------------------------------------------------------
/*
Filename	: ioVideoPlayer.h
Author		: NOVERA
Date		: 2026.07.18
Desc		: '.mp4' file player via Media Foundation (IMFMediaSession + EVR).
			  Video: IMFVideoDisplayControl::GetCurrentImage -> D3D9 dynamic texture.
			  Audio: EVR routes to default audio endpoint; volume via IMFSimpleAudioVolume.
			  Three render modes (UI / cinematic / in-world) share the same VideoPlayer.
*/
//----------------------------------------------------------------------------------

#ifndef _IOVIDEOPLAYER_H_
#define _IOVIDEOPLAYER_H_

//----------------------------------------------------------------------------------
#include "ioWndEX.h"

// StdAfx.h defines _WIN32_WINNT = 0x500 (Win2000). Media Foundation requires
// _WIN32_WINNT >= 0x0601 (Win7). Bump locally for this TU; MF APIs are loaded
// dynamically from mfplat.dll so runtime is unaffected on Win7+ targets.
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#ifndef WINVER
#define WINVER 0x0601
#endif

#include <mfapi.h>
#include <mfidl.h>
#include <evr.h>
#include <mferror.h>
#include <strsafe.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "Strmiids.lib")  // CLSID_EnhancedVideoRenderer, SAR

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
/*
VideoPlayer - Media Foundation glue (mirrors FlashPlayer structure)
*/
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
class __EX VideoPlayer
{
public:
	enum EState
	{
		STATE_IDLE = 0,
		STATE_PLAYING,
		STATE_PAUSED,
		STATE_STOPPED
	};

public:
	VideoPlayer();
	~VideoPlayer();

	bool						Init( IDirect3DDevice9* device, unsigned int width, unsigned int height );
	void						Destroy();

	bool						Load( const char* filename );
	bool						Reload();

	void						Play();
	void						Stop();
	void						Pause();
	void						Resume();

	void						SetLoop( bool bLoop );
	void						SetVolume( float fVolume );		// 0.0f ~ 1.0f
	void						SetMute( bool bMute );
	void						SetColor( DWORD color ) { m_color = color; }	// ARGB tint for quad vertices (fade-in)
	void						Resize( IDirect3DDevice9* device, unsigned int width, unsigned int height );

	// Cap how often GetCurrentImage + texture upload runs, in frames-per-second.
	// 0 = pull every render frame (no throttle). Default 30 (33ms) - typical MP4 fps.
	void						SetMaxUpdateFps( DWORD fps );

	// Pull latest frame from EVR -> dynamic texture. Call once per frame.
	void						Update( IDirect3DDevice9* device );
	// Draw quad using current dynamic texture.
	void						Render( IDirect3DDevice9* device );
	// Expose texture for in-world mesh binding (ioVideoTexture adapter).
	IDirect3DTexture9*			GetCurrentTexture() const { return m_pTexture; }

	const char*					GetFileName() const			{ return m_strFilename.c_str(); }
	bool						IsPlaying() const			{ return m_eState == STATE_PLAYING; }
	bool						IsLoop() const				{ return m_bLoop; }
	bool						IsMute() const				{ return m_bMute; }
	float						GetVolume() const			{ return m_fVolume; }
	unsigned int				GetWidth() const			{ return m_uWidth; }
	unsigned int				GetHeight() const			{ return m_uHeight; }

	// Device lost / reset hooks (lazy-recreate dynamic texture).
	void						OnLostDevice();
	void						OnResetDevice( IDirect3DDevice9* device );

protected:
	bool						CreateTexture( IDirect3DDevice9* device, unsigned int videoW, unsigned int videoH );
	void						ReleaseTexture();
	bool						CreateSourceFromURL_( const wchar_t* wszFileURL, IMFMediaSource** ppSource );
	bool						CreateSourceFromPack_( const char* szInPackPath, IMFMediaSource** ppSource );
	bool						BuildTopology( IMFMediaSource* pSource );
	void						DestroySession();
	void						PollSessionEvents();
	void						ApplyVolumeAndMute();

	static void					EnsureMFStartup();
	static void					EnsureMFShutdown();

protected:
	// MF pipeline
	IMFMediaSession*			m_pSession;
	IMFVideoDisplayControl*		m_pDisplay;
	IMFSimpleAudioVolume*		m_pAudioVolume;
	IMFByteStream*				m_pByteStream;		// in-memory byte stream (pack mode); NULL in disk mode. Holds IStream/HGLOBAL alive across session lifetime.
	HWND						m_hClipWnd;		// message-only window for windowless EVR

	// D3D9 resources. Texture is created at VIDEO NATIVE size (lazy on first frame).
	// Render quad uses m_uWidth/m_uHeight (draw size); GPU scales via UV (0,0,1,1).
	IDirect3DTexture9*			m_pTexture;		// D3DPOOL_DEFAULT, D3DUSAGE_DYNAMIC, A8R8G8B8
	bool						m_bTextureLost;	// device-lost flag for lazy recreate

	// State
	unsigned int				m_uWidth;		// draw (quad) width
	unsigned int				m_uHeight;		// draw (quad) height
	unsigned int				m_uVideoW;		// video native width (= texture width)
	unsigned int				m_uVideoH;		// video native height (= texture height)
	bool						m_bLoop;
	float						m_fVolume;
	bool						m_bMute;
	DWORD						m_color;		// ARGB tint applied to quad vertices (fade-in)
	EState						m_eState;
	std::string					m_strFilename;

	// Frame pull throttle (perf: avoid GetCurrentImage + texture upload when source
	// frame hasn't advanced since last pull). m_uMinUpdateIntervalMs caps pull rate
	// (default 33ms = 30fps). m_llLastFrameStamp dedups duplicate EVR frames.
	LONGLONG					m_llLastFrameStamp;
	DWORD						m_dwLastUpdateMs;
	DWORD						m_uMinUpdateIntervalMs;

	struct TLVERTEX
	{
		enum { FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1, };

		float		m_x;
		float		m_y;
		float		m_z;
		float		m_rhw;
		D3DCOLOR	m_color;
		float		m_u;
		float		m_v;

		void		Set( float x, float y, float z, float rhw, D3DCOLOR color, float u, float v )
		{
			m_x = x;
			m_y = y;
			m_z = z;
			m_rhw = rhw;
			m_color = color;
			m_u = u;
			m_v = v;
		}
	};
	TLVERTEX					m_vertices[ 4 ];
};


//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
/*
ioVideoPlayer - ioWndEX wrapper (mirrors ioFlashPlayer)
*/
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------

// Device-lost hooks called from ioRenderSystem::Reset3DEnvironment.
// Declared here as free functions so ioRenderSystem.cpp can forward-declare them
// without pulling Media Foundation headers into its translation unit.
void						ioVideoPlayer_OnLostDeviceAll();
void						ioVideoPlayer_OnResetDeviceAll( IDirect3DDevice9* device );

class __EX ioVideoPlayer : public ioWndEX
{
public:
	ioVideoPlayer();
	ioVideoPlayer( IOWNDEX_TYPE type );
	virtual ~ioVideoPlayer();

public:
	// Core API
	bool						Load( const char* filename );
	void						Play();
	void						Stop();
	void						Pause();
	void						Resume();
	void						SetLoop( bool bLoop );
	void						SetVolume( float fVolume );
	void						SetMute( bool bMute );

	// Color / alpha tint
	void						SetFrontColor( DWORD color ) { m_color = color; }
	DWORD						GetFrontColor() const { return m_color; }

	// ioWnd override
	virtual void				SetSize( int width, int height );

	// ioWnd Event override (input not routed to video; left as no-op pass-through).
	virtual void				iwm_lbuttondown( const ioMouse& mouse );
	virtual void				iwm_lbuttonup( const ioMouse& mouse );
	virtual void				iwm_rbuttondown( const ioMouse& mouse );
	virtual void				iwm_rbuttonup( const ioMouse& mouse );
	virtual void				iwm_mouseover( const ioMouse& mouse );
	virtual void				iwm_mousemove( const ioMouse& mouse );
	virtual void				iwm_mouseleave( const ioMouse& mouse );

	// Tool / XML hooks
	virtual void				ParseExtraInfo( ioXMLElement &extxml );
	virtual void				Copy( ioWnd* wnd );
	virtual void				_SetSaveData( ioXMLElement& extxml );

	// Get
	const char*					GetMP4FileName() const;
	bool						IsPlaying() const;
	bool						IsLoop() const;
	bool						IsMute() const;
	float						GetVolume() const;

	// Tool naming interface
	DECL_NAME_INTERFACE_FOR_TOOL( VideoPlayer, ioVideoPlayer )
	virtual ioWnd*				_Clone() { return new ioVideoPlayer(); }

protected:
	// ioWnd render interface
	virtual void				OnRender();
	virtual void				CheckFrameReSize( int iWidth, int iHeight, int iPrevWidth, int iPrevHeight );

protected:
	VideoPlayer*				m_videoPlayer;
	DWORD						m_color;
	bool						m_ismouseover;
};

#endif // _IOVIDEOPLAYER_H_
