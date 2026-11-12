#ifndef _WIN32_START_DIALOG_H_
#define _WIN32_START_DIALOG_H_

/* The Windows start screen: the IDD_START dialog in VectorWar.rc, shown when
 * vectorwar.exe runs without arguments. Modeless and pumped by the caller so
 * matchmaking can keep it responsive and offer Cancel (see vw_start_form.h). */

#include <windows.h>

#include "vw_start_form.h"

class Win32StartDialog {
public:
   enum Action { kNone = 0, kConnect, kMatchmaking, kQuit, kCancel };

   explicit Win32StartDialog(HINSTANCE instance);
   ~Win32StartDialog();
   bool IsReady() const { return _dialog != NULL; }
   HWND Handle() const { return _dialog; }

   /* Dispatches pending messages, waiting up to wait_ms for one; returns the
    * button chosen since the last call, if any. */
   Action Pump(DWORD wait_ms);

   void ReadForm(VwStartForm *form) const;
   void WriteForm(const VwStartForm &form);
   void SetMessage(const char *text);
   /* While waiting the fields are disabled and Quit becomes Cancel. */
   void SetWaiting(bool waiting);

private:
   static INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam);

   HWND _dialog;
   Action _pending;
   bool _waiting;
};

#endif
