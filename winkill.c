/********************************************************************\
*  WINKILL
\********************************************************************/

/*********************  Header Files  *********************/

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>


#define TRAYICONID   0
#define TRAYICONMSG  (WM_USER+1)
#define TRAYICONNAME "TRAYICON"

#define XWINDOWSIZE 50
#define YWINDOWSIZE 34

/*********************  Prototypes  ***********************/

LRESULT WINAPI MainWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT WINAPI AboutDlgProc( HWND, UINT, WPARAM, LPARAM );

/*******************  Global Variables ********************/

HANDLE  ghInstance;
HWND    HWnd;
HCURSOR FindAndKillCursor;
HCURSOR FindAndHideCursor;
HCURSOR FindAndNiceCursor;

#define MAXHIDDENWINDOWS 50
HWND HiddenWindows[MAXHIDDENWINDOWS];
int HiddenWindowsCardinal = 0;


/********************************************************************\
*  Function: HICON GetWindowIcon( HWND hWnd, BOOL bBigIcon)
*                                                         
*   Purpose: Gets the icon of a window
*                                                         
\********************************************************************/
HICON GetWindowIcon( HWND hWnd, BOOL bBigIcon)
{
  return (HICON)SendMessage( hWnd, WM_GETICON, bBigIcon ? ICON_BIG : ICON_SMALL, 0);
}


/**
 *
 */
void AddHiddenWindow( HWND window )
{
  if( HiddenWindowsCardinal >= MAXHIDDENWINDOWS )
    return;

  ShowWindow( window, SW_HIDE );

  if( window == HWnd )
    return;

  HiddenWindows[HiddenWindowsCardinal++] = window;
}

/**
 *
 */
void RemoveHiddenWindow( int index )
{
  int i;
  HWND window = HiddenWindows[index];
  ShowWindow( window, SW_SHOW );

  for( i = index ; i < HiddenWindowsCardinal-1 ; i++ ){
    HiddenWindows[i] = HiddenWindows[i+1];
  }

  HiddenWindowsCardinal--;
}


/**
 *
 */
BOOL HiddingWindow()
{
  return GetAsyncKeyState( VK_CONTROL )&0x8000;
}

/**
 *
 */
BOOL NicingWindow()
{
  return GetAsyncKeyState( VK_SHIFT )&0x8000;
}


/**
 *
 */
BOOL KillingWindow()
{
  return !HiddingWindow() && !NicingWindow();                                        
}

/**
 *
 */
HCURSOR GetFindCursor()
{
  if( HiddingWindow() )
    return FindAndHideCursor;
  
  if( NicingWindow() )
    return FindAndNiceCursor;

  return FindAndKillCursor;
}

/********************************************************************\
*  Function: HWND GetTopWindowFromPoint( POINT* p )
*                                                         
*   Purpose: Finds the top level window parent of a window
*                                                         
\********************************************************************/
HWND GetTopWindowFromPoint( POINT* p )
{
  HWND ret = WindowFromPoint( *p );

  if( ret == NULL )
    return NULL;

  while( GetParent( ret ) != NULL )
    ret = GetParent( ret );

  return ret;
}


/**
 *
 */
void HideWindowUnderCursor( POINT screenPoint )
{
  HWND window = GetTopWindowFromPoint( &screenPoint );
  if( window == NULL )
    return;

  AddHiddenWindow( window );

}


/**
 *
 */
HMENU CreateHiddenWindowsPopupMenu()
{
  int i;
  HMENU menu;
  
  if( HiddenWindowsCardinal == 0 )
    return NULL;

  menu = CreatePopupMenu();

  for( i = 0 ; i < HiddenWindowsCardinal ; i++ ){
    MENUITEMINFO itemInfo;
    char title[101];
    
    GetWindowText( HiddenWindows[i], title, sizeof(title)-1 );

    itemInfo.cbSize = sizeof(itemInfo);
    itemInfo.fMask = MIIM_ID | MIIM_DATA | MIIM_TYPE | MIIM_STATE | MIIM_CHECKMARKS;
    itemInfo.wID = i+1;
    itemInfo.dwItemData = (DWORD)HiddenWindows[i];
    itemInfo.fType = MFT_STRING;
    itemInfo.fState = MFS_ENABLED;
    itemInfo.dwTypeData = title;
    itemInfo.cch = strlen(title);
    itemInfo.hbmpUnchecked = (HBITMAP)GetWindowIcon( HiddenWindows[i], FALSE );
    itemInfo.hbmpChecked = (HBITMAP)GetWindowIcon( HiddenWindows[i], FALSE );

    InsertMenuItem( menu, 0, TRUE, &itemInfo );
  }

  return menu;
}


/**
 *
 */
HMENU CreateNicePopupMenu( DWORD priorityClass )
{
  int i;
  HMENU menu;
  static DWORD priorities[] = {  REALTIME_PRIORITY_CLASS, HIGH_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, IDLE_PRIORITY_CLASS };
  static char* szPriorities[] = { "Real time priority", "High priority", "Normal priority", "Iddle priority" };
  
  menu = CreatePopupMenu();

  for( i = 0 ; i < sizeof(priorities)/sizeof(*priorities) ; i++ ){
    MENUITEMINFO itemInfo;
    
    itemInfo.cbSize = sizeof(itemInfo);
    itemInfo.fMask = MIIM_ID | MIIM_DATA | MIIM_TYPE | MIIM_STATE;
    itemInfo.wID = i+1;
    itemInfo.dwItemData = priorities[i];
    itemInfo.fType = MFT_STRING;
    itemInfo.fState = priorities[i] == priorityClass ? MFS_ENABLED|MFS_CHECKED : MFS_ENABLED;
    itemInfo.dwTypeData = szPriorities[i];
    itemInfo.cch = strlen(itemInfo.dwTypeData);

    InsertMenuItem( menu, 0, TRUE, &itemInfo );
  }

  return menu;
}



/**
 *
 */
void ShowHiddenWindowsMenu( POINT screenPoint )
{

  HMENU menu = CreateHiddenWindowsPopupMenu();
  WORD id;
  DWORD error;

  if( menu == 0 )
    return;

  id = (WORD)TrackPopupMenu( menu, 
                             TPM_BOTTOMALIGN | TPM_RIGHTALIGN | TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTBUTTON,
                             screenPoint.x,
                             screenPoint.y,
                             0,
                             HWnd,
                             0 );

  error = GetLastError();

  if( id == 0 )
    return;
  
  RemoveHiddenWindow( id-1 );

  DestroyMenu( menu );
}







/********************************************************************\
*  Function: void SetTrayIcon( HINSTANCE hInstance, HWND hWnd )
*                                                         
*   Purpose: Sets the tray icon
*                                                         
\********************************************************************/
void SetTrayIcon( HINSTANCE hInstance, HWND hWnd )
{
  NOTIFYICONDATA nid;
  HICON          hIcon;

  hIcon = LoadImage( hInstance, TRAYICONNAME, IMAGE_ICON, 16, 16, 0 );

  nid.cbSize = sizeof(nid);
  nid.hWnd = hWnd;
  nid.uID = TRAYICONID;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage = TRAYICONMSG;
  nid.hIcon = hIcon;
  strcpy( nid.szTip, "Kills any process you can point to" );

  Shell_NotifyIcon( NIM_ADD, &nid );
}


/********************************************************************\
*  Function: void RemoveTrayIcon( HINSTANCE hInstance, HWND hWnd )
*                                                         
*   Purpose: Removes the tray icon
*                                                         
\********************************************************************/
void RemoveTrayIcon( HINSTANCE hInstance, HWND hWnd )
{
  NOTIFYICONDATA nid;

  nid.cbSize = sizeof(nid);
  nid.hWnd = hWnd;
  nid.uID = TRAYICONID;
  nid.uFlags = 0;

  Shell_NotifyIcon( NIM_DELETE, &nid );
}                                            





/********************************************************************\
*  Function: BOOL EnableDebugPriv() 
*                                                         
*   Purpose: Gets debug privilege for the calling process
*                                                         
\********************************************************************/
BOOL EnableDebugPriv() 
{ 
    HANDLE           hToken; 
    LUID             DebugValue; 
    TOKEN_PRIVILEGES tkp; 
    OSVERSIONINFO    verInfo = {0};
    int              lastError;

    //
    // Nothing to do in w95
    //
    verInfo.dwOSVersionInfoSize = sizeof (verInfo); 
    GetVersionEx(&verInfo); 
    if( verInfo.dwPlatformId != VER_PLATFORM_WIN32_NT )
      return TRUE;
 
    // 
    // Retrieve a handle of the access token 
    // 
    if (!OpenProcessToken(GetCurrentProcess(), 
            TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, 
            &hToken)) { 
        return 0; 
    } 
 
    // 
    // Enable the SE_DEBUG_NAME privilege 
    // 
    if (!LookupPrivilegeValue((LPSTR) NULL, 
            SE_DEBUG_NAME, 
            &DebugValue)) { 
        return 0; 
    } 
 
    tkp.PrivilegeCount = 1; 
    tkp.Privileges[0].Luid = DebugValue; 
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
 
    AdjustTokenPrivileges(hToken, 
        0, 
        &tkp, 
        sizeof(TOKEN_PRIVILEGES), 
        (PTOKEN_PRIVILEGES) NULL, 
        (PDWORD) NULL); 
 
    // 
    // The return value of AdjustTokenPrivileges can't be tested 
    // 
    lastError = GetLastError();
    if ( lastError != ERROR_SUCCESS) { 
    } 
 
    return TRUE; 
} 

/********************************************************************\
*  Function: BOOL NiceProcess( DWORD processID )
*                                                         
*   Purpose: Nice a process
*                                                         
\********************************************************************/
BOOL NiceProcess(  DWORD processID )
{
  HANDLE hProcess;
  HMENU menu;
  WORD id;
  DWORD error;
  POINT screenPoint;
  MENUITEMINFO menuItemInfo;
  DWORD priorityClass;


  if( !EnableDebugPriv() )
    return FALSE;

  /* ABRO EL PROCESO */
  hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION , 0, processID );
  if( hProcess == NULL )
    return FALSE;
  priorityClass = GetPriorityClass( hProcess );

  /* CREO EL MENU Y LO MUESTRO */
  menu = CreateNicePopupMenu( priorityClass );
  if( menu == 0 )
    return FALSE;
  GetCursorPos( &screenPoint );
  id = (WORD)TrackPopupMenu( menu, 
                             TPM_BOTTOMALIGN | TPM_RIGHTALIGN | TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTBUTTON,
                             screenPoint.x,
                             screenPoint.y,
                             0,
                             HWnd,
                             0 );

  error = GetLastError();

  if( id == 0 )
    return FALSE;

  /* COJO LA PRIORIDAD SELECCIONADA */
  menuItemInfo.cbSize = sizeof(menuItemInfo);
  menuItemInfo.fMask = MIIM_ID | MIIM_DATA;
  GetMenuItemInfo(menu, id, FALSE, &menuItemInfo );
  priorityClass = menuItemInfo.dwItemData;

  DestroyMenu( menu );


  /* SE LA PONGO AL PROCESO */

  if (!SetPriorityClass( hProcess, priorityClass )) { 
      CloseHandle( hProcess ); 
      return FALSE; 
  } 

  CloseHandle( hProcess ); 
  return TRUE; 
}


/********************************************************************\
*  Function: BOOL KillProcess( DWORD processID )
*                                                         
*   Purpose: Kill a process
*                                                         
\********************************************************************/
BOOL KillProcess( DWORD processID )
{
  HANDLE hProcess;

  if( !EnableDebugPriv() )
    return 0;
  
  hProcess = OpenProcess( PROCESS_TERMINATE, 0, processID );
  if( hProcess == NULL )
    return 0;

  if (!TerminateProcess( hProcess, 1 )) { 
      CloseHandle( hProcess ); 
      return 0; 
  } 

  CloseHandle( hProcess ); 
  return TRUE; 
}



/********************************************************************\
* Function: LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM) *
*                                                                    *
*  Purpose: Processes Application Messages                           *
*                                                                    *
*                                                                    *
*                                                                    *
\********************************************************************/

LRESULT CALLBACK MainWndProc( HWND hWnd, UINT msg, WPARAM wParam,
   LPARAM lParam )
{
   int          ret = 0;
   static char  szWindowTitle[200] = "";
   static int   FindingWindowFlag = 0;
   static int   MovingFlag = 0;
   static POINT MovingPoint;

   switch( msg ) {

/**************************************************************\
*     WM_PAINT:                                                *
\**************************************************************/
      case WM_PAINT:{
         PAINTSTRUCT  ps;
         HDC          hDC;
         RECT         rect;
         HBRUSH       hbr;

         hDC = BeginPaint( hWnd, &ps );

         // Rectangulo blanco
         rect.top = 0;
         rect.left = 0;
         rect.bottom = YWINDOWSIZE;
         rect.right = XWINDOWSIZE;
         hbr = CreateSolidBrush( RGB(255,255,255) );
         FillRect( hDC, &rect, hbr );
         DeleteObject( hbr );

         // Rectangulo gris
         rect.top = 1;
         rect.left = 1;
         rect.bottom = YWINDOWSIZE-1;
         rect.right = XWINDOWSIZE-1;
         hbr = CreateSolidBrush( RGB(0,0,0) );
         FillRect( hDC, &rect, hbr );
         DeleteObject( hbr );

         // Si estoy matando pinto el titulo de la ventana
         if( FindingWindowFlag ){
           LOGFONT lf;
           HFONT   font, oldFont;
           RECT    rect;

           // Consigo la fuente
           lf.lfHeight = 10;
           lf.lfWidth = 4;
           lf.lfEscapement = 0;
           lf.lfOrientation = 0;
           lf.lfWeight = 0;
           lf.lfItalic = FALSE;
           lf.lfUnderline = FALSE;
           lf.lfStrikeOut = FALSE;
           lf.lfCharSet = DEFAULT_CHARSET;
           lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
           lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
           lf.lfQuality = DEFAULT_QUALITY;
           lf.lfPitchAndFamily = FF_DONTCARE;
           strcpy( lf.lfFaceName, "Small Fonts" );
           font = CreateFontIndirect( &lf );

           // Pinto el texto
           rect.left = rect.top = 1;
           rect.bottom = YWINDOWSIZE - 1;
           rect.right = XWINDOWSIZE - 1;

           oldFont = SelectObject( hDC, font );
           SetBkColor( hDC, RGB(0,0,0) );
           SetTextColor( hDC, RGB(255,255,255) );
           DrawText( hDC, szWindowTitle, strlen(szWindowTitle), &rect, DT_CENTER|DT_NOPREFIX|DT_WORDBREAK );
           SelectObject( hDC, oldFont );
           DeleteObject( font );
         }

         //
         if( !FindingWindowFlag ){
           HICON hIcon;
           hIcon = LoadImage( ghInstance, TRAYICONNAME, IMAGE_ICON, 32, 32, 0 );
           DrawIcon( hDC, 1 + (XWINDOWSIZE-1 - 32)/2, 1 + (YWINDOWSIZE-1 - 32)/2, hIcon );
           DeleteObject( hIcon );
         }

         EndPaint( hWnd, &ps );
         break;
      }

/**************************************************************\
*     WM_MOUSEMOVE:                                            *
\**************************************************************/
      case WM_MOUSEMOVE:{
         POINT point;
         HWND  window;

         point.x = MAKEPOINTS(lParam).x;
         point.y = MAKEPOINTS(lParam).y;
         ClientToScreen( hWnd, &point );
         window = GetTopWindowFromPoint( &point );

         // KILLING 
         if( FindingWindowFlag ){
           if( window == NULL ){
             break;
           }

           GetWindowText( window, szWindowTitle, sizeof(szWindowTitle) );
           InvalidateRect( hWnd, NULL, FALSE );
         }

         // MOVING
         if( MovingFlag ){
           MoveWindow( hWnd,
                       point.x - MovingPoint.x,
                       point.y - MovingPoint.y,
                       XWINDOWSIZE,
                       YWINDOWSIZE,
                       TRUE );
         }
         break;
      }

/**************************************************************\
*     WM_KEYDOWN:                                              *
\**************************************************************/
      case WM_KEYDOWN:

         if( FindingWindowFlag )
           SetCursor( GetFindCursor() );

         if( VK_ESCAPE == (int)wParam ){
           ReleaseCapture();
           FindingWindowFlag = 0;
           MovingFlag = 0;
           strcpy( szWindowTitle, "" );
           InvalidateRect( hWnd, NULL, TRUE );
         }

         break;


/**************************************************************\
*     WM_KILLFOCUS:                                            *
\**************************************************************/
      case WM_KILLFOCUS:
         ReleaseCapture();
         FindingWindowFlag = 0;
         MovingFlag = 0;
         strcpy( szWindowTitle, "" );
         InvalidateRect( hWnd, NULL, TRUE );

         break;


/**************************************************************\
*     WM_KEYUP:                                              *
\**************************************************************/
      case WM_KEYUP:
         if( FindingWindowFlag )
           SetCursor( GetFindCursor() );

         break;


/**************************************************************\
*     WM_LBUTTONDOWN:                                            *
\**************************************************************/
      case WM_LBUTTONDOWN:
         if( MovingFlag || FindingWindowFlag )
           break;

         MovingPoint.x = MAKEPOINTS(lParam).x;
         MovingPoint.y = MAKEPOINTS(lParam).y;
         if( GetCapture() != hWnd && hWnd != NULL ){
           SetCapture( hWnd );
         }
         MovingFlag = 1;
         break;

/**************************************************************\
*     WM_LBUTTONUP:                                            *
\**************************************************************/
      case WM_LBUTTONUP:
         if( !MovingFlag )
           break;

         ReleaseCapture();
         MovingFlag = 0;
         break;


/**************************************************************\
*     WM_RBUTTONDOWN:                                          *
\**************************************************************/
      case WM_RBUTTONDOWN:
         if( MovingFlag || FindingWindowFlag )
           break;

         if( GetCapture() != hWnd && hWnd != NULL ){
           SetCapture( hWnd );
         }
         FindingWindowFlag = 1;
         SetCursor( GetFindCursor() );
         break;

/**************************************************************\
*     WM_LBUTTONDBLCLK:                                        *
\**************************************************************/
      case WM_LBUTTONDBLCLK:{
         ShowWindow( hWnd, IsWindowVisible(hWnd) ? SW_HIDE : SW_SHOW );
         break;
      }

/**************************************************************\
*     WM_RBUTTONUP:                                            *
\**************************************************************/
      case WM_RBUTTONUP:{
         POINT point;
         HWND  window;
         DWORD threadID, processID;

         if( !FindingWindowFlag )
           break;

         FindingWindowFlag = 0;

         point.x = MAKEPOINTS(lParam).x;
         point.y = MAKEPOINTS(lParam).y;

         ClientToScreen( hWnd, &point );
         window = GetTopWindowFromPoint( &point );
         if( window == NULL )
           break;

         if( window == HWnd && KillingWindow() ){
           if( MessageBox( HWnd, "Do you really want to kill WinKill?", "Are you sure?", MB_ICONQUESTION|MB_YESNO ) == IDNO )
             break;
         }

         threadID = GetWindowThreadProcessId( window, &processID );

         if( HiddingWindow() )
           HideWindowUnderCursor( point );
         
         if( KillingWindow() )
           KillProcess( processID );

         if( NicingWindow() )
           NiceProcess( processID );

         ReleaseCapture();

         strcpy( szWindowTitle, "" );
         InvalidateRect( hWnd, NULL, TRUE );

         break;
      }

/**************************************************************\
*     TRAYICONMSG                                              *
\**************************************************************/
      case TRAYICONMSG:
	      switch (lParam){

            case WM_LBUTTONDOWN:{
              POINT screenPoint;
              ShowWindow( hWnd, IsWindowVisible(hWnd) ? SW_HIDE : SW_SHOW );
              GetCursorPos( &screenPoint );          
              MoveWindow( hWnd, screenPoint.x-XWINDOWSIZE/2, screenPoint.y-YWINDOWSIZE/2, XWINDOWSIZE, YWINDOWSIZE, TRUE );

              break;
            }

            case WM_RBUTTONDOWN:{
              POINT screenPoint;
              GetCursorPos( &screenPoint );
              ShowHiddenWindowsMenu( screenPoint );
              break;
          }

        }

        break;

/**************************************************************\
*     WM_GETMINMAXINFO                                         *
\**************************************************************/
      case WM_GETMINMAXINFO:{
        MINMAXINFO *mmi;

        mmi = (MINMAXINFO*)lParam;

        mmi->ptMaxSize.x = mmi->ptMaxTrackSize.x = mmi->ptMinTrackSize.x = XWINDOWSIZE;
        mmi->ptMaxSize.y = mmi->ptMaxTrackSize.y = mmi->ptMinTrackSize.y = YWINDOWSIZE;

        ret = 0;
        break;
      }


/**************************************************************\
*     WM_DESTROY: PostQuitMessage() is called                  *
\**************************************************************/

      case WM_DESTROY:
         PostQuitMessage( 0 );
         break;

/**************************************************************\
*     Let the default window proc handle all other messages    *
\**************************************************************/

      default:
         return( DefWindowProc( hWnd, msg, wParam, lParam ));
   }

   return ret;
}







 








/********************************************************************\
*  Function: int PASCAL WinMain(HINSTANCE, HINSTANCE, LPSTR, int)    *
*                                                                    *
*   Purpose: Initializes Application                                 *
*                                                                    *
*  Comments: Register window class, create and display the main      *
*            window, and enter message loop.                         *
*                                                                    *
*                                                                    *
\********************************************************************/

int PASCAL WinMain( HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine,
    int nCmdShow )
{
   WNDCLASS wc;
   MSG msg;
   DWORD style;

   if( !hPrevInstance )
   {
      wc.lpszClassName = "WinKillWindowClass";
      wc.lpfnWndProc = MainWndProc;
      wc.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
      wc.hInstance = hInstance;
      wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
      wc.hCursor = LoadCursor( NULL, IDC_ARROW );
      wc.hbrBackground = (HBRUSH)( COLOR_WINDOW+1 );
      wc.lpszMenuName = 0;
      wc.cbClsExtra = 0;
      wc.cbWndExtra = 0;

      RegisterClass( &wc );
   }

   ghInstance = hInstance;

   style = WS_POPUP;
   

   HWnd = CreateWindowEx( WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                          "WinKillWindowClass",
                          "WinKill",
                          style,
                          -1,
                          -1,
                          XWINDOWSIZE,
                          YWINDOWSIZE,
                          NULL,
                          NULL,
                          hInstance,
                          NULL
   );

   ShowWindow( HWnd, SW_HIDE );

   SetTrayIcon( hInstance, HWnd );

   FindAndKillCursor = LoadCursor( hInstance, "FindAndKillCursor" );
   FindAndHideCursor = LoadCursor( hInstance, "FindAndHideCursor" );
   FindAndNiceCursor = LoadCursor( hInstance, "FindAndNiceCursor" );


   while( GetMessage( &msg, NULL, 0, 0 ) ) {
      TranslateMessage( &msg );
      DispatchMessage( &msg );
   }

   RemoveTrayIcon( hInstance, HWnd );

   return msg.wParam;
}

int main( int argc, char *argv[] )
{
  return WinMain( 0, 0, "", SW_HIDE );
}



