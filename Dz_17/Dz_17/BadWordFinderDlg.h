#pragma once
#include "header.h"

struct WordData
{
	std::wstring word; // Слово
	int count; // Количество повторений слова

	WordData(const std::wstring& w, int c) : word(w), count(c) {}
};

class CBadWordFinderDlg
{
public:
	CBadWordFinderDlg(void);
public:
	HANDLE hEventFinishProcessing; 
	~CBadWordFinderDlg(void);
	static BOOL CALLBACK DlgProc(HWND hWnd, UINT mes, WPARAM wp, LPARAM lp);
	static CBadWordFinderDlg* ptr;
	void Cls_OnClose(HWND hwnd);
	BOOL Cls_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam);
	void Cls_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
	HWND hDialog, hStart, hStop, hPause, hContinue, hEnteredValue, hStatus, hProgressBar, hUploadFile, hSaveTo, hOutputAllBadWords, hOutputTopTenBadWords,hExit;
	//////////////////////////////////////////////////////////////
	HANDLE hEventPause;
	HANDLE hEventStop;
	HANDLE hMutex;
	bool isUsingFileForBannedWords;
	std::vector<WordData> badWords; 
	std::vector<WordData> topTenBadWords;
	std::wstring pathToBannedWordsFile;
	void UpdateTopTen(); 
	void AddBadWord(const std::wstring& word); 
	std::wstring GetEnteredWords();
	void SearchAndProcessFiles(const std::wstring& inputDirectory);
	void ProcessTxtFile(const std::wstring& filePath);
	void CreateReport();
};
