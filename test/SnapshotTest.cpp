#include <imgui.h>
#include <application.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include <string>
#include <sstream>
#include "Overview.h"
#include "Snapshot.h"
#include "FFUtils.h"
#include "TextureManager.h"
#include "Logger.h"
#include "DebugHelper.h"

using namespace std;
using namespace MediaCore;
using namespace RenderUtils;
using namespace Logger;

static Overview::Holder g_movr;
static Snapshot::Generator::Holder g_ssgen;
static Snapshot::Viewer::Holder g_ssvw1;
static double g_windowPos = 0.f;
static double g_windowSize = 300.f;
static double g_windowFrames = 14.0f;
static Vec2<int32_t> g_snapImageSize;
static TextureManager::Holder g_txmgr;
static string g_snapTxPoolName = "SnapshotGridTexturePool";
const string c_imguiIniPath = "ms_test.ini";
const string c_bookmarkPath = "bookmark.ini";
static bool g_isImageSequence = false;
static MediaParser::Holder g_mediaParser;

// Application Framework Functions
static void MediaSnapshot_Initialize(void** handle)
{
    GetDefaultLogger()
        ->SetShowLevels(DEBUG);
    MediaParser::GetLogger()
        ->SetShowLevels(INFO);
    Snapshot::GetLogger()
        ->SetShowLevels(INFO);
    g_txmgr = TextureManager::CreateInstance();

#ifdef USE_BOOKMARK
	// load bookmarks
	ifstream docFile(c_bookmarkPath, ios::in);
	if (docFile.is_open())
	{
		stringstream strStream;
		strStream << docFile.rdbuf(); //read the file
		ImGuiFileDialog::Instance()->DeserializeBookmarks(strStream.str());
		docFile.close();
	}
#endif

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = c_imguiIniPath.c_str();

    size_t ssCnt = (size_t)ceil(g_windowFrames)+1;
    g_movr = Overview::CreateInstance();
    g_movr->SetSnapshotSize(320, 180);
    g_ssgen = Snapshot::Generator::CreateInstance();
    g_ssgen->SetLogLevel(DEBUG);
    // g_ssgen->SetSnapshotResizeFactor(0.5f, 0.5f);
    g_ssgen->SetCacheFactor(3);
    g_ssvw1 = g_ssgen->CreateViewer(0);
}

static void MediaSnapshot_Finalize(void** handle)
{
    g_ssgen->ReleaseViewer(g_ssvw1);
    g_ssgen = nullptr;
    g_movr = nullptr;
#ifdef USE_BOOKMARK
	// save bookmarks
	ofstream configFileWriter(c_bookmarkPath, ios::out);
	if (!configFileWriter.bad())
	{
		configFileWriter << ImGuiFileDialog::Instance()->SerializeBookmarks();
		configFileWriter.close();
	}
#endif

    g_txmgr->Release();
    g_txmgr = nullptr;
}

static bool MediaSnapshot_Frame(void * handle, bool app_will_quit)
{
    bool app_done = false;
    auto& io = ImGui::GetIO();
    if (g_snapImageSize.x == 0 || g_snapImageSize.y == 0)
    {
        g_snapImageSize.x = (int32_t)(io.DisplaySize.x/(g_windowFrames+1));
        g_snapImageSize.y = (int32_t)(g_snapImageSize.x*9/16);
        g_ssgen->SetSnapshotSize(g_snapImageSize.x, g_snapImageSize.y);
        g_txmgr->CreateGridTexturePool(g_snapTxPoolName, g_snapImageSize, IM_DT_INT8, {16, 9}, 0);
    }

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    if (ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        if (ImGui::Button((string(ICON_IGFD_FOLDER_OPEN)+" Open file").c_str()))
        {
            const char *filters = "视频文件(*.mp4 *.mov *.mkv *.webm *.avi *.mxf){.mp4,.mov,.mkv,.webm,.avi,.mxf,.MP4,.MOV,.MKV,.WEBM,.AVI,.MXF},.*";
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开视频文件", 
                                                    filters, "~/Videos/", 1, nullptr, 
                                                    ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_Modal);
        }

        ImGui::SameLine();
        ImGui::Checkbox("Open image sequence", &g_isImageSequence);

        ImGui::SameLine();
        if (ImGui::Button("Refresh snapwnd configuration"))
            g_ssgen->ConfigSnapWindow(g_windowSize, g_windowFrames, true);

        ImGui::Spacing();

        float pos = g_windowPos;
        float minPos = (float)g_ssgen->GetVideoMinPos()/1000.f;
        float vidDur = (float)g_ssgen->GetVideoDuration()/1000.f;
        if (ImGui::SliderFloat("Position", &pos, minPos, minPos+vidDur, "%.3f"))
        {
            g_windowPos = pos;
        }

        float wndSize = g_windowSize;
        float minWndSize = (float)g_ssgen->GetMinWindowSize();
        float maxWndSize = (float)g_ssgen->GetMaxWindowSize();
        if (ImGui::SliderFloat("WindowSize", &wndSize, minWndSize, maxWndSize, "%.3f"))
            g_windowSize = wndSize;
        if (ImGui::IsItemDeactivated())
            g_ssgen->ConfigSnapWindow(g_windowSize, g_windowFrames);

        ImGui::Spacing();

        vector<Snapshot::Image::Holder> snapshots;
        // auto t0 = GetTimePoint();
        bool ret = g_ssvw1->GetSnapshots(pos, snapshots);
        // auto t1 = GetTimePoint();
        // Log(WARN) << "<TimeCost> GetSnapshots() : " << CountElapsedMillisec(t0, t1) << endl;
        if (!ret)
            snapshots.clear();
        else
            g_ssvw1->UpdateSnapshotTexture(snapshots, g_txmgr, g_snapTxPoolName);

        float startPos = minPos;
        if (snapshots.size() > 0 && snapshots[0] && snapshots[0]->mTimestampMs != INT64_MIN)
            startPos = (float)snapshots[0]->mTimestampMs/1000;
        int snapshotCnt = (int)ceil(g_windowFrames);
        for (int i = 0; i < snapshotCnt; i++)
        {
            ImGui::BeginGroup();
            if (i >= snapshots.size())
            {
                ImGui::Dummy(g_snapImageSize);
                ImGui::TextUnformatted("No image");
            }
            else
            {
                string tag = snapshots[i]->mTimestampMs != INT64_MIN ? MillisecToString(snapshots[i]->mTimestampMs) : "N/A";
                auto hTx = snapshots[i]->mTextureReady ? snapshots[i]->mhTx : nullptr;
                ImTextureID tid = hTx ? hTx->TextureID() : nullptr;
                if (!tid)
                {
                    ImGui::Dummy(g_snapImageSize);
                    tag += "(loading)";
                }
                else
                {
                    auto roiRect = hTx->GetDisplayRoi();
                    ImGui::Image(tid, g_snapImageSize, roiRect.lt, roiRect.rb);
                }
                ImGui::TextUnformatted(tag.c_str());
            }
            ImGui::EndGroup();
            ImGui::SameLine();
        }

        ImGui::End();
    }

    // open file dialog
    ImVec2 modal_center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 maxSize = ImVec2((float)io.DisplaySize.x, (float)io.DisplaySize.y);
	ImVec2 minSize = maxSize * 0.5f;
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
	{
        if (ImGuiFileDialog::Instance()->IsOk())
		{
            g_ssgen->Close();
            string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_mediaParser = MediaParser::CreateInstance();
            if (g_isImageSequence)
                g_mediaParser->OpenImageSequence({25, 1}, filePathName, ".+_([[:digit:]]{1,})\\.png", false);
            else
            {
                g_mediaParser->Open(filePathName);
                g_mediaParser->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
            }
            g_movr->Open(g_mediaParser, 20);
            g_ssgen->Open(g_mediaParser);
            g_ssgen->SetOverview(g_movr);
            g_windowPos = (float)g_ssgen->GetVideoMinPos()/1000.f;
            g_windowSize = (float)g_ssgen->GetVideoDuration()/10000.f;
            g_ssgen->ConfigSnapWindow(g_windowSize, g_windowFrames);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false))
    {
        app_done = true;
    }
    if (app_will_quit)
    {
        app_done = true;
    }

    g_txmgr->UpdateTextureState();
    // Log(DEBUG) << g_txmgr.get() << endl;
    return app_done;
}

void Application_Setup(ApplicationWindowProperty& property)
{
    property.name = "MediaSnapshotTest";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1280;
    property.height = 720;
    property.application.Application_Initialize = MediaSnapshot_Initialize;
    property.application.Application_Finalize = MediaSnapshot_Finalize;
    property.application.Application_Frame = MediaSnapshot_Frame;
}
