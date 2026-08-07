//----------------------------------------------------------------------------------
/*
Filename	: ioCinematicPlayer.cpp
Author		: NOVERA
Date		: 2026.07.18
Desc		: Fullscreen cinematic video layer.
*/
//----------------------------------------------------------------------------------

#include "stdafx.h"
#include "ioCinematicPlayer.h"
#include "ioRenderSystem.h"

//----------------------------------------------------------------------------------
/*
Construction / Destruction
*/
ioCinematicPlayer::ioCinematicPlayer()
	: m_pVideo( NULL )
	, m_iScreenW( 0 )
	, m_iScreenH( 0 )
	, m_bActive( false )
	, m_bPaused( false )
	, m_bFinished( false )
{
}

ioCinematicPlayer::~ioCinematicPlayer()
{
	Destroy();
}

//----------------------------------------------------------------------------------
/*
Init / Destroy
*/
bool ioCinematicPlayer::Init( IDirect3DDevice9* device, int iScreenWidth, int iScreenHeight )
{
	Destroy();

	if ( !device || iScreenWidth <= 0 || iScreenHeight <= 0 )
		return false;

	m_iScreenW = iScreenWidth;
	m_iScreenH = iScreenHeight;

	m_pVideo = new VideoPlayer();
	if ( !m_pVideo->Init( device, iScreenWidth, iScreenHeight ) )
	{
		LOG.PrintTimeAndLog( 0, "ioCinematicPlayer::Init - VideoPlayer::Init Failed" );
		Destroy();
		return false;
	}
	return true;
}

void ioCinematicPlayer::Destroy()
{
	SAFEDELETE( m_pVideo );
	m_iScreenW = m_iScreenH = 0;
	m_bActive = false;
	m_bPaused = false;
	m_bFinished = false;
}

//----------------------------------------------------------------------------------
/*
Load / Controls
*/
bool ioCinematicPlayer::Load( const char* szFile )
{
	if ( !m_pVideo || !szFile )
		return false;
	return m_pVideo->Load( szFile );
}

void ioCinematicPlayer::Play()
{
	if ( !m_pVideo )
		return;
	m_pVideo->Play();
	m_bActive   = true;
	m_bPaused   = false;
	m_bFinished = false;
}

void ioCinematicPlayer::Stop()
{
	if ( m_pVideo ) m_pVideo->Stop();
	m_bActive   = false;
	m_bPaused   = false;
	m_bFinished = true;
}

void ioCinematicPlayer::Pause()
{
	if ( !m_pVideo )
		return;
	m_pVideo->Pause();
	m_bPaused = true;
}

void ioCinematicPlayer::Resume()
{
	if ( !m_pVideo )
		return;
	m_pVideo->Resume();
	m_bPaused = false;
}

void ioCinematicPlayer::SetLoop( bool bLoop )   { if ( m_pVideo ) m_pVideo->SetLoop( bLoop ); }
void ioCinematicPlayer::SetVolume( float fVol ) { if ( m_pVideo ) m_pVideo->SetVolume( fVol ); }
void ioCinematicPlayer::SetMute( bool bMute )   { if ( m_pVideo ) m_pVideo->SetMute( bMute ); }

//----------------------------------------------------------------------------------
/*
Update: pull frame + detect natural end.
Natural end = active, not paused, not looping, and VideoPlayer stopped (MESessionEnded
sets state to STOPPED). Stop() also sets m_bFinished explicitly.
*/
void ioCinematicPlayer::Update( IDirect3DDevice9* device )
{
	if ( !m_pVideo )
		return;

	m_pVideo->Update( device );

	if ( m_bActive && !m_bPaused && !m_pVideo->IsLoop() && !m_pVideo->IsPlaying() )
	{
		// Video ended naturally (or was never playing). Mark finished; keep last frame.
		m_bFinished = true;
		m_bActive   = false;
	}
}

//----------------------------------------------------------------------------------
/*
Render: fullscreen quad to current render target (backbuffer).
VideoPlayer::Render draws at (0,0)-(m_uWidth,m_uHeight) which equals the screen size
set in Init/Resize, so this is a fullscreen quad.
*/
void ioCinematicPlayer::Render( IDirect3DDevice9* device )
{
	if ( !m_bActive || !m_pVideo )
		return;
	m_pVideo->Render( device );
}

//----------------------------------------------------------------------------------
/*
Resize: update fullscreen quad dimensions.
*/
void ioCinematicPlayer::Resize( int iScreenWidth, int iScreenHeight )
{
	if ( m_pVideo && iScreenWidth > 0 && iScreenHeight > 0 )
	{
		m_pVideo->Resize( RenderSystem().GetDevice(), iScreenWidth, iScreenHeight );
		m_iScreenW = iScreenWidth;
		m_iScreenH = iScreenHeight;
	}
}

//----------------------------------------------------------------------------------
/*
Get
*/
bool ioCinematicPlayer::IsLoop() const    { return m_pVideo ? m_pVideo->IsLoop() : false; }
bool ioCinematicPlayer::IsMute() const    { return m_pVideo ? m_pVideo->IsMute() : false; }
float ioCinematicPlayer::GetVolume() const { return m_pVideo ? m_pVideo->GetVolume() : 0.0f; }
const char* ioCinematicPlayer::GetFileName() const { return m_pVideo ? m_pVideo->GetFileName() : ""; }
