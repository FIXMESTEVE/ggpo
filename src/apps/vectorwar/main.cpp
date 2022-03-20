#include <windows.h>
#include <stdio.h>
#include <functional>
#if defined(_DEBUG)
#   include <crtdbg.h>
#endif
#include "vectorwar.h"
#include "ggpo_perfmon.h"
#include "eos_auth.h"
#include "eos_connect.h"
#include "eos_init.h"
#include "eos_sdk.h"
#include "eos_lobby.h"
#include "eos_logging.h"
#include <string>
#include "../../../eos_secrets.h"

#include <iostream>
#include <sstream>
#include <string>

#define WIN32_LEAN_AND_MEAN


EOS_HPlatform g_EOS_Platform_Handle;
EOS_HConnect g_EOS_Connect_Handle;
EOS_HLobby g_EOS_Lobby_Handle;
EOS_HLobbySearch g_EOS_hLobbySearch;
EOS_ProductUserId g_EOS_LocalUserId;
std::string g_EOS_LobbyId;
bool g_bEOS_Connect_Succeeded = false;
bool g_bEOS_IsInLobby = false;
bool g_bEOS_LobbyCreate_Requested = false;
bool g_bEOS_LobbySearch_Requested = false;
bool g_bMustCreateLobby = false;
bool g_bTriedLaunch = false;
HWND g_hWnd;

void
_Print_Lobby_Members(EOS_LobbyId lobbyId)
{
    EOS_HLobbyDetails lobbyDetails = {};
    EOS_Lobby_CopyLobbyDetailsHandleOptions detailsOptions = {};
    detailsOptions.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
    detailsOptions.LocalUserId = g_EOS_LocalUserId;
    detailsOptions.LobbyId = lobbyId;
    EOS_Lobby_CopyLobbyDetailsHandle(g_EOS_Lobby_Handle, &detailsOptions, &lobbyDetails);

    EOS_LobbyDetails_Info* lobbyDetailsInfo;
    EOS_LobbyDetails_CopyInfoOptions copyOptions = {};
    copyOptions.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;
    EOS_LobbyDetails_CopyInfo(lobbyDetails, &copyOptions, &lobbyDetailsInfo);

    EOS_LobbyDetails_GetMemberCountOptions getMemberCountOptions = {};
    getMemberCountOptions.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST;
    uint32_t memberCount = EOS_LobbyDetails_GetMemberCount(lobbyDetails, &getMemberCountOptions);

    bool tryLaunch = false;
    GGPOPlayer players[GGPO_MAX_SPECTATORS + GGPO_MAX_PLAYERS];
    
    for (uint32_t i = 0; i < memberCount; i++) {
        EOS_LobbyDetails_GetMemberByIndexOptions getMemberByIndexOptions = {};
        getMemberByIndexOptions.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERBYINDEX_API_LATEST;
        getMemberByIndexOptions.MemberIndex = i;
        EOS_ProductUserId userId = EOS_LobbyDetails_GetMemberByIndex(lobbyDetails, &getMemberByIndexOptions);
        std::wostringstream os2_;
        os2_ << L"User number " << std::to_string(i).c_str() << L", ID: " << userId << std::endl;
        OutputDebugString(os2_.str().c_str());

        if (memberCount > 1 && !g_bTriedLaunch) {
            tryLaunch = true;
            //try game launch
            //        GGPOPlayer players[GGPO_MAX_SPECTATORS + GGPO_MAX_PLAYERS];

   //        int i;
   //        for (i = 0; i < num_players; i++) {
   //            const wchar_t* arg = __wargv[offset++];

   //            players[i].size = sizeof(players[i]);
   //            players[i].player_num = i + 1;
   //            if (!_wcsicmp(arg, L"local")) {
   //                players[i].type = GGPO_PLAYERTYPE_LOCAL;
   //                local_player = i;
   //                continue;
   //            }

   //            players[i].type = GGPO_PLAYERTYPE_REMOTE;
   //            if (swscanf_s(arg, L"%[^:]:%hd", wide_ip_buffer, wide_ip_buffer_size, &players[i].u.remote.port) != 2) {
   //                Syntax();
   //                return 1;
   //            }
   //            wcstombs_s(nullptr, players[i].u.remote.ip_address, ARRAYSIZE(players[i].u.remote.ip_address), wide_ip_buffer, _TRUNCATE);
            if (userId == g_EOS_LocalUserId) {
                players[i].size = sizeof(players[i]);
                players[i].type = GGPO_PLAYERTYPE_LOCAL;
            }
            else {
                players[i].size = sizeof(players[i]);
                players[i].type = GGPO_PLAYERTYPE_REMOTE;
                players[i].u.remote._EOS_ProductUserId = userId;
            }
        }
    }


    if (tryLaunch) {
        g_bTriedLaunch = true;
        VectorWar_Init(g_hWnd, 0, memberCount, players, 0, g_EOS_LocalUserId, g_EOS_Platform_Handle);
    }

    EOS_LobbyDetails_Info_Release(lobbyDetailsInfo);
}

LRESULT CALLBACK
MainWindowProc(HWND hwnd,
               UINT uMsg,
               WPARAM wParam,
               LPARAM lParam)
{
   switch (uMsg) {
   case WM_ERASEBKGND:
      return 1;
   case WM_KEYDOWN:
      if (wParam == 'P') {
         ggpoutil_perfmon_toggle();
      } else if (wParam == VK_ESCAPE) {
         VectorWar_Exit();
		 PostQuitMessage(0);
      } else if (wParam >= VK_F1 && wParam <= VK_F12) {
         VectorWar_DisconnectPlayer((int)(wParam - VK_F1));
      }
      return 0;
   case WM_PAINT:
      VectorWar_DrawCurrentFrame();
      ValidateRect(hwnd, NULL);
      return 0;
   case WM_CLOSE:
      PostQuitMessage(0);
      break;
   }
   return CallWindowProc(DefWindowProc, hwnd, uMsg, wParam, lParam);
}

HWND
CreateMainWindow(HINSTANCE hInstance)
{
   HWND hwnd;
   WNDCLASSEX wndclass = { 0 };
   RECT rc;
   int width = 640, height = 480;
   WCHAR titlebuf[128];

   wsprintf(titlebuf, L"(pid:%d) ggpo sdk sample: vector war", GetCurrentProcessId());
   wndclass.cbSize = sizeof(wndclass);
   wndclass.lpfnWndProc = MainWindowProc;
   wndclass.lpszClassName = L"vwwnd";
   RegisterClassEx(&wndclass);
   hwnd = CreateWindow(L"vwwnd",
                       titlebuf,
                       WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                       CW_USEDEFAULT, CW_USEDEFAULT,
                       width, height,
                       NULL, NULL, hInstance, NULL);

   GetClientRect(hwnd, &rc);
   SetWindowPos(hwnd, NULL, 0, 0, width + (width - (rc.right - rc.left)), height + (height - (rc.bottom - rc.top)), SWP_NOMOVE);
   return hwnd;
}

void
_Execute_EOS_Lobby_Search_And_Join_Lobby()
{
    g_bEOS_LobbySearch_Requested = true;

    EOS_Lobby_CreateLobbySearchOptions oCreateLobbySearch = {};
    oCreateLobbySearch.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
    oCreateLobbySearch.MaxResults = 10;
    EOS_Lobby_CreateLobbySearch(g_EOS_Lobby_Handle, &oCreateLobbySearch, &g_EOS_hLobbySearch);

    EOS_LobbySearch_SetParameterOptions oSetParameter = {};
    oSetParameter.ApiVersion = EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;
    oSetParameter.ComparisonOp = EOS_EComparisonOp::EOS_CO_EQUAL;
    EOS_Lobby_AttributeData attributeData = {};
    attributeData.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
    attributeData.Key = "Name";
    attributeData.Value.AsUtf8 = "GGPO Test";
    attributeData.ValueType = EOS_ELobbyAttributeType::EOS_AT_STRING;
    oSetParameter.Parameter = &attributeData;
    EOS_LobbySearch_SetParameter(g_EOS_hLobbySearch, &oSetParameter);

    EOS_LobbySearch_FindOptions oFind = {};
    oFind.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST;
    oFind.LocalUserId = g_EOS_LocalUserId;
    EOS_LobbySearch_Find(g_EOS_hLobbySearch, &oFind, NULL, [](const EOS_LobbySearch_FindCallbackInfo* data) {
        EOS_LobbySearch_GetSearchResultCountOptions oGetSearchResultCount = {};
        oGetSearchResultCount.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
        uint32_t count = EOS_LobbySearch_GetSearchResultCount(g_EOS_hLobbySearch, &oGetSearchResultCount);

        std::wostringstream os2_;
        os2_ << L"EOS_LobbySearch_Find Result Code: " << std::to_string((int)data->ResultCode).c_str() << ". Found " << std::to_string(count).c_str() << L" lobbies." << std::endl;
        OutputDebugString(os2_.str().c_str());

        if (count > 0) { // join lobby
            EOS_HLobbyDetails hLobbyDetails;
            EOS_LobbySearch_CopySearchResultByIndexOptions oCopySearchResultByIndex = {};
            oCopySearchResultByIndex.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
            oCopySearchResultByIndex.LobbyIndex = 0;
            EOS_LobbySearch_CopySearchResultByIndex(g_EOS_hLobbySearch, &oCopySearchResultByIndex, &hLobbyDetails);

            EOS_Lobby_JoinLobbyOptions oJoinLobby = {};
            oJoinLobby.ApiVersion = EOS_LOBBY_JOINLOBBY_API_LATEST;
            oJoinLobby.LocalUserId = g_EOS_LocalUserId;
            oJoinLobby.LobbyDetailsHandle = hLobbyDetails;
            EOS_Lobby_JoinLobby(g_EOS_Lobby_Handle, &oJoinLobby, NULL, [](const EOS_Lobby_JoinLobbyCallbackInfo* data) {
                _Print_Lobby_Members(data->LobbyId);
            });
        }
    });
}

void
_Execute_EOS_Lobby_CreateLobby()
{
    g_bEOS_LobbyCreate_Requested = true;

    EOS_Lobby_AddNotifyLobbyUpdateReceivedOptions addNotifyLobbyUpdateReceivedOptions = {};
    addNotifyLobbyUpdateReceivedOptions.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYUPDATERECEIVED_API_LATEST;
    EOS_Lobby_AddNotifyLobbyUpdateReceived(g_EOS_Lobby_Handle, &addNotifyLobbyUpdateReceivedOptions, NULL, [](const EOS_Lobby_LobbyUpdateReceivedCallbackInfo* info) {
        _Print_Lobby_Members(info->LobbyId);
    });

    EOS_Lobby_CreateLobbyOptions createLobbyOptions = {};
    createLobbyOptions.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
    createLobbyOptions.PermissionLevel = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
    createLobbyOptions.LocalUserId = g_EOS_LocalUserId;
    createLobbyOptions.MaxLobbyMembers = 2;
    createLobbyOptions.bAllowInvites = true;
    createLobbyOptions.bDisableHostMigration = false;
    createLobbyOptions.bEnableRTCRoom = false;
    createLobbyOptions.LocalRTCOptions = NULL;
    createLobbyOptions.BucketId = "wtf"; // What is a Bucket Id?
    EOS_Lobby_CreateLobby(g_EOS_Lobby_Handle, &createLobbyOptions, NULL, [](const EOS_Lobby_CreateLobbyCallbackInfo* info) {
        if (info->ResultCode == EOS_EResult::EOS_Success) {
            g_bEOS_IsInLobby = true;
            g_EOS_LobbyId = std::string(info->LobbyId);
            std::wostringstream os_;
            os_ << L"EOS_Lobby_CreateLobby Result Code: " << std::to_string((int)info->ResultCode).c_str() << std::endl;
            OutputDebugString(os_.str().c_str());

            EOS_Lobby_UpdateLobbyModificationOptions updateLobbyModificationOptions = {};
            EOS_HLobbyModification hLobbyModification;
            updateLobbyModificationOptions.ApiVersion = EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST;
            updateLobbyModificationOptions.LobbyId = info->LobbyId;
            updateLobbyModificationOptions.LocalUserId = g_EOS_LocalUserId;
            EOS_Lobby_UpdateLobbyModification(g_EOS_Lobby_Handle, &updateLobbyModificationOptions, &hLobbyModification);

            EOS_Lobby_AttributeData attributeData = {};
            attributeData.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
            attributeData.Key = "Name";
            attributeData.Value.AsUtf8 = "GGPO Test";
            attributeData.ValueType = EOS_ELobbyAttributeType::EOS_AT_STRING;
            EOS_LobbyModification_AddAttributeOptions oAddAttribute = {};
            oAddAttribute.ApiVersion = EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST;
            oAddAttribute.Attribute = &attributeData;
            oAddAttribute.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
            EOS_LobbyModification_AddAttribute(hLobbyModification, &oAddAttribute);

            EOS_Lobby_UpdateLobbyOptions oUpdateLobby = {};
            oUpdateLobby.ApiVersion = EOS_LOBBY_UPDATELOBBY_API_LATEST;
            oUpdateLobby.LobbyModificationHandle = hLobbyModification;
            EOS_Lobby_UpdateLobby(g_EOS_Lobby_Handle, &oUpdateLobby, NULL, [](const EOS_Lobby_UpdateLobbyCallbackInfo* data) {
                std::wostringstream os3_;
                os3_ << L"EOS_Lobby_UpdateLobby Result Code: " << std::to_string((int)data->ResultCode).c_str() << std::endl;
                OutputDebugString(os3_.str().c_str());
            });

            _Print_Lobby_Members(info->LobbyId);
        }
    });
}

void
RunBeforeMainLoop()
{
    int start, next, now;
    start = next = now = timeGetTime();

    while (!g_bTriedLaunch) {
        now = timeGetTime();

        if (now >= next) {
            EOS_Platform_Tick(g_EOS_Platform_Handle);
            if (g_bEOS_Connect_Succeeded) {
                if (g_bMustCreateLobby && !g_bEOS_IsInLobby && !g_bEOS_LobbyCreate_Requested) {
                    _Execute_EOS_Lobby_CreateLobby();
                }
                else if (!g_bMustCreateLobby && !g_bEOS_LobbySearch_Requested) {
                    _Execute_EOS_Lobby_Search_And_Join_Lobby();
                }
            }
            next = now + (1000 / 60);
            if (g_bEOS_IsInLobby) {
                _Print_Lobby_Members(g_EOS_LobbyId.c_str());
            }
            //Sleep(1);
        }
    }
}

void
RunMainLoop(HWND hwnd)
{
   MSG msg = { 0 };
   int start, next, now;

   start = next = now = timeGetTime();
   while(1) {
      while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
         TranslateMessage(&msg); 
         DispatchMessage(&msg);
         if (msg.message == WM_QUIT) {
            return;
         }
      }
      now = timeGetTime();
      VectorWar_Idle(max(0, next - now - 1));
      if (now >= next) {
         EOS_Platform_Tick(g_EOS_Platform_Handle);

         VectorWar_RunFrame(hwnd);
         next = now + (1000 / 60);
      }
   }
}

void
Syntax()
{
   MessageBox(NULL, 
              L"Syntax: vectorwar.exe <local port> <num players> ('local' | <remote ip>:<remote port>)*\n",
              L"Could not start", MB_OK);
}

void
_Execute_EOS_Init()
{
    EOS_InitializeOptions initOptions = {};
    initOptions.ApiVersion = EOS_INITIALIZE_API_LATEST;
    initOptions.ProductName = "ggpo";
    initOptions.ProductVersion = "0.1";
    EOS_Initialize(&initOptions);

    EOS_Platform_Options platformOptions = {};
    platformOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
    platformOptions.ProductId = __PRODUCT_ID__;
    platformOptions.SandboxId = __SANDBOX_ID__;
    platformOptions.ClientCredentials.ClientId = __CLIENT_ID__;
    platformOptions.ClientCredentials.ClientSecret = __CLIENT_SECRET__;
    platformOptions.EncryptionKey = __ENCRYPTION_KEY__;
    platformOptions.DeploymentId = __DEPLOYMENT_ID__;
    platformOptions.bIsServer = EOS_FALSE;
    g_EOS_Platform_Handle = EOS_Platform_Create(&platformOptions);
    g_EOS_Connect_Handle = EOS_Platform_GetConnectInterface(g_EOS_Platform_Handle);

    EOS_Logging_SetCallback([](const EOS_LogMessage* info) {
        std::wostringstream os_;
        os_ << info->Message << std::endl;
        OutputDebugString(os_.str().c_str());
    });
}

void
_Execute_EOS_Login()
{
    EOS_Connect_LoginOptions loginOptions = {};
    EOS_Connect_Credentials credentials = {};
    EOS_Connect_UserLoginInfo loginInfos = {};

    credentials.Type = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;
    credentials.Token = NULL; // For DeviceID type login, token must be null.
    credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;

    loginInfos.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
    loginInfos.DisplayName = "UnnamedPlayer";

    loginOptions.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
    loginOptions.Credentials = &credentials;
    loginOptions.UserLoginInfo = &loginInfos;
    EOS_Connect_Login(g_EOS_Connect_Handle, &loginOptions, NULL, [](const EOS_Connect_LoginCallbackInfo* info) {
        if (info->ResultCode == EOS_EResult::EOS_Success) {
            g_bEOS_Connect_Succeeded = true;
            g_EOS_LocalUserId = info->LocalUserId;
            g_EOS_Lobby_Handle = EOS_Platform_GetLobbyInterface(g_EOS_Platform_Handle);
        }
    });
}

void
_Execute_EOS_Connect_CreateDeviceId_Then_Login()
{
    EOS_Connect_CreateDeviceIdOptions createDeviceIdOptions = {};
    createDeviceIdOptions.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
    createDeviceIdOptions.DeviceModel = "Windows PC";
    EOS_Connect_CreateDeviceId(g_EOS_Connect_Handle, &createDeviceIdOptions, NULL, [](const EOS_Connect_CreateDeviceIdCallbackInfo* info) {
        if (info->ResultCode == EOS_EResult::EOS_Success || info->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed) {
            // Success!
            _Execute_EOS_Login();
        }
    });
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE,
    _In_ LPWSTR,
    _In_ int)
{
   HWND hwnd = CreateMainWindow(hInstance);
   g_hWnd = hwnd;
   //int offset = 1, local_player = 0;
   WSADATA wd = { 0 };
   //wchar_t wide_ip_buffer[128];
   //unsigned int wide_ip_buffer_size = (unsigned int)ARRAYSIZE(wide_ip_buffer);

   WSAStartup(MAKEWORD(2, 2), &wd);
   POINT window_offsets[] = {
      { 64,  64 },   // player 1
      { 740, 64 },   // player 2
      { 64,  600 },  // player 3
      { 740, 600 },  // player 4
   };
   
#if defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
   _Execute_EOS_Init();
   _Execute_EOS_Connect_CreateDeviceId_Then_Login();

   if (wcscmp(__wargv[1], L"createlobby") == 0) {
       g_bMustCreateLobby = true;
   }

   //{
   //    if (__argc < 3) {
   //        Syntax();
   //        return 1;
   //    }
   //    unsigned short local_port = (unsigned short)_wtoi(__wargv[offset++]);
   //    int num_players = _wtoi(__wargv[offset++]);
   //    if (num_players < 0 || __argc < offset + num_players) {
   //        Syntax();
   //        return 1;
   //    }
   //    if (wcscmp(__wargv[offset], L"spectate") == 0) {
   //        char host_ip[128];
   //        unsigned short host_port;
   //        if (swscanf_s(__wargv[offset + 1], L"%[^:]:%hu", wide_ip_buffer, wide_ip_buffer_size, &host_port) != 2) {
   //            Syntax();
   //            return 1;
   //        }
   //        wcstombs_s(nullptr, host_ip, ARRAYSIZE(host_ip), wide_ip_buffer, _TRUNCATE);
   //        VectorWar_InitSpectator(hwnd, local_port, num_players, host_ip, host_port);
   //    }
   //    else {
   //        GGPOPlayer players[GGPO_MAX_SPECTATORS + GGPO_MAX_PLAYERS];

   //        int i;
   //        for (i = 0; i < num_players; i++) {
   //            const wchar_t* arg = __wargv[offset++];

   //            players[i].size = sizeof(players[i]);
   //            players[i].player_num = i + 1;
   //            if (!_wcsicmp(arg, L"local")) {
   //                players[i].type = GGPO_PLAYERTYPE_LOCAL;
   //                local_player = i;
   //                continue;
   //            }

   //            players[i].type = GGPO_PLAYERTYPE_REMOTE;
   //            if (swscanf_s(arg, L"%[^:]:%hd", wide_ip_buffer, wide_ip_buffer_size, &players[i].u.remote.port) != 2) {
   //                Syntax();
   //                return 1;
   //            }
   //            wcstombs_s(nullptr, players[i].u.remote.ip_address, ARRAYSIZE(players[i].u.remote.ip_address), wide_ip_buffer, _TRUNCATE);
   //        }
   //        // these are spectators...
   //        int num_spectators = 0;
   //        while (offset < __argc) {
   //            players[i].type = GGPO_PLAYERTYPE_SPECTATOR;
   //            if (swscanf_s(__wargv[offset++], L"%[^:]:%hd", wide_ip_buffer, wide_ip_buffer_size, &players[i].u.remote.port) != 2) {
   //                Syntax();
   //                return 1;
   //            }
   //            wcstombs_s(nullptr, players[i].u.remote.ip_address, ARRAYSIZE(players[i].u.remote.ip_address), wide_ip_buffer, _TRUNCATE);
   //            i++;
   //            num_spectators++;
   //        }

   //        if (local_player < sizeof(window_offsets) / sizeof(window_offsets[0])) {
   //            ::SetWindowPos(hwnd, NULL, window_offsets[local_player].x, window_offsets[local_player].y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
   //        }

   //        VectorWar_Init(hwnd, local_port, num_players, players, num_spectators);
   //    }
   //}
   RunBeforeMainLoop();
   RunMainLoop(hwnd);
   VectorWar_Exit();
   WSACleanup();
   DestroyWindow(hwnd);
   return 0;
}