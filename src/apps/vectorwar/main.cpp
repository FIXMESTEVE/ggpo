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

EOS_HPlatform g_EOS_Platform_Handle;
EOS_HConnect g_EOS_Connect_Handle;
EOS_HLobby g_EOS_Lobby_Handle;
EOS_ProductUserId g_EOS_LocalUserId;
bool g_bEOS_Connect_Succeeded = false;

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
_Execute_EOS_Lobby_CreateLobby()
{
    g_EOS_Lobby_Handle = EOS_Platform_GetLobbyInterface(g_EOS_Platform_Handle);

    EOS_Lobby_CreateLobbyOptions createLobbyOptions = {};
    createLobbyOptions.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
    createLobbyOptions.PermissionLevel = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
    createLobbyOptions.LocalUserId = g_EOS_LocalUserId;
    createLobbyOptions.MaxLobbyMembers = 2;
    createLobbyOptions.bAllowInvites = true;
    createLobbyOptions.bDisableHostMigration = false;
    createLobbyOptions.bEnableRTCRoom = false;
    createLobbyOptions.LocalRTCOptions = NULL;
    createLobbyOptions.BucketId = "wtf";
    EOS_Lobby_CreateLobby(g_EOS_Lobby_Handle, &createLobbyOptions, NULL, [](const EOS_Lobby_CreateLobbyCallbackInfo* info) {
        printf("%d", info->ResultCode);
    });
}

void
RunMainLoop(HWND hwnd)
{
   MSG msg = { 0 };
   int start, next, now;

   start = next = now = timeGetTime();
   while(1) {
      EOS_Platform_Tick(g_EOS_Platform_Handle);
      if (g_bEOS_Connect_Succeeded) {
          _Execute_EOS_Lobby_CreateLobby();
      }

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

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <sstream>
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
            MessageBox(NULL,
                L"EOS_Connect_Login succeeded!\n",
                L"Success!", MB_OK);
            g_bEOS_Connect_Succeeded = true;
            g_EOS_LocalUserId = info->LocalUserId;
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

   RunMainLoop(hwnd);
   VectorWar_Exit();
   WSACleanup();
   DestroyWindow(hwnd);
   return 0;
}