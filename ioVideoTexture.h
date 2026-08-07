//----------------------------------------------------------------------------------
/*
Filename	: ioVideoTexture.h
Author		: NOVERA
Date		: 2026.07.18
Desc		: In-world mesh video texture adapter. Wraps a VideoPlayer and exposes
			  its frames through an ioTexture registered in ioTextureManager by name,
			  so a mesh material can reference the video by texture name.
			  Each frame the video frame is StretchRect-copied from the VideoPlayer's
			  dynamic texture into the ioTexture's render-target texture (clean
			  ownership; device-lost handled by manager + VideoPlayer).
*/
//----------------------------------------------------------------------------------

#ifndef _IOVIDEOTEXTURE_H_
#define _IOVIDEOTEXTURE_H_

#include "ioVideoPlayer.h"
#include "ioTexture.h"

class __EX ioVideoTexture
{
public:
	ioVideoTexture();
	~ioVideoTexture();

	// szTextureName: unique name registered in ioTextureManager. A mesh material
	// references this name to bind the video. iTexW/iTexH: size of the texture the
	// mesh expects (video is scaled to fit via StretchRect).
	bool						Init( IDirect3DDevice9* device, const char* szTextureName, int iTexW, int iTexH );
	void						Destroy();

	bool						Load( const char* szFile );

	// Call once per frame: pulls video frame, copies into ioTexture.
	void						Update( IDirect3DDevice9* device );

	// ioTexture* for material binding (mesh references szTextureName).
	ioTexture*					GetTexture() const			{ return m_pTexture; }
	const char*					GetTextureName() const		{ return m_strName.c_str(); }

	// Video controls (pass-through to VideoPlayer).
	void						Play();
	void						Stop();
	void						Pause();
	void						Resume();
	void						SetLoop( bool bLoop );
	void						SetVolume( float fVol );
	void						SetMute( bool bMute );

	bool						IsPlaying() const;
	bool						IsLoop() const;
	bool						IsMute() const;
	float						GetVolume() const;
	const char*					GetFileName() const;

private:
	VideoPlayer*				m_pVideo;
	ioTexture*					m_pTexture;		// refcounted; released on Destroy
	std::string					m_strName;
	int							m_iTexW;
	int							m_iTexH;
};

#endif // _IOVIDEOTEXTURE_H_
