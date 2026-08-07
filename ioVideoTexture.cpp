//----------------------------------------------------------------------------------
/*
Filename	: ioVideoTexture.cpp
Author		: NOVERA
Date		: 2026.07.18
Desc		: In-world mesh video texture adapter.
*/
//----------------------------------------------------------------------------------

#include "stdafx.h"
#include "ioVideoTexture.h"
#include "ioTextureManager.h"
#include "ioRenderSystem.h"

//----------------------------------------------------------------------------------
/*
Construction / Destruction
*/
ioVideoTexture::ioVideoTexture()
	: m_pVideo( NULL )
	, m_pTexture( NULL )
	, m_iTexW( 0 )
	, m_iTexH( 0 )
{
}

ioVideoTexture::~ioVideoTexture()
{
	Destroy();
}

//----------------------------------------------------------------------------------
/*
Init / Destroy
*/
bool ioVideoTexture::Init( IDirect3DDevice9* device, const char* szTextureName, int iTexW, int iTexH )
{
	Destroy();

	if ( !device || !szTextureName || iTexW <= 0 || iTexH <= 0 )
		return false;

	m_strName = szTextureName;
	m_iTexW   = iTexW;
	m_iTexH   = iTexH;

	// Create / refcount an ioTexture registered in the manager under szTextureName.
	// D3DUSAGE_RENDERTARGET + D3DPOOL_DEFAULT so StretchRect can target it. Manager
	// handles its device-lost lifecycle (ReleaseDefaultPoolTextures / ReCreate).
	m_pTexture = g_TextureMgr.CreateManual( szTextureName, iTexW, iTexH,
		D3DFMT_A8R8G8B8, D3DUSAGE_RENDERTARGET, 1 );
	if ( !m_pTexture )
	{
		LOG.PrintTimeAndLog( 0, "ioVideoTexture::Init - CreateManual Failed(%s)", szTextureName );
		return false;
	}

	m_pVideo = new VideoPlayer();
	if ( !m_pVideo->Init( device, iTexW, iTexH ) )
	{
		LOG.PrintTimeAndLog( 0, "ioVideoTexture::Init - VideoPlayer::Init Failed" );
		Destroy();
		return false;
	}

	return true;
}

void ioVideoTexture::Destroy()
{
	SAFEDELETE( m_pVideo );

	if ( m_pTexture )
	{
		// Release our refcount; manager destroys when refcount hits 0 (or on shutdown).
		m_pTexture->Release();
		m_pTexture = NULL;
	}

	m_strName.clear();
	m_iTexW = m_iTexH = 0;
}

//----------------------------------------------------------------------------------
/*
Load video file
*/
bool ioVideoTexture::Load( const char* szFile )
{
	if ( !m_pVideo || !szFile )
		return false;
	return m_pVideo->Load( szFile );
}

//----------------------------------------------------------------------------------
/*
Update: pull latest frame from EVR into VideoPlayer's dynamic texture, then
StretchRect-copy into the ioTexture's render-target texture (GPU scales).
*/
void ioVideoTexture::Update( IDirect3DDevice9* device )
{
	if ( !m_pVideo || !m_pTexture || !device )
		return;

	m_pVideo->Update( device );

	IDirect3DTexture9* pSrcTex = m_pVideo->GetCurrentTexture();
	IDirect3DTexture9* pDstTex = m_pTexture->GetTexture();
	if ( !pSrcTex || !pDstTex )
		return;

	IDirect3DSurface9* pSrcSurf = NULL;
	IDirect3DSurface9* pDstSurf = NULL;
	if ( FAILED( pSrcTex->GetSurfaceLevel( 0, &pSrcSurf ) ) || !pSrcSurf )
		return;
	if ( FAILED( pDstTex->GetSurfaceLevel( 0, &pDstSurf ) ) || !pDstSurf )
	{
		SAFERELEASE( pSrcSurf );
		return;
	}

	// StretchRect scales source video frame into the ioTexture's RT (linear filter).
	HRESULT hr = device->StretchRect( pSrcSurf, NULL, pDstSurf, NULL, D3DTEXF_LINEAR );
	if ( FAILED( hr ) )
	{
		LOG.PrintTimeAndLog( 0, "ioVideoTexture::Update - StretchRect Failed(%x)", hr );
	}

	SAFERELEASE( pSrcSurf );
	SAFERELEASE( pDstSurf );
}

//----------------------------------------------------------------------------------
/*
Video controls (pass-through)
*/
void ioVideoTexture::Play()       { if ( m_pVideo ) m_pVideo->Play(); }
void ioVideoTexture::Stop()       { if ( m_pVideo ) m_pVideo->Stop(); }
void ioVideoTexture::Pause()      { if ( m_pVideo ) m_pVideo->Pause(); }
void ioVideoTexture::Resume()     { if ( m_pVideo ) m_pVideo->Resume(); }
void ioVideoTexture::SetLoop( bool bLoop ) { if ( m_pVideo ) m_pVideo->SetLoop( bLoop ); }
void ioVideoTexture::SetVolume( float fVol ){ if ( m_pVideo ) m_pVideo->SetVolume( fVol ); }
void ioVideoTexture::SetMute( bool bMute ) { if ( m_pVideo ) m_pVideo->SetMute( bMute ); }

bool ioVideoTexture::IsPlaying() const { return m_pVideo ? m_pVideo->IsPlaying() : false; }
bool ioVideoTexture::IsLoop() const    { return m_pVideo ? m_pVideo->IsLoop() : false; }
bool ioVideoTexture::IsMute() const    { return m_pVideo ? m_pVideo->IsMute() : false; }
float ioVideoTexture::GetVolume() const { return m_pVideo ? m_pVideo->GetVolume() : 0.0f; }
const char* ioVideoTexture::GetFileName() const { return m_pVideo ? m_pVideo->GetFileName() : ""; }
