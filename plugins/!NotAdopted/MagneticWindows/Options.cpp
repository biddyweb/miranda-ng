#include "MagneticWindowsCore.h"

HANDLE hInitOptionsHook;
TOptions Options = {
		true,
		cDefaultSnapWidth,
		false
};


int CALLBACK OptionsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {

	char str[64];

	switch (msg) {
		case WM_INITDIALOG: {
			TranslateDialogDefault(hwndDlg);
			
			CheckDlgButton(hwndDlg, IDC_CHK_SNAP, Options.DoSnap?BST_CHECKED:BST_UNCHECKED);
			SendDlgItemMessage(hwndDlg, IDC_SLIDER_SNAPWIDTH, TBM_SETRANGE, FALSE, MAKELONG(1,32));
			SendDlgItemMessage(hwndDlg, IDC_SLIDER_SNAPWIDTH, TBM_SETPOS, TRUE, Options.SnapWidth);
			
			wsprintf(str, Translate("%d pix"), Options.SnapWidth);
			SetDlgItemText(hwndDlg, IDC_TXT_SNAPWIDTH, str);
			
			EnableWindow(GetDlgItem(hwndDlg, IDC_SLIDER_SNAPWIDTH), Options.DoSnap);
			EnableWindow(GetDlgItem(hwndDlg, IDC_TXT_SNAPWIDTH), Options.DoSnap);

			CheckDlgButton(hwndDlg, IDC_CHK_SCRIVERWORKAROUND, Options.ScriverWorkAround?BST_CHECKED:BST_UNCHECKED);
			
			break;		
		}
		case WM_HSCROLL: {		
			_snprintf(str, 64, Translate("%d pix"), SendDlgItemMessage(hwndDlg, IDC_SLIDER_SNAPWIDTH, TBM_GETPOS, 0, 0));
			SetDlgItemText(hwndDlg, IDC_TXT_SNAPWIDTH, str);

			SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			break;
		}

		case WM_COMMAND: {
			WORD idCtrl = LOWORD(wParam), wNotifyCode = HIWORD(wParam);

			switch(idCtrl) {
				case IDC_CHK_SNAP: {
					if (wNotifyCode == BN_CLICKED) {
						
					
						EnableWindow(GetDlgItem(hwndDlg, IDC_SLIDER_SNAPWIDTH), IsDlgButtonChecked(hwndDlg, IDC_CHK_SNAP));
						EnableWindow(GetDlgItem(hwndDlg, IDC_TXT_SNAPWIDTH), IsDlgButtonChecked(hwndDlg, IDC_CHK_SNAP));
	
						SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
					}
					break;
				}
				case IDC_CHK_SCRIVERWORKAROUND: {
					if (wNotifyCode == BN_CLICKED) {					
						SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
					}
					break;
				}
			}
			break;
		}

		case WM_NOTIFY: { //Here we have pressed either the OK or the APPLY button.
			switch(((LPNMHDR)lParam)->idFrom) {
				case 0:
					switch (((LPNMHDR)lParam)->code) {
						case PSN_RESET:							
							LoadOptions();
							break;
							
						case PSN_APPLY: {
							Options.DoSnap = (IsDlgButtonChecked(hwndDlg, IDC_CHK_SNAP) == TRUE);
							Options.SnapWidth = SendDlgItemMessage(hwndDlg, IDC_SLIDER_SNAPWIDTH, TBM_GETPOS, 0, 0);
							Options.ScriverWorkAround = (IsDlgButtonChecked(hwndDlg, IDC_CHK_SCRIVERWORKAROUND) == TRUE);

							DBWriteContactSettingByte(NULL, ModuleName, "DoSnap", Options.DoSnap);
							DBWriteContactSettingByte(NULL, ModuleName, "SnapWidth", Options.SnapWidth);
							DBWriteContactSettingByte(NULL, ModuleName, "ScriverWorkAround", Options.ScriverWorkAround);
							
							break;
						} 
					} 
					break;
			} 

			break;
			
		}
		default:

			break;
	}
	return 0;

}

int InitOptions(WPARAM wParam, LPARAM lParam) {
	OPTIONSDIALOGPAGE Opt = { 0 };

	Opt.cbSize = sizeof(Opt);
//	Opt.position = 0;
	Opt.pszTitle = "Magnetic Windows";
	Opt.pfnDlgProc = &OptionsDlgProc;
	Opt.pszTemplate = (char *) MAKEINTRESOURCE(IDD_OPT_MAGNETICWINDOWS);
	Opt.hInstance = hInst;
//	Opt.hIcon = 0;
	Opt.pszGroup = "Customize";
//	Opt.groupPosition = 0;
//	Opt.hGroupIcon = 0;
	Opt.flags = ODPF_BOLDGROUPS;
//	Opt.nIDBottomSimpleControl = 0;
//	Opt.nIDRightSimpleControl = 0;
//	Opt.expertOnlyControls = NULL;
//	Opt.nExpertOnlyControls = 0;

	CallService(MS_OPT_ADDPAGE, wParam, (LPARAM)(&Opt));
	
	return 0;
}

void LoadOptions() {
	Options.DoSnap = DBGetContactSettingByte(NULL, ModuleName, "DoSnap", TRUE);
	Options.SnapWidth = DBGetContactSettingByte(NULL, ModuleName, "SnapWidth", cDefaultSnapWidth);
	Options.ScriverWorkAround = DBGetContactSettingByte(NULL, ModuleName, "ScriverWorkAround", FALSE);
}