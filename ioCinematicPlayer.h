//----------------------------------------------------------------------------------
/*
Filename	: ioCinematicPlayer.h
Author		: NOVERA
Date		: 2026.07.18
Desc		: Fullscreen cinematic video layer. Driven by LSClient render loop:
			  when active, Render() draws a fullscreen video quad to the backbuffer
			  in place of (or before) the game stage. Audio + loop + volume handled
			  by the underlying VideoPlayer. IsFinished() lets the caller transition
			  to the next stage when the video ends or is stopped.
*/
//----------------------------------------------------------------------------------

#ifndef _IOCINEMATICPLAYER_H_
#define _IOCINEMATICPLAYER_H_

#include "ioVideoPlayer.h"

class __EX ioCinematicPlayer
{
public:
	ioCinematicPlayer();
	~ioCinematicPlayer();

	// iScreenWidth/iScreenHeight: backbuffer dimensions (quad covers the screen).
	bool						Init( IDirect3DDevice9* device, int iScreenWidth, int iScreenHeight );
	void						Destroy();

	bool						Load( const char* szFile );

	// Mark active so Render() draws; cleared by Stop() or when video ends.
	void						Play();
	void						Stop();
	void						Pause();
	void						Resume();

	void						SetLoop( bool bLoop );
	void						SetVolume( float fVol );
	void						SetMute( bool bMute );
	void						SetColor( DWORD color ) { if ( m_pVideo ) m_pVideo->SetColor( color ); }

	// Call once per frame: pulls latest frame + detects natural end.
	void						Update( IDirect3DDevice9* device );
	// Draws fullscreen quad to current render target (call when IsActive()).
	void						Render( IDirect3DDevice9* device );

	void						Resize( int iScreenWidth, int iScreenHeight );

	bool						IsActive() const			{ return m_bActive; }
	bool						HasFrame() const			{ return m_pVideo && m_pVideo->GetCurrentTexture() != NULL; }
	bool						IsPaused() const			{ return m_bPaused; }
	bool						IsFinished() const			{ return m_bFinished; }
	bool						IsLoop() const;
	bool						IsMute() const;
	float						GetVolume() const;
	const char*					GetFileName() const;

private:
	VideoPlayer*				m_pVideo;
	int							m_iScreenW;
	int							m_iScreenH;
	bool						m_bActive;		// Render() should draw
	bool						m_bPaused;		// distinguishes pause from natural end
	bool						m_bFinished;	// video ended (or Stop called)
};

#endif // _IOCINEMATICPLAYER_H_
