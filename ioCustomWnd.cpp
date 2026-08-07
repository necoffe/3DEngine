//----------------------------------------------------------------------------------
/*
Filename	: ioCustomWnd.cpp
Author		: megamegy
Date		: 2013.11.21
*/
//----------------------------------------------------------------------------------

#include "stdafx.h"
#include "ioCustomWnd.h"
#include "ioXMLElement.h"
#include "TinyXML/tinyxml2.h"

//----------------------------------------------------------------------------------
/*
»ý¼º / ¼Ò¸ê
*/
ioCustomWnd::ioCustomWnd()
	: ioWnd()
	, m_xml( 0 )
{
}

ioCustomWnd::~ioCustomWnd()
{
	SAFEDELETE( m_xml );
}

//----------------------------------------------------------------------------------
/*
ioWnd override
*/
void	ioCustomWnd::ParseXML( ioXMLElement& xml )
{
	SAFEDELETE( m_xml );

	m_xml = xml.Clone();
}

bool	ioCustomWnd::Save( ioXMLElement& xml )
{
	if ( m_xml )
	{		
		tinyxml2::XMLElement* elem = m_xml->GetElement();
		if ( elem )
		{
			tinyxml2::XMLNode* parent = xml.GetElement()->Parent();
			if ( parent ) 
			{
				parent->DeleteChild( xml.GetElement() );
				parent->InsertEndChild( elem->DeepClone( parent->GetDocument() ) );				
			}
			else 
			{
				xml.SetElement( elem->DeepClone( nullptr )->ToElement() );
			}
		}
	}
	return true;
}