#include "VinaApp.h"

struct WaterfallCard {
	float y;
	std::wstring text;
	unsigned long color;
	float animY = 20.0f;
	float alpha = 0.0f;
	bool animated = false;
};

class MainAppContext {
public:
	int currentTabIndex = 0;
	float scrollVal=0;
	std::vector<std::shared_ptr<VinaSideTab>> tabs;
	std::shared_ptr<VinaButton> btn = std::make_shared<VinaButton>();
	std::shared_ptr<VinaButton> btnf = std::make_shared<VinaButton>();
	std::shared_ptr<VinaButton> startBtn = std::make_shared<VinaButton>();
	std::shared_ptr<VinaSlider> sli = std::make_shared<VinaSlider>();
	std::shared_ptr<VinaSwitch> sw = std::make_shared<VinaSwitch>();
	std::shared_ptr<VinaFileSelector> fi = std::make_shared<VinaFileSelector>();
	std::shared_ptr<VinaMultiTextBox> mt = std::make_shared<VinaMultiTextBox>();
	std::shared_ptr<VinaEdit> edt = std::make_shared<VinaEdit>();
	std::shared_ptr<VinaEdit> edtf = std::make_shared<VinaEdit>();
	std::shared_ptr<VinaCaptionBar> capt = std::make_shared<VinaCaptionBar>();
	std::atomic<bool> monitoring{ false };      
	std::atomic<bool> stopMonitoring{ false };  
	std::atomic<bool> monitoringFinished{ false };
	std::atomic<bool> followLaunchFailed{ false };
	struct LogMessage {
		std::wstring text;
		DWORD startTick;          // 开始显示的时间戳（毫秒）
		float alpha;              // 当前透明度 (0~1)
		float yOffset;            // Y轴偏移量（用于滑动效果）
		int animStage;            // 0=稳定显示, 1=进入动画, 2=退出动画
		DWORD animStartTime;      // 动画开始时间（毫秒）
		DWORD animDuration;       // 动画持续时间（毫秒）

		LogMessage(const std::wstring& t, DWORD now)
			: text(t), startTick(now), alpha(0.0f), yOffset(20.0f),
			animStage(1), animStartTime(now), animDuration(200) {
		}
	};

	std::list<LogMessage> logMessages;
	std::mutex logMutex;         


	std::wstring lastText;
	std::list<CharAnim> animatingChars;
	ID2D1Bitmap* pAppLogo = nullptr;

	std::vector<WaterfallCard> waterfallData;

	float maxScrollLimit = 0.0f; 
	bool isGenerating = false; 
	int activeAnimationCount = 0;

	void InitTabs() {
		if (!tabs.empty()) return;
		auto tab1 = std::make_shared<VinaSideTab>();
		tab1->Set(0, 55, 120, L"Link", L"test-home", 20, VuiFadeColor(VERTEXUICOLOR_WHITE,40));
		tab1->Activate(true);

		//auto tab2 = std::make_shared<VinaSideTab>();
		//tab2->Set(0, 55, 120, L"Waterfall", L"test-test1", 20, VuiFadeColor(VERTEXUICOLOR_WHITE, 20));
		
		auto tab3 = std::make_shared<VinaSideTab>();
		tab3->Set(0, 55, 120, L"About", L"test-info", 20, VuiFadeColor(VERTEXUICOLOR_WHITE, 20));


		tabs.push_back(tab1);
		//tabs.push_back(tab2);
		tabs.push_back(tab3);
	}

	void InitWaterfall() {
		if (!waterfallData.empty()) return;
		GenerateCards(6);
	}

	void GenerateCards(int count) {
		unsigned long macaronColors[] = { VERTEXUICOLOR_SEA, VERTEXUICOLOR_BLOOMLAVENDER, VERTEXUICOLOR_DAWN, VERTEXUICOLOR_FOREST, VERTEXUICOLOR_SEA };
		for (int i = 0; i < count; i++) {
			WaterfallCard card;
			card.text = L"VinaUI Card " + std::to_wstring(waterfallData.size());
			card.color = macaronColors[rand() % 5];
			waterfallData.push_back(card);
		}
	}

	void ResetWaterfallAnimationState() {
		activeAnimationCount = 0;
		for (auto& card : waterfallData) {
			if (card.animated && (card.alpha < 1.0f || card.animY > 0.0f)) {
				card.alpha = 1.0f;
				card.animY = 0.0f;
			}
		}
	}

	void LoadPaths() {
		HKEY hKey;
		if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Linker", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			WCHAR buffer[MAX_PATH];
			DWORD size = sizeof(buffer);
			if (RegQueryValueEx(hKey, L"MainPath", nullptr, nullptr, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
				edt->SetText(buffer);
				edt->bSetText = true;
			}
			size = sizeof(buffer);
			if (RegQueryValueEx(hKey, L"FollowPath", nullptr, nullptr, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
				edtf->SetText(buffer);
				edtf->bSetText = true;
			}
			RegCloseKey(hKey);
		}
	}

	void SavePaths() {
		HKEY hKey;
		if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Linker", 0, nullptr,
			REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
			std::wstring mainPath = edt->GetText();
			std::wstring followPath = edtf->GetText();
			RegSetValueEx(hKey, L"MainPath", 0, REG_SZ,
				(const BYTE*)mainPath.c_str(), (mainPath.size() + 1) * sizeof(wchar_t));
			RegSetValueEx(hKey, L"FollowPath", 0, REG_SZ,
				(const BYTE*)followPath.c_str(), (followPath.size() + 1) * sizeof(wchar_t));
			RegCloseKey(hKey);
		}
	}

	void AddLog(const std::wstring& msg) {
		std::lock_guard<std::mutex> lock(logMutex);
		DWORD now = GetTickCount();
		logMessages.push_front(LogMessage(msg, now));
		if (logMessages.size() > 10) {
			logMessages.pop_back();
		}
	}
};


std::wstring GetNormalizedProcessPath(HANDLE hProcess) {
	WCHAR szPath[MAX_PATH];
	DWORD size = MAX_PATH;
	if (!QueryFullProcessImageName(hProcess, 0, szPath, &size))
		return L"";
	// 转换为长路径
	WCHAR longPath[MAX_PATH];
	if (GetLongPathName(szPath, longPath, MAX_PATH) == 0)
		wcscpy_s(longPath, szPath);
	// 转为大写以便比较
	_wcsupr_s(longPath);
	return std::wstring(longPath);
}

VinaWindow* MainWindow = new VinaWindow;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int        nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

	InitGlobalD2D();

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VINAAPP));

	VuiColorSystemInit();
	gScale = GetScreenScale();
	//LoadVinaCom();

	auto fontData = AppResourceManager::LoadRawResource(hInstance, IDF_FONTAWESOME, L"BINARY");

	if (!fontData.empty()) {
		DWORD nFonts = 0;
		static UniqueFontHandle myFontHandle{
			AddFontMemResourceEx((void*)fontData.data(), (DWORD)fontData.size(), nullptr, &nFonts)
		};
	}

	namespace fs = std::filesystem;
	fs::path fontPath =  L"Font-AwesomeFree.ttf";

	FreeAnyResource(IDF_FONTAWESOME, L"BINARY", fontPath.c_str());
	AddFontResource(fontPath.c_str());

	auto ctx = std::make_shared<MainAppContext>();

	MainWindow->Set(100, 100, 770 * gScale, 440 * gScale, L"Vina.Class.App.Main.Test", L"Vilinko VinaUI");



	MainWindow->CreatePanel([ctx](HWND hWnd, ID2D1HwndRenderTarget* hrt)->void {

		RECT rc;
		GetClientRect(hWnd, &rc);
		D2DDrawSolidRect(hrt, 0, 0, rc.right, rc.bottom, VERTEXUICOLOR_DARKNIGHT);
		MainWindow->GetPanel()->Set(hWnd, hrt);

		/*
		CreatePanel 会在窗口刷新时被调用。

		控件需要使用静态标识，或者在 CreatePanel 外部初始化。

		使用 std::vector 等容器储存控件也是较好的方法。
		*/


		if (ctx->currentTabIndex == 0) {

			std::wstring description = L"选择要启动的主程序，再选择要随之启动的联动程序，点击启动按钮后，当主程序运行时，联动程序就会自动启动\n";
			D2DDrawText(hrt, description.c_str(), 40, 100, 610, 100, 16,
				((COLORREF)(((BYTE)(vuicolor.txt_r) | ((WORD)((BYTE)(vuicolor.txt_g)) << 8)) | (((DWORD)(BYTE)(vuicolor.txt_b)) << 16))), L"Segoe UI", 0.9f);

			std::wstring maindescrip = L"主程序";
			D2DDrawText(hrt, maindescrip.c_str(), 75, 182, 560, 100, 16,
				VERTEXUICOLOR_WHITE, L"Segoe UI", 0.9f);

			if (ctx->edt->cx == 0)
			{
				ctx->edt->Set(110, 100, 300, 30, L"请选择要启动的主程序");
			}
			ctx->edt->Set2(170, 178, 350, 30, VERTEXUICOLOR_MIDNIGHT, VERTEXUICOLOR_WHITE);
			MainWindow->GetPanel()->Add(ctx->edt);

			ctx->btn->Set(560, 178, 60, 30, L"浏览", [ctx] {
				OPENFILENAME ofn = { 0 };
				wchar_t szFile[260] = { 0 };
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = MainWindow->GetHandle();      
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
				ofn.lpstrFilter = L"Executable Files\0*.exe\0All Files\0*.*\0";
				ofn.nFilterIndex = 1;
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;  

				if (GetOpenFileName(&ofn)) {
					ctx->edt->SetText(szFile);
					ctx->SavePaths();
				}
			});
			MainWindow->GetPanel()->Add(ctx->btn);

			std::wstring followdescrip = L"联动程序";
			D2DDrawText(hrt, followdescrip.c_str(), 62, 220, 560, 100, 16,
				VERTEXUICOLOR_WHITE, L"Segoe UI", 0.9f);

			if (ctx->edtf->cx == 0)
			{
				ctx->edtf->Set(100, 100, 300, 30, L"请选择要被启动的程序");
			}
			ctx->edtf->Set2(170, 218, 350, 30, VERTEXUICOLOR_MIDNIGHT, VERTEXUICOLOR_WHITE);
			MainWindow->GetPanel()->Add(ctx->edtf);

			ctx->btnf->Set(560, 218, 60, 30, L"浏览", [ctx] {
				OPENFILENAME ofn = { 0 };
				wchar_t szFile[260] = { 0 };
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = MainWindow->GetHandle();       
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
				ofn.lpstrFilter = L"All Files\0*.*\0Executable Files\0*.exe\0";
				ofn.nFilterIndex = 1;
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;  

				if (GetOpenFileName(&ofn)) {
					ctx->edtf->SetText(szFile);
					ctx->SavePaths();
				}
			});
			MainWindow->GetPanel()->Add(ctx->btnf);

			ctx->startBtn->Set(80, 300, 144, 62, L"启动", [ctx] {
				if (ctx->monitoring) {
					// 正在监控 -> 停止监控
					ctx->stopMonitoring = true;
					ctx->startBtn->SetText(L"停止中...");
					InvalidateRect(MainWindow->GetHandle(), nullptr, FALSE);
				}
				else {
					// 未监控 -> 开始监控
					std::wstring mainPath = ctx->edt->GetText();
					std::wstring followPath = ctx->edtf->GetText();
					if (mainPath.empty() || followPath.empty()) {
						MessageBox(MainWindow->GetHandle(), L"请选择主程序和联动程序", L"提示", MB_OK);
						return;
					}

					ctx->monitoring = true;
					ctx->stopMonitoring = false;
					ctx->monitoringFinished = false;
					ctx->followLaunchFailed = false;

					ctx->SavePaths();
					ctx->startBtn->SetText(L"停止监控");
					ctx->AddLog(L"开始监控主程序...");
					InvalidateRect(MainWindow->GetHandle(), nullptr, FALSE);

					std::thread([ctx, mainPath, followPath]() {
						WCHAR mainLong[MAX_PATH], followLong[MAX_PATH];
						GetLongPathName(mainPath.c_str(), mainLong, MAX_PATH);
						GetLongPathName(followPath.c_str(), followLong, MAX_PATH);
						_wcsupr_s(mainLong);
						_wcsupr_s(followLong);
						std::wstring normMain = mainLong;
						std::wstring normFollow = followLong;
						bool lastMainRunning = false;

						while (!ctx->stopMonitoring) {
							bool mainRunning = false;

							HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
							if (hSnapshot != INVALID_HANDLE_VALUE) {
								PROCESSENTRY32 pe32;
								pe32.dwSize = sizeof(PROCESSENTRY32);
								if (Process32First(hSnapshot, &pe32)) {
									do {
										HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
										if (hProcess) {
											std::wstring procPath = GetNormalizedProcessPath(hProcess);
											CloseHandle(hProcess);
											if (procPath == normMain) {
												mainRunning = true;
												break;
											}
										}
									} while (Process32Next(hSnapshot, &pe32));
								}
								CloseHandle(hSnapshot);
							}

							if (mainRunning && !lastMainRunning) {
								ctx->AddLog(L"检测到主程序运行");

								bool followRunning = false;
								hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
								if (hSnapshot != INVALID_HANDLE_VALUE) {
									PROCESSENTRY32 pe32;
									pe32.dwSize = sizeof(PROCESSENTRY32);
									if (Process32First(hSnapshot, &pe32)) {
										do {
											HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
											if (hProcess) {
												std::wstring procPath = GetNormalizedProcessPath(hProcess);
												CloseHandle(hProcess);
												if (procPath == normFollow) {
													followRunning = true;
													break;
												}
											}
										} while (Process32Next(hSnapshot, &pe32));
									}
									CloseHandle(hSnapshot);
								}

								if (!followRunning) {
									ctx->AddLog(L"正在启动联动程序");
									HINSTANCE result = ShellExecute(nullptr, L"open", followPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
									if ((INT_PTR)result <= 32) {
										ctx->followLaunchFailed = true;
										ctx->AddLog(L"联动程序启动失败");
									}
									else {
										ctx->AddLog(L"联动程序启动成功");
									}
								}
								else {
									ctx->AddLog(L"联动程序已在运行");
								}
							}

							lastMainRunning = mainRunning;

							Sleep(1000); 
						}
						ctx->monitoring = false;
						ctx->monitoringFinished = true;
						InvalidateRect(MainWindow->GetHandle(), nullptr, FALSE);
						}).detach();
				}
			});
			MainWindow->GetPanel()->Add(ctx->startBtn);
			\
			const float logAreaX = 550.0f;
			const float logAreaY = 300.0f;      
			const float logAreaHeight = 120.0f; 
			const float lineHeight = 20.0f;     
			const int maxLines = static_cast<int>(logAreaHeight / lineHeight);

			{
				std::lock_guard<std::mutex> lock(ctx->logMutex);
				DWORD now = GetTickCount();

				for (auto it = ctx->logMessages.begin(); it != ctx->logMessages.end(); ) {
					bool remove = false;

					// 检查是否该启动退出动画
					if (it->animStage == 0 && (now - it->startTick) >= 5000) {
						it->animStage = 2;
						it->animStartTime = now;
						it->animDuration = 200;
					}

					// 处理动画
					if (it->animStage == 1) { // 进入动画
						float progress = static_cast<float>(now - it->animStartTime) / it->animDuration;
						if (progress >= 1.0f) {
							it->alpha = 1.0f;
							it->yOffset = 0.0f;
							it->animStage = 0; // 稳定显示
						}
						else {
							float eased = CalcEaseOutCurve(progress, 0, 1, 1);
							it->alpha = eased;
							it->yOffset = 20.0f * (1.0f - eased); // 从20px滑入
						}
					}
					else if (it->animStage == 2) { // 退出动画
						float progress = static_cast<float>(now - it->animStartTime) / it->animDuration;
						if (progress >= 1.0f) {
							it->alpha = 0.0f;
							it->yOffset = 20.0f;
							remove = true; // 动画结束，移除消息
						}
						else {
							float eased = CalcEaseOutCurve(progress, 0, 1, 1);
							it->alpha = 1.0f - eased;
							it->yOffset = 20.0f * eased; // 向下滑出
						}
					}

					if (remove)
						it = ctx->logMessages.erase(it);
					else
						++it;
				}
			}

			// 绘制日志
			{
				std::lock_guard<std::mutex> lock(ctx->logMutex);
				DWORD now = GetTickCount();
				
				ctx->logMessages.remove_if([now](const MainAppContext::LogMessage& msg) {
					return (now - msg.startTick) >= 5000;
					});
				
				int line = 0;
				for (auto it = ctx->logMessages.rbegin(); it != ctx->logMessages.rend(); ++it) {
					if (line >= maxLines) break;  

					float y = logAreaY + logAreaHeight - (line + 1) * lineHeight;

					D2DDrawText2(hrt, it->text.c_str(), logAreaX, y, 300, lineHeight,
						14, VERTEXUICOLOR_DARKENX, L"Segoe UI", it->alpha);

					line++;
				}
			}

			//ctx->sli->Set(40, 200, 140, 30, -1, VERTEXUICOLOR_DARKEN, L"Slider", [] {});
			//MainWindow->GetPanel()->Add(ctx->sli);

			//ctx->sw->Set(40, 255, 60, 30, { VERTEXUICOLOR_DARKEN }, [] {});
			//MainWindow->GetPanel()->Add(ctx->sw);
			/*
			if (ctx->sw->GetValue())
			{
				ctx->fi->Set(40, 300, 260, 160);
				ctx->fi->SetFileOpenCallback([](std::wstring path)->void {
					MessageBox(0, path.c_str(), L"Current", 0);
					});
				ctx->fi->SetParent(MainWindow->GetPanel());
				MainWindow->GetPanel()->Add(ctx->fi);
			}


			ctx->mt->Set(260, 120, 300, 30, L"This is a test string.\nAnd this is a multi line text area.\ne.g:\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10");
			ctx->mt->SetParent(MainWindow->GetPanel());
			MainWindow->GetPanel()->Add(ctx->mt);

			*/

			/*
			此处演示动画效果，以及动画播放时的内存管理。

			动画效果可以直接在 CreatePanel（也就是 OnPaint ）时操作，也可以在控件内部操作。

			此处为了演示方便，直接在 CreatePanel 内部操作。
			*/

			/*
			std::wstring currentText = ctx->edt->GetText();

			if (currentText != ctx->lastText) {
				if (currentText.length() > ctx->lastText.length()) {

					for (size_t i = ctx->lastText.length(); i < currentText.length(); ++i) {

						float lockedX = VuiMeasureStringWidth(currentText.substr(0, i), 18);

						ctx->animatingChars.push_back({ currentText[i], 0.0f, lockedX, 18.0f, false });
						CharAnim& ref = ctx->animatingChars.back();

						ref.alphaAnimId = MainWindow->AnimateVariableWithBezier(hWnd, ref.alpha, 0.0f, 1.0f, 0.4);
						ref.yOffsetAnimId = MainWindow->AnimateVariableWithBezier(hWnd, ref.yOffset, 18.0f, 0.0f, 0.4);
					}
				}
				else if (currentText.length() < ctx->lastText.length()) {

					size_t diff = ctx->lastText.length() - currentText.length();
					for (size_t i = 0; i < diff; ++i) {

						auto it = std::find_if(ctx->animatingChars.rbegin(), ctx->animatingChars.rend(),
							[](const CharAnim& c) { return !c.isRemoving; });

						if (it != ctx->animatingChars.rend()) {
							it->isRemoving = true;
							CharAnim& ref = *it;

							ref.alphaAnimId = MainWindow->AnimateVariableWithBezier(hWnd, ref.alpha, ref.alpha, 0.0f, 0.3);
							ref.yOffsetAnimId = MainWindow->AnimateVariableWithBezier(hWnd, ref.yOffset, ref.yOffset, 10.0f, 0.3);
						}
					}
				}
				ctx->lastText = currentText;
			}
			*/

			//清理
			ctx->animatingChars.remove_if([&](const CharAnim& c) {
				if (c.isRemoving && c.alpha <= 0.01f) {
		
					if (c.alphaAnimId != -1) {
						MainWindow->StopAnimation(c.alphaAnimId);
					}
					if (c.yOffsetAnimId != -1) {
						MainWindow->StopAnimation(c.yOffsetAnimId);
					}
					return true; 
				}
				return false;
				});

			float startX_txt = 260.0f;
			float startY_txt = 290.0f;

			if (ctx->monitoringFinished) {
				ctx->monitoringFinished = false;
				ctx->startBtn->SetText(L"启动");
				ctx->AddLog(L"已停止监控主程序");
				InvalidateRect(MainWindow->GetHandle(), nullptr, FALSE);
			}

			if (ctx->followLaunchFailed) {
				ctx->AddLog(L"启动联动程序失败");
				ctx->followLaunchFailed = false;
			}

			for (auto& item : ctx->animatingChars) {
				D2DDrawText2(hrt, std::wstring(1, item.ch).c_str(),
					startX_txt + item.xOffset, startY_txt + item.yOffset,
					20, 20, 18,
					VERTEXUICOLOR_WHITE, L"Segoe UI", item.alpha);
			}
		}
		if (ctx->currentTabIndex == 1)
		{
			float pageX = 60.0f;
			float pageY = 120.0f;
			float cardW = rc.right / gScale - 120.0f;
			float cardH = 300.0f;

			// 静态缓存位图，提升性能
			if (!ctx->pAppLogo) {
				ctx->pAppLogo = LoadIconToD2DBitmap(hrt, IDI_SMALL);
			}
			D2DDrawQuickShadow(hrt, pageX, pageY, rc.right / gScale - 120, 300, 15, 0, 2, 16, 8, 0, 0.02f, VERTEXUICOLOR_WHITE);
			D2DDrawRoundRect(hrt, pageX, pageY, rc.right / gScale - 120, 300, VERTEXUICOLOR_MIDNIGHT, 15, 0.5f, 1, 0, 0.1f);

			if (ctx->pAppLogo) {

				D2D1_RECT_F iconRect = D2D1::RectF(pageX + 40, pageY + 40, pageX + 104, pageY + 104);
				hrt->DrawBitmap(ctx->pAppLogo, iconRect);
			}

			D2DDrawText3(hrt, L"Vilinko VinaUI", pageX + 130, pageY + 45, 300, 40, 28,
				VERTEXUICOLOR_WHITE);

			D2DDrawText(hrt, L"Developer & Designer : CimiMoly", pageX + 130, pageY + 85, 300, 30, 14,
				VuiDarkenColor(VERTEXUICOLOR_WHITE, 100), L"Segoe UI", 0.8f);

			D2DDrawSolidRect(hrt, pageX + 40, pageY + 130, cardW - 80, 1,
				VuiDarkenColor(VERTEXUICOLOR_DARKNIGHT, 20), 0.5f);

			std::wstring description = L"Nice to meet U(●'◡'●)! VinaUI is here! Special thanks to @Haceau-Zoac for vina parser~\n";
			D2DDrawText(hrt, description.c_str(), pageX + 40, pageY + 150, cardW - 80, 100, 16,
				VERTEXUICOLOR_WHITE, L"Segoe UI", 0.9f);

			std::wstring copyright = L"Copyright © 2026 Vilinko. All rights reserved.";
			float cpWidth = VuiMeasureStringWidth(copyright, 14);
			D2DDrawText(hrt, copyright.c_str(), (rc.right / gScale - cpWidth) / 2.0f, pageY + cardH - 40,
				cpWidth + 20, 30, 14, VuiDarkenColor(VERTEXUICOLOR_WHITE, 150), L"Segoe UI", 0.7f);

			static std::shared_ptr<VinaButton> webBtn = std::make_shared<VinaButton>();
			webBtn->Set(pageX + 40, pageY + cardH - 90, 120, 35, L"Github", [] {
				ShellExecute(0, L"open", L"https://github.com/EnderMo/VinaUI", 0, 0, SW_SHOWNORMAL);
				});
			MainWindow->GetPanel()->Add(webBtn);

			static std::shared_ptr<VinaButton> spBtn = std::make_shared<VinaButton>();
			spBtn->Set(pageX + 180, pageY + cardH - 90, 120, 35, L"Sponsor", [] {
				ShellExecute(0, L"open", L"https://afdian.com/a/EnderMo", 0, 0, SW_SHOWNORMAL);
				});
			MainWindow->GetPanel()->Add(spBtn);
		}

		for (float i = 2; i < 90; i++)
		{
			D2DDrawSolidRect(hrt, 0, 1 * (i - 2), rc.right / gScale, 1.5F, VERTEXUICOLOR_DARKNIGHT, 1.0f - (i - 1.0f) / 80.0f);
		}
		D2DCreateQuickHeavyBlur(hrt, 0, 0, rc.right, 90 * gScale, 12);

		ctx->capt->Set(0, 0, rc.right / gScale - 160, 40, L"Linker", VERTEXUICOLOR_DARKNIGHT, 18);
		MainWindow->GetPanel()->Add(ctx->capt);

		static std::shared_ptr<VinaFAIcon>close = std::make_shared<VinaFAIcon>();
		close->Set(rc.right / gScale - 32, 20, L"win-close", 15, VERTEXUICOLOR_WHITE, [] {DestroyWindow(MainWindow->GetHandle()); PostQuitMessage(0); });
		MainWindow->GetPanel()->Add(close);

		bool IsMaximized = IsZoomed(hWnd) != 0;

		if (!IsMaximized)
		{
			static std::shared_ptr<VinaFAIcon>max = std::make_shared<VinaFAIcon>();
			max->Set(rc.right / gScale - 32 - 32, 20, L"win-max", 15, VERTEXUICOLOR_WHITE, [hWnd] {
				SendMessage(hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0); });
			MainWindow->GetPanel()->Add(max);
		}
		else {
			static std::shared_ptr<VinaFAIcon>max = std::make_shared<VinaFAIcon>();
			max->Set(rc.right / gScale - 32 - 32, 20, L"win-restore", 15, VERTEXUICOLOR_WHITE, [hWnd] {
				SendMessage(hWnd, WM_SYSCOMMAND, SC_RESTORE, 0); });
			MainWindow->GetPanel()->Add(max);
		}
		static std::shared_ptr<VinaFAIcon>min = std::make_shared<VinaFAIcon>();
		min->Set(rc.right / gScale - 32 - 32 - 32, 20, L"win-min", 15, VERTEXUICOLOR_WHITE, [hWnd] {MainWindow->KillAnimation(); SendMessage(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0); });
		MainWindow->GetPanel()->Add(min);



		ctx->InitTabs();

		float totalWidth = ctx->tabs.size() * 130.0f;
		float startX = (rc.right / gScale - totalWidth) / 2.0f;

		for (size_t i = 0; i < ctx->tabs.size(); i++) {
			ctx->tabs[i]->Set(startX + i * 130, 55, 120, ctx->tabs[i]->GetText().c_str(), ctx->tabs[i]->txt.c_str(), 20, VuiFadeColor(VERTEXUICOLOR_WHITE, 20), [ctx, i] {
				ctx->currentTabIndex = (int)i;
				ctx->ResetWaterfallAnimationState();
				});
			ctx->tabs[i]->Activate(ctx->currentTabIndex == i);
			MainWindow->GetPanel()->Add(ctx->tabs[i]);
		}

		//LoadBitmapFromFile(hrt, m_ImageFactory, L"D:\\FLUENTEMOJI\\four_leaf_clover_3d.png", 20, 20, 100, 100);

		});

	MainWindow->SetOutFrame(VinaWindow::Client);
	MainWindow->OnCreateCmd = [ctx] {
		CenterWindow(MainWindow->GetHandle());
		MainWindow->InitAnimation();
		MainWindow->StartAnimationSystem();
		ctx->LoadPaths();
		};
	MainWindow->RunFull();

	return 0;
}