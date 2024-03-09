#include "BadWordFinderDlg.h"

CBadWordFinderDlg* CBadWordFinderDlg::ptr = NULL;

CBadWordFinderDlg::CBadWordFinderDlg(void)
{
	ptr = this;
	isUsingFileForBannedWords = false; 
}

CBadWordFinderDlg::~CBadWordFinderDlg(void)
{
	if (hEventPause)
	{
		CloseHandle(hEventPause);
		hEventPause = NULL;
	}
	if (hEventStop)
	{
		CloseHandle(hEventStop);
		hEventStop = NULL;
	}
}

void CBadWordFinderDlg::Cls_OnClose(HWND hwnd)
{
	CloseHandle(hMutex);
	EndDialog(hwnd, 0);
}

// Глобальные переменные
std::vector<std::wstring> processedFiles; 
int totalReplacements = 0; 

void AddProcessedFile(const std::wstring& fileName) {
	processedFiles.push_back(fileName);
}

void IncrementTotalReplacements(int replacements) {
	totalReplacements += replacements;
}

std::wstring GetCurrentDirectory()
{
	wchar_t buffer[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, buffer);
	return std::wstring(buffer);
}

bool FileExists(const std::wstring& filename) {
	DWORD fileAttr = GetFileAttributes(filename.c_str());
	return (fileAttr != INVALID_FILE_ATTRIBUTES && !(fileAttr & FILE_ATTRIBUTE_DIRECTORY));
}

std::wstring CBadWordFinderDlg::GetEnteredWords()
{
	TCHAR buffer[1024] = { 0 };
	GetDlgItemText(hDialog, IDC_ENTERED_VALUE, buffer, 1024);
	return std::wstring(buffer);
}

/*
	Поиск запрещённых слов в файлах
		Реализовать приложение, позволяющее искать некоторый набор
	запрещенных слов в файлах.
		Пользовательский интерфейс приложения должен позволять ввести
	или загрузить из файла набор запрещенных слов. При нажатии на кнопку
	«Старт», приложение должно начать искать эти слова в одной директории.
		Файлы, содержащие запрещенные слова, должны быть скопированы
	в заданную папку.
		Кроме оригинального файла, нужно создать новый файл с
	содержимым оригинального файла, в котором запрещенные слова
	заменены на 7 повторяющихся звезд (*******).
		Также нужно создать файл отчета. Он должен содержать
	информацию обо всех найденных файлах с запрещенными словами, пути
	к этим файлам, размер файлов, информацию о количестве замен и так
	далее. В файле отчета нужно также отобразить топ-10 самых популярных
	запрещенных слов.
		Интерфейс программы должен показывать прогресс работы
	приложения с помощью индикаторов (progress bars). Пользователь через
	интерфейс приложения может приостановить работу алгоритма,
	возобновить, полностью остановить.
		По итогам работы программы необходимо вывести результаты
	работы в элементы пользовательского интерфейса (нужно продумать,
	какие элементы управления понадобятся).
		Программа обязательно должна использовать механизмы
	многопоточности и синхронизации!
		Программа может быть запущена только в одной копии


	Запустите программу и загрузите список запрещённых слов или введите их вручную.
	Нажмите "Старт" для начала поиска. 
	После окончания в директории программы появится отчёт "Отчёт.txt" с результатами (имя отчёта можно изменить).

*/


BOOL CBadWordFinderDlg::Cls_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{

	hDialog = hwnd;
	hStart = GetDlgItem(hwnd, IDC_START);
	hStop = GetDlgItem(hwnd, IDC_STOP);
	hPause = GetDlgItem(hwnd, IDC_PAUSE);
	hContinue = GetDlgItem(hwnd, IDC_CONTINUE);
	hEnteredValue = GetDlgItem(hwnd, IDC_ENTERED_VALUE);
	hStatus = GetDlgItem(hwnd, IDC_STATUS);
	hProgressBar = GetDlgItem(hwnd, IDC_PROGRESS1);
	hUploadFile = GetDlgItem(hwnd, IDC_UPLOAD);
	hSaveTo = GetDlgItem(hwnd, IDC_WHERE_TO_SAVE);
	hOutputAllBadWords = GetDlgItem(hwnd, IDC_LIST_OUTPUT_ALL);
	hOutputTopTenBadWords = GetDlgItem(hwnd, IDC_LIST_OUTPUT_TOP_10);
	hExit = GetDlgItem(hwnd, IDCANCEL);

	isUsingFileForBannedWords = false; 

	HINSTANCE hInst = GetModuleHandle(NULL);

	SendMessage(GetDlgItem(hwnd, IDC_ENTERED_VALUE), EM_SETLIMITTEXT, (WPARAM)30, 0);

	SetDlgItemText(hwnd, IDC_STATUS, TEXT("Ожидание действия"));

	hEventPause = CreateEvent(NULL, TRUE, TRUE, NULL); 
	hEventStop = CreateEvent(NULL, TRUE, FALSE, NULL); 

	if (!hEventPause || !hEventStop)
	{
		MessageBox(hwnd, TEXT("Не удалось создать события."), TEXT("Ошибка"), MB_OK | MB_ICONERROR);
	}

	SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
	SendMessage(hProgressBar, PBM_SETSTEP, (WPARAM)1, 0);

	SetDlgItemText(hwnd, IDC_WHERE_TO_SAVE, TEXT("Отчёт.txt"));

	EnableWindow(GetDlgItem(hwnd, IDC_WHERE_TO_SAVE), TRUE);

	hMutex = CreateMutex(NULL, FALSE, TEXT("{C54848BD-38B0-4F31-A983-7F65FF65BDDB}"));

	if (GetLastError() == ERROR_ALREADY_EXISTS) 
	{
		MessageBox(hwnd, TEXT("Приложение уже запущено."), TEXT("Ошибка"), MB_OK | MB_ICONERROR);

		CloseHandle(hMutex); 
		Cls_OnClose(hwnd);

		return FALSE; 
	}

	return TRUE;
}

void CBadWordFinderDlg::UpdateTopTen()
{

	std::sort
	(
		badWords.begin(), badWords.end(), [](const WordData& left, const WordData& right)
		{
			return left.count > right.count;
		}
	);

	topTenBadWords.clear();

	for (int i = 0; i < 10 && i < badWords.size(); ++i)
	{
		topTenBadWords.push_back(badWords[i]);
	}
}

void CBadWordFinderDlg::AddBadWord(const std::wstring& word)
{
	bool found = false;

	for (auto& wd : badWords)
	{
		if (wd.word == word)
		{
			wd.count++;
			found = true;

			break;
		}
	}

	if (!found)
	{
		badWords.push_back(WordData(word, 1));
	}

	UpdateTopTen();
}

bool ProcessWord(CBadWordFinderDlg* dlg, const std::wstring& word)
{
	DWORD waitResult = WaitForSingleObject(dlg->hEventPause, INFINITE); 

	if (waitResult == WAIT_OBJECT_0) 
	{
		if (WaitForSingleObject(dlg->hEventStop, 0) == WAIT_OBJECT_0)
		{
			SetDlgItemText(dlg->hDialog, IDC_STATUS, TEXT("Операция прервана пользователем"));

			return false; 
		}

		dlg->AddBadWord(word);
	}

	return true; 
}

void UpdateInterface(CBadWordFinderDlg* dlg) 
{
	SendMessage(dlg->hOutputAllBadWords, LB_RESETCONTENT, 0, 0);

	for (auto& wd : dlg->badWords)
	{
		SendMessage(dlg->hOutputAllBadWords, LB_ADDSTRING, 0, (LPARAM)wd.word.c_str());
	}

	dlg->UpdateTopTen();

	SendMessage(dlg->hOutputTopTenBadWords, LB_RESETCONTENT, 0, 0);

	for (auto& wd : dlg->topTenBadWords)
	{
		SendMessage(dlg->hOutputTopTenBadWords, LB_ADDSTRING, 0, (LPARAM)(wd.word + TEXT(" - ") + std::to_wstring(wd.count)).c_str());
	}

	EnableWindow(GetDlgItem(dlg->hDialog, IDC_START), TRUE);
	EnableWindow(GetDlgItem(dlg->hDialog, IDC_STOP), FALSE);
	EnableWindow(GetDlgItem(dlg->hDialog, IDC_PAUSE), FALSE);
}

void CBadWordFinderDlg::CreateReport() 
{
	std::locale::global(std::locale(""));

	TCHAR szReportName[MAX_PATH];
	GetDlgItemText(hDialog, IDC_WHERE_TO_SAVE, szReportName, MAX_PATH);

	TCHAR szCurrentDirectory[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szCurrentDirectory);

	std::wstring reportPath = std::wstring(szCurrentDirectory) + TEXT("\\") + std::wstring(szReportName);

	std::wofstream reportFile(reportPath, std::ios::out | std::ios::trunc);
	if (reportFile.is_open())
	{
		reportFile.imbue(std::locale(""));

		reportFile << TEXT("Отчет по работе\n");
		reportFile << TEXT("10 самых популярных слов:\n");

		for (auto& wordData : topTenBadWords) 
		{
			reportFile << wordData.word << L" - " << wordData.count << L"\n";
		}

		reportFile << TEXT("\nНазвания обработанных файлов и их размер:\n");

		for (auto& fileName : processedFiles) 
		{
			std::wifstream file(fileName, std::ifstream::ate | std::ifstream::binary);

			if (file.is_open()) 
			{
				auto fileSize = file.tellg();
				reportFile << fileName << TEXT(" - ") << fileSize << TEXT(" bytes\n");
				file.close();
			}
		}

		reportFile << TEXT("\nОбщее количество замен: ") << totalReplacements << TEXT("\n");
		reportFile.close();

		MessageBox(hDialog, TEXT("Отчет успешно создан."), TEXT("Информация"), MB_OK | MB_ICONINFORMATION);
	}
	else 
	{
		MessageBox(hDialog, TEXT("Ошибка при создании файла отчета."), TEXT("Ошибка"), MB_OK | MB_ICONERROR);
	}
}

void CBadWordFinderDlg::SearchAndProcessFiles(const std::wstring& inputDirectory)
{
	WIN32_FIND_DATA findFileData;
	std::wstring directory;

	TCHAR buffer[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, buffer);
	
	if (isUsingFileForBannedWords) 
	{
		directory = inputDirectory;
	}
	else 
	{	
		directory = std::wstring(buffer);
	}
	
	HANDLE hFind = FindFirstFile((directory + TEXT("\\*.txt")).c_str(), &findFileData);

	if (hFind == INVALID_HANDLE_VALUE) 
	{
		MessageBox(hDialog, TEXT("Не найдены текстовые файлы в директории."), TEXT("Информация"), MB_OK | MB_ICONINFORMATION);

		return;
	}
	else
	{
		do
		{
			std::wstring currentFilePath = directory + TEXT("\\") + findFileData.cFileName;

			if (isUsingFileForBannedWords && currentFilePath == pathToBannedWordsFile)
			{
				continue; 
			}

			ProcessTxtFile(currentFilePath);
			AddProcessedFile(currentFilePath);

		} while (FindNextFile(hFind, &findFileData) != 0);

		FindClose(hFind);
	}

	SetDlgItemText(hDialog, IDC_STATUS, TEXT("Создание отчета"));
	SendMessage(hProgressBar, PBM_SETPOS, 92, 0);

	Sleep(10);

	CreateReport();

	EnableWindow(GetDlgItem(hDialog, IDC_WHERE_TO_SAVE), TRUE);

	SendMessage(hProgressBar, PBM_SETPOS, 100, 0);
	SetDlgItemText(hDialog, IDC_STATUS, TEXT("Обработка завершена"));
}

void CBadWordFinderDlg::ProcessTxtFile(const std::wstring& filePath) 
{

	if (isUsingFileForBannedWords) {
		std::wstring content;
		std::wstring line;
		
		for (auto& wordData : badWords) {
			size_t pos = content.find(wordData.word);
			while (pos != std::wstring::npos) {
				content.replace(pos, wordData.word.length(), TEXT("*******"));
				pos = content.find(wordData.word, pos + 1);
			}
		}
	}
	{
		std::wifstream file(filePath);

		if (!file.is_open())
		{
			MessageBox(hDialog, (TEXT("Не удалось открыть файл: ") + filePath).c_str(), TEXT("Ошибка"), MB_OK | MB_ICONERROR);
			return;
		}

		std::wstring content;
		std::wstring line;

		SetDlgItemText(hDialog, IDC_STATUS, TEXT("Обработка запрещенных слов в файлах"));
		SendMessage(hProgressBar, PBM_SETPOS, 45, 0);

		Sleep(10);

		while (std::getline(file, line))
		{
			// Проверка на паузу
			DWORD waitResult = WaitForSingleObject(hEventPause, INFINITE);

			if (waitResult == WAIT_OBJECT_0)
			{
				
				if (WaitForSingleObject(hEventStop, 0) == WAIT_OBJECT_0)
				{
					SetDlgItemText(hDialog, IDC_STATUS, TEXT("Операция прервана пользователем"));

					break;
				}
				content += line + TEXT("\n");
				
			}
		}
		file.close();

		SetDlgItemText(hDialog, IDC_STATUS, TEXT("Замена запрещенных слов в файлах"));
		SendMessage(hProgressBar, PBM_SETPOS, 60, 0);
		Sleep(10);
		
		for (auto& wordData : badWords)
		{
			size_t pos = content.find(wordData.word);
			while (pos != std::wstring::npos)
			{
				
				if (WaitForSingleObject(hEventStop, 0) == WAIT_OBJECT_0)
				{
					SetDlgItemText(hDialog, IDC_STATUS, TEXT("Операция прервана пользователем"));
					return; 
				}

				DWORD waitResult = WaitForSingleObject(hEventPause, INFINITE);

				if (waitResult == WAIT_OBJECT_0)
				{
					content.replace(pos, wordData.word.length(), TEXT("*******"));
					pos = content.find(wordData.word, pos + 1);
					IncrementTotalReplacements(1);
				}
			}
		}
		
		SetDlgItemText(hDialog, IDC_STATUS, TEXT("Замена слов в файлах на *******"));
		SendMessage(hProgressBar, PBM_SETPOS, 75, 0);
		Sleep(10);
		
		if (WaitForSingleObject(hEventStop, 0) == WAIT_OBJECT_0)
		{
			SetDlgItemText(hDialog, IDC_STATUS, TEXT("Операция прервана пользователем"));
			return; 
		}
		
		DWORD waitResult = WaitForSingleObject(hEventPause, INFINITE);

		if (waitResult == WAIT_OBJECT_0)
		{
			
			std::wstring newFilePath = filePath + TEXT("_modified.txt");
			std::wofstream newFile(newFilePath);

			if (!newFile.is_open())
			{
				MessageBox(hDialog, (TEXT("Не удалось создать файл: ") + newFilePath).c_str(), TEXT("Ошибка"), MB_OK | MB_ICONERROR);
				return;
			}

			newFile << content;
			newFile.close();
		}
	}

}

DWORD WINAPI ReadBannedWords(LPVOID lpParam)
{
	auto dlg = reinterpret_cast<CBadWordFinderDlg*>(lpParam);

	std::wstring word;
	dlg->badWords.clear();
	dlg->topTenBadWords.clear();

	std::wifstream inFile;

	if (dlg->isUsingFileForBannedWords) {
		inFile.open(dlg->pathToBannedWordsFile);

		if (!inFile.is_open()) 
		{
			MessageBox(dlg->hDialog, TEXT("Не удалось открыть файл с запрещенными словами."), TEXT("Ошибка"), MB_OK | MB_ICONERROR);
			return 1;
		}

		SetDlgItemText(dlg->hDialog, IDC_STATUS, TEXT("Чтение запрещенных слов из файла"));
		SendMessage(dlg->hProgressBar, PBM_SETPOS, 20, 0);
		Sleep(10);
	}
	else
	{
		SetDlgItemText(dlg->hDialog, IDC_STATUS, TEXT("Чтение запрещенных слов из ввода пользователя"));
		SendMessage(dlg->hProgressBar, PBM_SETPOS, 20, 0);
		Sleep(10);
	}

	EnableWindow(GetDlgItem(dlg->hDialog, IDC_START), FALSE);
	EnableWindow(GetDlgItem(dlg->hDialog, IDC_STOP), TRUE);
	EnableWindow(GetDlgItem(dlg->hDialog, IDC_PAUSE), TRUE);


	if (dlg->isUsingFileForBannedWords)
	{
		while (inFile >> word)
		{
			if (!ProcessWord(dlg, word)) break;
		}
		inFile.close();
	}
	else
	{
		std::wstring enteredWords = dlg->GetEnteredWords();
		std::wistringstream wordsStream(enteredWords);
		while (wordsStream >> word)
		{
			if (!ProcessWord(dlg, word)) break;
		}
	}

	auto parentPath = dlg->pathToBannedWordsFile.substr(0, dlg->pathToBannedWordsFile.find_last_of(TEXT("\\/")));
	dlg->SearchAndProcessFiles(parentPath);

	UpdateInterface(dlg);

	return 0;
}

void CBadWordFinderDlg::Cls_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	switch (id)
	{
	case IDC_START:
	{
		ResetEvent(hEventStop);

		EnableWindow(GetDlgItem(hwnd, IDC_WHERE_TO_SAVE), FALSE);

		SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100)); 
		SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

		TCHAR szReportPath[MAX_PATH];
		GetDlgItemText(hwnd, IDC_WHERE_TO_SAVE, szReportPath, MAX_PATH);

		DWORD dwThreadId;
		HANDLE hThread = CreateThread(NULL, 0, ReadBannedWords, ptr, 0, &dwThreadId);

		if (hThread == NULL) {
			MessageBox(hwnd, TEXT("Не удалось создать поток."), TEXT("Ошибка"), MB_OK | MB_ICONERROR);
		}
		else
		{
			CloseHandle(hThread);
		}

		break;
	}
	case IDC_STOP:
	{
		SetEvent(hEventStop);

		EnableWindow(GetDlgItem(hwnd, IDC_WHERE_TO_SAVE), TRUE);

		MessageBox(hwnd, TEXT("Остановлено пользователем."), TEXT("Ошибка"), MB_OK | MB_ICONERROR);

		break;
	}
	case IDC_PAUSE:
	{
		ResetEvent(hEventPause);

		SetDlgItemText(hwnd, IDC_STATUS, TEXT("Пауза"));

		EnableWindow(GetDlgItem(hwnd, IDC_CONTINUE), TRUE);
		EnableWindow(GetDlgItem(hwnd, IDC_START), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_STOP), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_PAUSE), FALSE);

		break;
	}
	case IDC_CONTINUE:
	{
		SetEvent(hEventPause); 

		SetDlgItemText(hwnd, IDC_STATUS, TEXT("Продолжена работа"));

		EnableWindow(GetDlgItem(hwnd, IDC_CONTINUE), FALSE);
		EnableWindow(GetDlgItem(hwnd, IDC_STOP), TRUE);
		EnableWindow(GetDlgItem(hwnd, IDC_PAUSE), TRUE);

		break;
	}
	case IDC_ENTERED_VALUE:
	{
		if (codeNotify == EN_CHANGE)
		{
			TCHAR buffer[50];
			GetDlgItemText(hwnd, IDC_ENTERED_VALUE, buffer, 50);

			bool hasText = _tcslen(buffer) > 0;
			EnableWindow(GetDlgItem(hwnd, IDC_UPLOAD), !hasText);
			EnableWindow(GetDlgItem(hwnd, IDC_START), hasText);

			isUsingFileForBannedWords = !hasText;

			if (_tcslen(buffer) > 25)
			{
				MessageBox(hwnd, TEXT("Текст не должен превышать 25 символов."), TEXT("Внимание"), MB_OK | MB_ICONWARNING);
				buffer[25] = 0;
				SetDlgItemText(hwnd, IDC_ENTERED_VALUE, buffer);
			}

		}
		break;
	}
	case IDC_UPLOAD:
	{
		OPENFILENAME ofn;
		WCHAR szFile[260];
		WCHAR szTitle[512];

		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = hwnd;
		ofn.lpstrFile = szFile;
		ofn.lpstrFile[0] = '\0';
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = TEXT("All\0*.*\0Text\0*.TXT\0");
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = NULL;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (GetOpenFileName(&ofn))
		{
			wsprintf(szTitle, TEXT("Путь к файлу: %s"), ofn.lpstrFile);
			SetDlgItemText(hwnd, IDC_STATUS, TEXT("Файл выбран"));
			
			EnableWindow(GetDlgItem(hwnd, IDC_ENTERED_VALUE), FALSE);

			isUsingFileForBannedWords = true;

			EnableWindow(GetDlgItem(hwnd, IDC_START), TRUE);

			pathToBannedWordsFile = ofn.lpstrFile;
		}
		else
		{
			lstrcpy(szTitle, TEXT("Bad Word Detector"));
			SetDlgItemText(hwnd, IDC_STATUS, TEXT("Файл не выбран"));

			EnableWindow(GetDlgItem(hwnd, IDC_ENTERED_VALUE), TRUE);

			isUsingFileForBannedWords = false;

			EnableWindow(GetDlgItem(hwnd, IDC_START), FALSE);
		}

		SetWindowText(hwnd, szTitle);

		break;
	}
	case IDCANCEL:
	{
		Cls_OnClose(hwnd);
		break;
	}

	}
}

BOOL CALLBACK CBadWordFinderDlg::DlgProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		HANDLE_MSG(hwnd, WM_CLOSE, ptr->Cls_OnClose);
		HANDLE_MSG(hwnd, WM_INITDIALOG, ptr->Cls_OnInitDialog);
		HANDLE_MSG(hwnd, WM_COMMAND, ptr->Cls_OnCommand);
	}
	return FALSE;
}