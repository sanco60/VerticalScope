#include "stdafx.h"
#include "plugin.h"

#include "MemLeaker.h"
#include <string>
#include <sstream>
#include <fstream>
#include <io.h>


BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}

PDATAIOFUNC	 g_pFuncCallBack;

//��ȡ�ص�����
void RegisterDataInterface(PDATAIOFUNC pfn)
{
	g_pFuncCallBack = pfn;
}

//ע������Ϣ
void GetCopyRightInfo(LPPLUGIN info)
{
	//��д������Ϣ
	strcpy_s(info->Name,"�����Ϳռ�");
	strcpy_s(info->Dy,"US");
	strcpy_s(info->Author,"john");
	strcpy_s(info->Period,"����");
	strcpy_s(info->Descript,"�����Ϳռ�");
	strcpy_s(info->OtherInfo,"�����Ϳռ�");

	//��д������Ϣ
	info->ParamNum = 3;//��ʾ3������

	strcpy_s(info->ParamInfo[0].acParaName,"�ٷֱ�");
	info->ParamInfo[0].nMin=0;
	info->ParamInfo[0].nMax=1000;
	info->ParamInfo[0].nDefault=20;

	strcpy_s(info->ParamInfo[1].acParaName,"��ˮλ");
	info->ParamInfo[1].nMin=0;
	info->ParamInfo[1].nMax=1;
	info->ParamInfo[1].nDefault=1;

	strcpy_s(info->ParamInfo[2].acParaName,"����");
	info->ParamInfo[2].nMin=0;
	info->ParamInfo[2].nMax=1;
	info->ParamInfo[2].nDefault=1;
}

////////////////////////////////////////////////////////////////////////////////
//�Զ���ʵ��ϸ�ں���(�ɸ���ѡ����Ҫ���)

const	BYTE	g_nAvoidMask[]={0xF8,0xF8,0xF8,0xF8};	// ��Ч���ݱ�־(ϵͳ����)

char* g_nFatherCode[] = { "999999", "399001", "399005", "399006" };

int g_FatherUpPercent[] = {-1, -1, -1, -1};

const int cIgnoreStocksMaxCount = 5000;
char g_IgnoreStocks[cIgnoreStocksMaxCount][7];

bool g_bInitial = false;

const char g_UserDir[] = {".\\UserData\\"};

const char g_IgnoreKeyword[] = {"IGS_VerticalScope*.EBK"};


typedef enum _eFatherCode
{
	EShangHaiZZ,
	EShenZhenCZ,
	EZhongXBZZ,
	EChuangYBZZ,
	EFatherCodeMax
} EFatherCode;

EFatherCode mathFatherCode(char* Code)
{
	if (NULL == Code)
		return EFatherCodeMax;

	EFatherCode eFCode = EFatherCodeMax;
	if (Code[0] == '6')
	{
		eFCode = EShangHaiZZ;
	}
	else if (Code[0] == '3')
	{
		eFCode = EChuangYBZZ;
	}
	else if (strstr(Code, "002") == Code)
	{
		eFCode = EZhongXBZZ;
	}
	else if (Code[0] == '0')
	{
		eFCode = EShenZhenCZ;
	}

	return eFCode;
}

//����������̼�
LPHISDAT maxClose(LPHISDAT pHisDat, long lDataNum)
{
	if (NULL == pHisDat || lDataNum <= 0)
		return NULL;

	LPHISDAT pMax = pHisDat;
	for (long i = 0; i < lDataNum; i++)
	{
		if (pMax->Close < (pHisDat+i)->Close)
			pMax = pHisDat+i;
	}
	return pMax;
}

//������С���̼�
LPHISDAT minClose(LPHISDAT pHisDat, long lDataNum)
{
	if (NULL == pHisDat || lDataNum <= 0)
		return NULL;

	LPHISDAT pMin = pHisDat;
	for (long i = 0; i < lDataNum; i++)
	{
		if (pMin->Close > (pHisDat+i)->Close)
			pMin = pHisDat+i;
	}
	return pMin;
}

BOOL fEqual(double a, double b)
{
	const double cJudge = 0.001;
	double fValue = 0.0;

	fValue = (a > b) ? a - b : b - a;

	if (fValue > cJudge)
		return FALSE;

	return TRUE;
}


BOOL dateEqual(NTime t1, NTime t2)
{
	if (t1.year != t2.year || t1.month != t2.month || t1.day != t2.day)
		return FALSE;

	return TRUE;
}

//�����ߴ����ұ߷������� ���С���ұ߷��ظ��� ��ȷ���0 
int dateCompare(NTime left, NTime right)
{
	const int cLeftBig = 1;
	const int cRightBig = -1;
	const int cSame = 0;

	if (left.year > right.year)
		return cLeftBig;
	else if (left.year < right.year)
		return cRightBig;

	if (left.month > right.month)
		return cLeftBig;
	else if (left.month < right.month)
		return cRightBig;

	if (left.day > right.day)
		return cLeftBig;
	else if (left.day < right.day)
		return cRightBig;

	return cSame;
}


NTime dateInterval(NTime nLeft, NTime nRight)
{
	NTime nInterval;
	memset(&nInterval, 0, sizeof(NTime));
	
	unsigned int iLeft = 0;
	unsigned int iRight = 0;
	unsigned int iInterval = 0;

	const unsigned int cDayofyear = 365;
	const unsigned int cDayofmonth = 30;

	//֤ȯ����������������ǰ��10��
	const unsigned int cBaseYear = 1980;

	if (nLeft.year < cBaseYear || nRight.year < cBaseYear)
		return nInterval;
	
	iLeft = (nLeft.year - cBaseYear) * cDayofyear + (nLeft.month - 1) * cDayofmonth + nLeft.day;
	iRight = (nRight.year - cBaseYear) * cDayofyear + (nRight.month - 1) * cDayofmonth + nRight.day;

	iInterval = (iLeft > iRight) ? iLeft - iRight : iRight - iLeft;

	nInterval.year = iInterval / cDayofyear;
	iInterval = iInterval % cDayofyear;
	nInterval.month = (iInterval / cDayofmonth) + 1;
	iInterval = iInterval % cDayofmonth;
	nInterval.day = iInterval;

	return nInterval;
}


/* ���˺��� ���� ��S��*��ͷ������һ��ģ�ͣ�ƵĹ�Ʊ
   ����ֵ�� ������������FALSE��ʾ��ͨ��
*/
BOOL filterStock(char * Code, short nSetCode, NTime time1, NTime time2, BYTE nTQ)
{
	if (NULL == Code)
		return FALSE;

	{
		//�û�ָ���Ĺ�Ʊ��Ҫ����
		for (int iRow = 0; iRow < cIgnoreStocksMaxCount; iRow++)
		{
			if (0 == strlen(g_IgnoreStocks[iRow]))
				break;

			if (0 == strcmp(g_IgnoreStocks[iRow], Code))
			{
				OutputDebugStringA("User abandon ");
				return FALSE;
			}
		}
	}

	const unsigned short cMinYears = 2;	
	const short cInfoNum = 2;
	short iInfoNum = cInfoNum;

	{
		STOCKINFO stockInfoArray[cInfoNum];
		memset(stockInfoArray, 0, cInfoNum*sizeof(STOCKINFO));

		LPSTOCKINFO pStockInfo = stockInfoArray;

		//��ȡ��Ʊ�����Լ�����ʱ��
		long readnum = g_pFuncCallBack(Code, nSetCode, STKINFO_DAT, pStockInfo, iInfoNum, time1, time2, nTQ, 0);
		if (readnum <= 0)
		{
			//delete[] pStockInfo;
			pStockInfo = NULL;
			return FALSE;
		}
		if ('S' == pStockInfo->Name[0] || '*' == pStockInfo->Name[0])
		{
			//delete[] pStockInfo;
			pStockInfo = NULL;
			return FALSE;
		}

		NTime startDate, todayDate, dInterval;
		memset(&startDate, 0, sizeof(NTime));
		memset(&todayDate, 0, sizeof(NTime));
		memset(&dInterval, 0, sizeof(NTime));

		long lStartDate = pStockInfo->J_start;
		startDate.year = (short)(lStartDate / 10000);
		lStartDate = lStartDate % 10000;
		startDate.month = (unsigned char)(lStartDate / 100);
		lStartDate = lStartDate % 100;
		startDate.day = (unsigned char)lStartDate;

		//��ȡ��������
		SYSTEMTIME tdTime;
		memset(&tdTime, 0, sizeof(SYSTEMTIME));
		GetLocalTime(&tdTime);

		todayDate.year = tdTime.wYear;
		todayDate.month = (unsigned char)tdTime.wMonth;
		todayDate.day = (unsigned char)tdTime.wDay;

		dInterval = dateInterval(startDate, todayDate);

		//̫����Ĺɷ���FALSE
		if (dInterval.year < cMinYears)
		{
			OutputDebugStringA("It's too young.");
			pStockInfo = NULL;
			return FALSE;
		}

		//delete[] pStockInfo;
		pStockInfo = NULL;
	}
	/*
	{
		//��ȡ��Ʊ�������Ϣ������ͣ�Ƶķ���FALSE
		REPORTDAT2 reportArray[cInfoNum];
		memset(reportArray, 0, cInfoNum*sizeof(REPORTDAT2));

		LPREPORTDAT2 pReportDat2 = reportArray;
		
		//��ȡ��Ʊ���쿪����Ϣ
		long readnum = g_pFuncCallBack(Code, nSetCode, REPORT_DAT2, pReportDat2, iInfoNum, time1, time2, nTQ, 0);
		if (0 >= readnum)
		{
			pReportDat2 = NULL;
			return FALSE;
		}

		if ( fEqual(pReportDat2->Open, 0) )
		{
			pReportDat2 = NULL;
			return FALSE;
		}
		pReportDat2 = NULL;
	}*/	

	return TRUE;
}


/* ���������������̼ۿռ� */
int calcMax2MinPercent(char * Code, short nSetCode, short DataType, NTime time1, NTime time2, BYTE nTQ, int bUp)
{
	int iMax2Min = -1;

	LPHISDAT pMax = NULL;
	LPHISDAT pMin = NULL;

	//�������ݸ���
	long datanum = g_pFuncCallBack(Code, nSetCode, DataType, NULL, -1, time1, time2, nTQ, 0);
	if ( 1 > datanum ){
		return iMax2Min;
	}

	LPHISDAT pHisDat = new HISDAT[datanum];

	long readnum = g_pFuncCallBack(Code, nSetCode, DataType, pHisDat, (short)datanum, time1, time2, nTQ, 0);
	if ( 1 > readnum || readnum > datanum )
	{
		OutputDebugStringA("========= g_pFuncCallBack read error! =========\n");
		delete[] pHisDat;
		pHisDat = NULL;
		return iMax2Min;
	}

	//����������̼�
	pMax = maxClose(pHisDat, readnum);
	pMin = minClose(pHisDat, readnum);

	int iComp = dateCompare(pMax->Time, pMin->Time);
	if ((0 < iComp && 0 == bUp)
		|| (0 > iComp && 1 == bUp))
	{
		delete[] pHisDat;
		pHisDat = NULL;
		return iMax2Min;
	}	

	if (NULL == pMax || NULL == pMin)
	{
		OutputDebugStringA("========= max or min error! =========\n");
		delete[] pHisDat;
		pHisDat=NULL;
		return iMax2Min;
	}

	/*����ռ�ٷֱ�*/
	iMax2Min = int(((pMax->Close - pMin->Close)/pMin->Close) * 100);

	delete[] pHisDat;
	pHisDat=NULL;

	return iMax2Min;
}


void restoreIgnoreStocks()
{
	long lFind = 0;
	struct _finddata_t fInfo;
	std::string szFind = g_UserDir;
	szFind.append(g_IgnoreKeyword);

	memset(g_IgnoreStocks, 0, sizeof(g_IgnoreStocks));

	lFind = _findfirst(szFind.c_str(), &fInfo);
	if (-1 == lFind)
		return;

	int iRow = 0;
	do{
		std::string szFilePath = g_UserDir;
		szFilePath.append(fInfo.name);
		
		std::ifstream ifs;
		ifs.open(szFilePath);
		if (!ifs.is_open())
			continue;

		while(!ifs.eof())
		{
			char szLine[64] = {0};
			ifs.getline(szLine, 63);
			std::string dataLine(szLine+1);
			if (6 == dataLine.length())
			{
				if (iRow >= cIgnoreStocksMaxCount)
					break;
				strcpy_s(g_IgnoreStocks[iRow++], dataLine.c_str());
			}
		}
		ifs.close();
		
	} while (0 == _findnext(lFind, &fInfo));
	_findclose(lFind);

	return;
}


bool init()
{
	if (g_bInitial)
		return false;

	restoreIgnoreStocks();

	g_bInitial = true;

	return true;
}


BOOL InputInfoThenCalc1(char * Code,short nSetCode,int Value[4],short DataType,short nDataNum,BYTE nTQ,unsigned long unused) //��������ݼ���
{
	BOOL nRet = FALSE;
	return nRet;
}

BOOL InputInfoThenCalc2(char * Code,short nSetCode,int Value[4],short DataType,NTime time1,NTime time2,BYTE nTQ,unsigned long unused)  //ѡȡ����
{
	BOOL nRet = FALSE;

	if ( (Value[0] < 0 || Value[0] > 1000) 
		|| (Value[1] != 0 && Value[1] != 1) 
		|| (Value[2] != 0 && Value[2] != 1) 
		|| NULL == Code )
	{
		OutputDebugStringA("Parameters Error!\n");
		goto endCalc2;
	}

	if (!g_bInitial)
		init();

	int iSonRate = 0;

	/* ���������ɺ�ͣ�ƹ� */
	if (FALSE == filterStock(Code, nSetCode, time1, time2, nTQ))
	{
		OutputDebugStringA("===== filter stock : ");
		OutputDebugStringA(Code);
		OutputDebugStringA(" =========\n");
		goto endCalc2;
	}

	/* ������ɴ�ֱ�ռ�ٷֱ� */
	iSonRate = calcMax2MinPercent(Code, nSetCode, DataType, time1, time2, nTQ, Value[2]);
	if (iSonRate < 0)
	{
		OutputDebugStringA("=========== son calcMax2MinPercent error!!\n");
		goto endCalc2;
	}
	
	if (0 == Value[1])
	{
		if (iSonRate <= Value[0])
			nRet = TRUE;
	} else
	{
		if (iSonRate >= Value[0])
			nRet = TRUE;
	}
	
endCalc2:
	MEMLEAK_OUTPUT();
	return nRet;
}
