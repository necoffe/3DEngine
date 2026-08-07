

#include "stdafx.h"

#include "ioXMLDocument.h"
#include "ioXMLElement.h"

#include "TinyXML/tinyxml2.h"
#include <strsafe.h>

ioXMLDocument::ioXMLDocument()
{
	m_pDoc = new tinyxml2::XMLDocument;
}

ioXMLDocument::~ioXMLDocument()
{
	SAFEDELETE( m_pDoc );
}

void ioXMLDocument::CreateDocument( const char *szRootName,
								    const char *szVersion,
									const char *szEncoding,
									const char *szStandAlone )
{
	m_pDoc->Clear();

	char szDecl[256];
	if( szStandAlone && szStandAlone[0] )
		StringCbPrintf( szDecl, sizeof(szDecl), "xml version=\"%s\" encoding=\"%s\" standalone=\"%s\"", szVersion, szEncoding, szStandAlone );
	else
		StringCbPrintf( szDecl, sizeof(szDecl), "xml version=\"%s\" encoding=\"%s\"", szVersion, szEncoding );
	m_pDoc->InsertEndChild( m_pDoc->NewDeclaration( szDecl ) );
	m_pDoc->InsertEndChild( m_pDoc->NewElement( szRootName ) );
}

bool ioXMLDocument::LoadFromFile( const char *szFileName )
{
	m_pDoc->Clear();

	if( m_pDoc->LoadFile( szFileName ) != tinyxml2::XML_SUCCESS )
	{
		LOG.PrintTimeAndLog( 0, "ioXMLDocumemt::LoadFromFile - %s (%s)",
								m_pDoc->ErrorStr(), szFileName );
		return false;
	}

	return true;
}

bool ioXMLDocument::LoadFromMemory( const char *pData )
{
	m_pDoc->Clear();

	if( m_pDoc->Parse( pData ) != tinyxml2::XML_SUCCESS )
	{
		if( m_pDoc->Error() )
		{
			LOG.PrintTimeAndLog( 0, "ioXMLDocument::LoadFromMemory - %s(%d)",
									m_pDoc->ErrorStr(), m_pDoc->ErrorLineNum() );
			return false;
		}
	}

	return true;
}

bool ioXMLDocument::SaveXML( const char *szFileName )
{
	if( m_pDoc->SaveFile( szFileName ) == tinyxml2::XML_SUCCESS )
		return true;

	return false;
}

ioXMLElement ioXMLDocument::GetRootElement()
{
	return ioXMLElement( m_pDoc->RootElement() );
}
