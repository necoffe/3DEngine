#include "stdafx.h"
#include ".\iolocalmanagerparent.h"
#include "HelpFunc.h"

ioLocalManagerParent::LocalType ioLocalManagerParent::m_eLocalType = ioLocalManagerParent::LCT_KOREA;

ioLocalManagerParent::ioLocalManagerParent(void)
{
}

ioLocalManagerParent::~ioLocalManagerParent(void)
{
}

DWORD ioLocalManagerParent::GetNativeIMEConvMode( LocalType eLocalType )
{
	if( eLocalType == LCT_US ||
		eLocalType == LCT_INDONESIA )
		return IME_CMODE_ALPHANUMERIC;
	else
		return IME_CMODE_NATIVE;
}

void ioLocalManagerParent::GetPacPassword( OUT char *szPassword, IN const int iPasswordSize, IN const int iPasswordType, IN LocalType eLocalType )
{
	if (eLocalType == LCT_KOREA)
	{
		if (iPasswordType == 0)
		{
			char szEncPassWord[MAX_PASSWORD] = { -79,111,126,91,111,24,38,-23,48,86,50,98,119,-109,92,106,21,120,120,94 };
			EncryptDecryptData(szPassword, iPasswordSize, szEncPassWord, MAX_PASSWORD, true);
		}
		else if (iPasswordType == 1)
		{
			char szEncPassWord[MAX_PASSWORD] = { -100,39,123,97,14,103,89,-105,119,31,57,31,126,-46,20,120,4,95,106,84 };
			EncryptDecryptData(szPassword, iPasswordSize, szEncPassWord, MAX_PASSWORD, true);
		}
	}
}

void ioLocalManagerParent::EncryptDecryptData( OUT char *szResultData, IN const int iResultSize, IN const char *szSourceData, IN const int iSourceSize, IN bool bPassword )
{
	enum { MAX_KEY_TYPE = 2,  MAX_KEY = 30, };
	BYTE byKey[MAX_KEY_TYPE][MAX_KEY] = { 255,1,2,9,89,32,123,39,34,211,222,244,100,129,23,1,4,3,29,30,1,4,5,7,8,233,89,1,98,67,// password
									   201,245,239,157,14,243,76,144,20,172,24,25,65,236,91,17,107,40,207,41,81,192,221,229,90,197,214,24,142,71,
	};
	int iKeyType = 0;
	if( !bPassword )
		iKeyType = 1;

	

	for(int i =0; i < iSourceSize; i++)
	{
		if( i >= iResultSize ) break;
		szResultData[i] = szSourceData[i] ^ byKey[iKeyType][i%MAX_KEY];
		szResultData[i] = szResultData[i] ^ byKey[iKeyType][(iSourceSize-i)%MAX_KEY];
	}
}

void ioLocalManagerParent::GetChangedFontScale( OUT float &rfFontScale )
{
	//if( ioLocalManagerParent::GetLocalType() == ioLocalManagerParent::LCT_US )
	//{
	//	rfFontScale -= 0.02;
	//	if( rfFontScale < 0.0f )
	//		rfFontScale = 0.0f;
	//}
}

bool ioLocalManagerParent::IsVPrintf()
{
	if( GetLocalType() == LCT_KOREA )
		return true;

	return false;
}

bool ioLocalManagerParent::IsIME()
{
	if( GetLocalType() == LCT_KOREA     || 
		GetLocalType() == LCT_US        || 
		GetLocalType() == LCT_INDONESIA || 
		GetLocalType() == LCT_THAILAND )
		return true;

	return false;
}