#pragma once

#include <vector>
#include "ExecCmd.h"
#include "VersionCompare.h"
#include "Path.h"
#include "File.h"
#include "DownloadDialogSupport.h"
#include "DownloadDialog.h"
#include "StringUtil.h"

//in tutti i percorsi viene sostituito #APPPATH con il percorso dell'applicazione corrente, #SYSTEMPATH con la directory System del computer e #WINDOWSPATH con la directory di installazione di Windows, #TEMPPATH con la directory temporanea
//in all the paths we replace the string constant #APPPATH with the current path of the application and #SYSTEMPATH with the system directory of the computer, #TEMPPATH with the temp directory

#define c_APPPATH TEXT("#APPPATH")
#define c_SYSTEMPATH TEXT("#SYSTEMPATH")
#define c_WINDOWSPATH TEXT("#WINDOWSPATH")
#define c_TEMPPATH TEXT("#TEMPPATH")

enum component_type
{
	cmd, //shell command
	msi, //windows installer setup
	openfile //ShellExecute call to open a file
};

struct installedcheck
{
	virtual bool IsInstalled() = 0;
};

struct installedcheck_check_registry_value : public installedcheck
{
	CString path; //percorso del registry
	CString fieldname; //nome del campo del registry
	CString fieldvalue; //valore del registry bisogna convertirlo in base al tipo
	CString fieldtype; //tipo del campo nel registry : REG_DWORD (long) o REG_SZ  (string)
	CString comparison; //tipo di comparazione : match (verifica se le due stringhe sono uguali) version (che tratta le due stringhe come versioni e quindi se quella richiesta � minore bisogna installare altrimenti no)

	virtual bool IsInstalled()
	{
		try
		{
			HKEY l_HKey;
			LONG l_result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						path,
						0,
						KEY_READ,
						&l_HKey);

			if (l_result != ERROR_SUCCESS)
				return false;

			if (fieldtype == TEXT("REG_DWORD"))
			{
				DWORD wordValue;
				DWORD l_dwordLen = sizeof(DWORD);
				DWORD l_type = REG_DWORD;
				l_result = RegQueryValueEx(l_HKey,fieldname, NULL, &l_type, (LPBYTE)&wordValue, &l_dwordLen);
				if (l_result != ERROR_SUCCESS)
				{
					RegCloseKey(l_HKey);
					return false;
				}

				RegCloseKey(l_HKey);

				DWORD l_checkValue;
				if (_stscanf(fieldvalue, TEXT("%d"), &l_checkValue) != 1)
				{
					AfxMessageBox(TEXT("Invalid registry value to check expected DWORD."), MB_OK|MB_ICONSTOP);
					return false;
				}

				if (comparison == TEXT("match"))
				{
					if (l_checkValue == wordValue)
						return true;
					else
						return false;
				}
				else if (comparison == TEXT("version"))
				{
					if (l_checkValue <= wordValue)
						return true;
					else
						return false;
				}
				else
				{
					AfxMessageBox(TEXT("Invalid comparison type, expected match or version."), MB_OK|MB_ICONSTOP);
					return false;
				}
			}
			else if (fieldtype == TEXT("REG_SZ"))
			{
				DWORD l_dwordLen = 0;//number of bytes
				DWORD l_type = REG_SZ;
				l_result = RegQueryValueEx(l_HKey,fieldname, NULL, &l_type, NULL, &l_dwordLen);
				if (l_result != ERROR_SUCCESS)
				{
					RegCloseKey(l_HKey);
					return false;
				}

				size_t numberOfChars = l_dwordLen / sizeof(TCHAR);
				TCHAR * charsRegValue = new TCHAR[ numberOfChars + 1 ];
				ZeroMemory(charsRegValue, numberOfChars+1);

				l_result = RegQueryValueEx(l_HKey,fieldname, NULL, &l_type, (LPBYTE)charsRegValue, &l_dwordLen);
				if (l_result != ERROR_SUCCESS)
				{
					RegCloseKey(l_HKey);
					return false;
				}

				RegCloseKey(l_HKey);

				CString registryValue = charsRegValue;
				delete [] charsRegValue;

				if (comparison == TEXT("match"))
				{
					if (fieldvalue == registryValue)
						return true;
					else
						return false;
				}
				else if (comparison == TEXT("version"))
				{
					if ( DVLib::stringVersionCompare(fieldvalue, registryValue) <= 0 )
						return true;
					else
						return false;
				}
				else
				{
					AfxMessageBox(TEXT("Invalid comparison type, expected match or version."), MB_OK|MB_ICONSTOP);
					return false;
				}
			}
			else
			{
				RegCloseKey(l_HKey);

				AfxMessageBox(TEXT("Invalid registry type"), MB_OK|MB_ICONSTOP);
				return false;
			}
		}
		catch(...)
		{
			return false;
		}
	}
};

struct installedcheck_check_file : public installedcheck
{
	CString filename; //percorso del file da cercare
	CString fileversion; //versione del file (se "" non viene verificata la versione ma solo la presenza del file)
	CString comparison; //tipo di comparazione : match (verifica se le due stringhe sono uguali) version (che tratta le due stringhe come versioni e quindi se quella richiesta � minore bisogna installare altrimenti no)

	virtual bool IsInstalled()
	{
		try
		{
			if (DVLib::FileExistsCustom(filename))
			{
				if (fileversion.GetLength()>0)
				{
					CString l_FileVersion = DVLib::GetFileVersionString(filename);

					if (comparison == TEXT("match"))
					{
						if (l_FileVersion == fileversion)
							return true;
						else
							return false;
					}
					else if (comparison == TEXT("version"))
					{
						//se la versione � uguale o maggiore
						if (DVLib::stringVersionCompare(l_FileVersion, fileversion) >= 0)
							return true;
						else
							return false;
					}
					else
					{
						AfxMessageBox(TEXT("Invalid comparison type, expected match or version."), MB_OK|MB_ICONSTOP);
						return false;
					}
				}
				else
					return true;
			}
			else
				return false;
		}
		catch(...)
		{
			return false;
		}
	}
};

struct component
{
	//tipo di componente: cmd, msi, openfile
	component_type type;
	//descrizione
	CString description;
	//filtro che indica il minimo sistema operativo in cui lanciare il componente (estremo escluso)
	CString os_filter_greater;
	//filtro che indica il massimo sistema operativo in cui lanciare il componente (estremo escluso)
	CString os_filter_smaller;
	//filtro che indica il filtro per lingua del sistema operativo (es. Italy = 1040, English - United Kingdom = 2057, English - United States = 1033)
	CString os_filter_lcid;
	//testo da visualizzare a fine installazione (se vuoto non visualizza niente)
	CString installcompletemessage;
	//true per forzare il reboot al termine dell'installazione altrimenti false
	bool mustreboot;

	//classi per gestire la verifica se il componente � installato o no
	std::vector<installedcheck*> installedchecks;

	//Informazioni sull'eventuale download dei componenti (fa riferimento al nodo downloaddialog all'interno di component, se non � presente non viene scaricato nessun componente)
	DVLib::DownloadGroupConfiguration DownloadDialogConfiguration;
	//indica se il componente contiene o meno dei componenti da scaricare (in pratica dice se il nodo downloaddialog � presente o no)
	bool ContainsDownloadComponent;

	//scarica gli eventuali componenti necesari e restituisce true se il download ha avuto successo
	bool DownloadComponents(CWnd * p_Parent)
	{
		try
		{
			if (ContainsDownloadComponent)
				return RunDownloadDialog(DownloadDialogConfiguration, p_Parent);
			else
				return true;
		}
		catch(...)
		{
			return false;
		}
	}

	//virtual bool ExecWait(DWORD * pExitCodes) = 0;

	//funzione virtuale specifica per il tipo di componente
	virtual bool Exec() = 0;

	virtual DWORD GetExitCode() = 0;
	virtual bool IsExecuting() = 0;
	virtual void Init() = 0;

	bool IsInstalled()
	{
		if (installedchecks.size() == 0)
			return false;

		bool l_installed = true;
		for (size_t j = 0; j < installedchecks.size(); j++)
		{
			l_installed = l_installed && installedchecks[j]->IsInstalled();
		}
		return l_installed;
	}


	//variabile che non viene letta dall'xml, ma viene calcolata inizialmente sulla base di !(IsInstalled()) e successivamente potrebbe essere scelta dall'utente
	// indica se il componente verr� eseguito o no
	bool selected;
};

struct process_component : public component
{
	//Informations on the executing process
	PROCESS_INFORMATION m_process_info;

	virtual DWORD GetExitCode()
	{
		DWORD l_ExitCode;
		if (GetExitCodeProcess(m_process_info.hProcess, &l_ExitCode))
		{
			return l_ExitCode;
		}
		else
		{
			_ASSERT(false);
			throw -1;
		}
	}
	virtual bool IsExecuting()
	{
		DWORD l_ExitCode;
		if (GetExitCodeProcess(m_process_info.hProcess, &l_ExitCode))
		{
			if (l_ExitCode == STILL_ACTIVE)
				return true;
			else
				return false;
		}
		else
			return false;
	}
	virtual void Init()
	{
		ZeroMemory(&m_process_info, sizeof(PROCESS_INFORMATION));
	}
};

struct cmd_component : public process_component
{
	CString command;

/*	virtual bool ExecWait(DWORD * pExitCodes)
	{
		return DVLib::ExecCmdAndWait(command, pExitCodes);
	};*/

	virtual bool Exec()
	{
		return DVLib::ExecCmd(command, &m_process_info);
	};
};

struct msi_component : public process_component
{
	CString package;
	CString cmdparameters; // es. "/qn REBOOT=ReallySuppress"

	// msi install cmd-line
	//const TCHAR g_tszMsiCmdLine[] = _T("Msiexec /I %s REBOOT=ReallySuppress");

	/*virtual bool ExecWait(DWORD * pExitCodes)
	{
		CString l_command = "msiexec /I ";
		l_command += "\"";
		l_command += package;
		l_command += "\"";
		l_command += " ";
		l_command += cmdparameters;
		//if (silent)
		//	l_command += " /qn";
		//if (suppressreboot)
		//	l_command += " REBOOT=ReallySuppress";

		return DVLib::ExecCmdAndWait(l_command, pExitCodes);
	};*/

	virtual bool Exec()
	{
		CString l_command = TEXT("msiexec /I ");
		l_command += TEXT("\"");
		l_command += package;
		l_command += TEXT("\"");
		l_command += TEXT(" ");
		l_command += cmdparameters;
		//if (silent)
		//	l_command += " /qn";
		//if (suppressreboot)
		//	l_command += " REBOOT=ReallySuppress";

		return DVLib::ExecCmd(l_command, &m_process_info);
	};
};

//open a file with a shellexecute call
struct openfile_component : public component
{
	CString file; //file to open. Can be a web link or a standard file

	virtual bool Exec()
	{
		return DVLib::ShellExecuteDefault(file);
	}
	virtual DWORD GetExitCode()
	{
		return ERROR_SUCCESS;
	}
	virtual bool IsExecuting()
	{
		return false;
	}
	virtual void Init()
	{
	}
};


struct installerSetting
{
	CString dialog_caption;
	CString dialog_message;
	CString dialog_bitmap;
	CString install_caption;
	CString cancel_caption;
	CString status_installed;
	CString status_notinstalled;
	CString failed_exec_command_continue;
	CString installation_completed;
	CString reboot_required;
	CString installing_component_wait;
	//gestione component selector
	CString advanced_caption;
	CString dialog_selector_caption;
	CString dialog_selector_message;
	CString dialog_selector_ok;
	CString dialog_selector_cancel;
	//Link properties
	CString dialog_otherinfo_caption;
	CString dialog_otherinfo_link;

	//Complete command (executed when all components are installed correctly), can be any executable, document or web page valid for ShellExecute API. Usually is a readme file, a web page or a startup file. If empty no command is executed. (OPTIONAL)
	CString complete_command;
	//If true auto close the dialog (display installation_completed message and execute the complete_command) if all the components are already installed. (REQUIRED)
	bool auto_close_if_installed;

	void ExecuteCompleteCode()
	{
		if (installation_completed.Trim().GetLength() > 0)
			AfxMessageBox( installation_completed, MB_OK|MB_ICONINFORMATION);

		if (complete_command.Trim().GetLength() > 0)
			DVLib::ShellExecuteDefault(complete_command);
	}

	std::vector<component*> components;
};

// HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\RunOnce
#define c_dotNetInstaller TEXT("dotNetInstallerBoot")
//#define c_RunOnce "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce"
#define c_Reg_Run TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run")

inline void InsertRegistryRun()
{
	try
	{
		HKEY l_HKey;
		LONG l_result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					c_Reg_Run,
					0,
					KEY_WRITE,
					&l_HKey);
		if (l_result != ERROR_SUCCESS)
			return;

		CString l_ExeName = DVLib::GetAppFullName();
		RegSetValueEx(l_HKey, c_dotNetInstaller,0,REG_SZ,(LPBYTE)(LPCTSTR)l_ExeName, (DWORD)(l_ExeName.GetLength()+1) * sizeof(TCHAR) );

		RegCloseKey(l_HKey);
	}
	catch(...)
	{
		_ASSERT(false);
	}
};

inline void RemoveRegistryRun()
{
	try
	{
		HKEY l_HKey;
		LONG l_result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					c_Reg_Run,
					0,
					KEY_WRITE,
					&l_HKey);
		if (l_result != ERROR_SUCCESS)
			return;

		RegDeleteValue(l_HKey, c_dotNetInstaller);

		RegCloseKey(l_HKey);
	}
	catch(...)
	{
		_ASSERT(false);
	}
};

// ==========================================================================
// InitiateReboot()
//
// Purpose: initiates a system reboot
//
// ==========================================================================
inline BOOL InitiateReboot()
{
    HANDLE hToken;              // handle to process token 
    TOKEN_PRIVILEGES tkp;       // pointer to token structure     
     
    try 
    {
        // Get the current process token handle so we can get shutdown 
        // privilege. 
         
        if (!OpenProcessToken(GetCurrentProcess(), 
                TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        {
           // return FALSE;
        }
         
        // Get the LUID for shutdown privilege. 
         
        LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, 
                &tkp.Privileges[0].Luid); 
         
        tkp.PrivilegeCount = 1;  // one privilege to set    
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
         
        // Get shutdown privilege for this process. 
         
        AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, 
            (PTOKEN_PRIVILEGES) NULL, 0); 
         
        // Cannot test the return value of AdjustTokenPrivileges. 
         
        if (GetLastError() != ERROR_SUCCESS) 
        {
           // return FALSE;
        }
    }

    catch (...)
    {
    }
    return ExitWindowsEx( EWX_REBOOT, 0);
}

inline CString ValidatePath(LPCTSTR p_Path)
{
	//ApplicationPath
	CString l_CurrentPath = DVLib::GetAppPath();

	//SystemPath
	TCHAR l_bufferSystem[MAX_PATH+1];
	ZeroMemory(l_bufferSystem,MAX_PATH+1);
	GetSystemDirectory(l_bufferSystem, MAX_PATH+1);

	//WindowsPath
	TCHAR l_bufferWindows[MAX_PATH+1];
	ZeroMemory(l_bufferWindows,MAX_PATH+1);
	GetWindowsDirectory(l_bufferWindows, MAX_PATH+1);

	//TempPath
	TCHAR l_bufferTempPath[MAX_PATH+1];
	ZeroMemory(l_bufferTempPath,MAX_PATH+1);
	GetTempPath(MAX_PATH+1, l_bufferTempPath);

	CString tmp = p_Path;
	tmp.Replace(c_APPPATH, l_CurrentPath);
	tmp.Replace(c_SYSTEMPATH, l_bufferSystem);
	tmp.Replace(c_WINDOWSPATH, l_bufferWindows);
	tmp.Replace(c_TEMPPATH, l_bufferTempPath);

	return tmp;
}


//inline CString ValidatePathUTF8(const char * strUtf8Path)
//{
//	return ValidatePath(DVLib::UTF8string2Tstring(strUtf8Path).data());
//}