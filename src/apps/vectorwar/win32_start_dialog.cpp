#include "win32_start_dialog.h"

#include "Resource.h"

namespace {

const int kFields[] = {
   IDC_START_LOCAL_PORT, IDC_START_PLAYER_NUMBER, IDC_START_REMOTE,
   IDC_START_SERVER, IDC_START_PLAYERS, IDC_START_LOBBY
};

}  // namespace

Win32StartDialog::Win32StartDialog(HINSTANCE instance) :
   _dialog(NULL), _pending(kNone), _waiting(false)
{
   _dialog = CreateDialogParamA(instance, MAKEINTRESOURCEA(IDD_START), NULL, DialogProc,
                                (LPARAM)this);
   if (_dialog) {
      VwStartForm form;
      VwStartFormDefaults(&form);
      WriteForm(form);
      ShowWindow(_dialog, SW_SHOW);
      SetFocus(GetDlgItem(_dialog, IDC_START_REMOTE));
   }
}

Win32StartDialog::~Win32StartDialog()
{
   if (_dialog) DestroyWindow(_dialog);
}

INT_PTR CALLBACK
Win32StartDialog::DialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
   if (message == WM_INITDIALOG) {
      SetWindowLongPtrA(dialog, DWLP_USER, (LONG_PTR)lparam);
      return FALSE;   /* focus is set by the constructor */
   }
   Win32StartDialog *self = (Win32StartDialog *)GetWindowLongPtrA(dialog, DWLP_USER);
   if (!self) return FALSE;
   switch (message) {
   case WM_COMMAND:
      if (HIWORD(wparam) != BN_CLICKED) return FALSE;
      switch (LOWORD(wparam)) {
      case IDC_START_CONNECT:
         if (!self->_waiting) self->_pending = kConnect;
         return TRUE;
      case IDC_START_MATCHMAKING:
         if (!self->_waiting) self->_pending = kMatchmaking;
         return TRUE;
      case IDCANCEL:      /* Quit button, Esc, or the close box */
         self->_pending = self->_waiting ? kCancel : kQuit;
         return TRUE;
      }
      return FALSE;
   case WM_CLOSE:
      self->_pending = self->_waiting ? kCancel : kQuit;
      return TRUE;
   }
   return FALSE;
}

Win32StartDialog::Action
Win32StartDialog::Pump(DWORD wait_ms)
{
   if (wait_ms) MsgWaitForMultipleObjects(0, NULL, FALSE, wait_ms, QS_ALLINPUT);
   MSG msg;
   while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
         _pending = _waiting ? kCancel : kQuit;
         break;
      }
      if (!_dialog || !IsDialogMessageA(_dialog, &msg)) {
         TranslateMessage(&msg);
         DispatchMessageA(&msg);
      }
   }
   const Action action = _pending;
   _pending = kNone;
   return action;
}

void
Win32StartDialog::ReadForm(VwStartForm *form) const
{
   VwStartFormDefaults(form);
   GetDlgItemTextA(_dialog, IDC_START_LOCAL_PORT, form->local_port, sizeof(form->local_port));
   GetDlgItemTextA(_dialog, IDC_START_PLAYER_NUMBER, form->player_number, sizeof(form->player_number));
   GetDlgItemTextA(_dialog, IDC_START_REMOTE, form->remote, sizeof(form->remote));
   GetDlgItemTextA(_dialog, IDC_START_SERVER, form->server, sizeof(form->server));
   GetDlgItemTextA(_dialog, IDC_START_PLAYERS, form->players, sizeof(form->players));
   GetDlgItemTextA(_dialog, IDC_START_LOBBY, form->lobby, sizeof(form->lobby));
}

void
Win32StartDialog::WriteForm(const VwStartForm &form)
{
   SetDlgItemTextA(_dialog, IDC_START_LOCAL_PORT, form.local_port);
   SetDlgItemTextA(_dialog, IDC_START_PLAYER_NUMBER, form.player_number);
   SetDlgItemTextA(_dialog, IDC_START_REMOTE, form.remote);
   SetDlgItemTextA(_dialog, IDC_START_SERVER, form.server);
   SetDlgItemTextA(_dialog, IDC_START_PLAYERS, form.players);
   SetDlgItemTextA(_dialog, IDC_START_LOBBY, form.lobby);
}

void
Win32StartDialog::SetMessage(const char *text)
{
   SetDlgItemTextA(_dialog, IDC_START_STATUS, text ? text : "");
}

void
Win32StartDialog::SetWaiting(bool waiting)
{
   _waiting = waiting;
   for (size_t i = 0; i < sizeof(kFields) / sizeof(kFields[0]); ++i) {
      EnableWindow(GetDlgItem(_dialog, kFields[i]), !waiting);
   }
   EnableWindow(GetDlgItem(_dialog, IDC_START_CONNECT), !waiting);
   EnableWindow(GetDlgItem(_dialog, IDC_START_MATCHMAKING), !waiting);
   SetDlgItemTextA(_dialog, IDCANCEL, waiting ? "Cancel" : "Quit");
}
