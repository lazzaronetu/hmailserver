// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#include "StdAfx.h"


#include "../../hMailServer/hMailServer.h"


#include "ScriptServer.h"
#include "ScriptSite.h"
#include "ScriptObjectContainer.h"


#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace HM
{
   ScriptServer::ScriptServer(void) :
      has_on_client_connect_(false),
      has_on_accept_message_(false),
      has_on_deliver_message_(false),
      has_on_backup_completed_(false),
      has_on_backup_failed_(false),
      has_on_delivery_start_(false),
      has_on_error_(false),
      has_on_delivery_failed_(false),
      has_on_external_account_download_(false),
      has_on_smtpdata_(false)
   {
      
   }

   ScriptServer::~ScriptServer(void)
   {
      
   }

   String 
   ScriptServer::GetCurrentScriptFile() const
   {
      String sEventsDir = IniFileSettings::Instance()->GetEventDirectory();

      String sScriptLanguage = Configuration::Instance()->GetScriptLanguage();

      String sExtension;
      if (sScriptLanguage == _T("VBScript"))
         sExtension = "vbs";
      else if (sScriptLanguage == _T("JScript"))
         sExtension = "js";
      else
         sExtension = "";

      String sFileName = sEventsDir + "\\EventHandlers." + sExtension;

      return sFileName;
   }

   void
   ScriptServer::LoadScripts()
   {
      try
      {
         script_language_ = Configuration::Instance()->GetScriptLanguage();

         if (script_language_ == _T("VBScript"))
            script_extension_ = "vbs";
         else if (script_language_ == _T("JScript"))
            script_extension_ = "js";
         else
            script_extension_ = "";

         String sCurrentScriptFile = GetCurrentScriptFile();

         // Load the script file from disk
         script_contents_ = FileUtilities::ReadCompleteTextFile(sCurrentScriptFile);

         // Do a syntax check before loading it.
         String sMessage = ScriptServer::CheckSyntax();
         if (!sMessage.IsEmpty())
         {
            ErrorManager::Instance()->ReportError(ErrorManager::High, 5016, "ScriptServer::LoadScripts", sMessage);
            return;
         }
         
         // Determine which functions are available.
         has_on_client_connect_ = DoesFunctionExist_("OnClientConnect");
         has_on_accept_message_ = DoesFunctionExist_("OnAcceptMessage");
         has_on_deliver_message_ = DoesFunctionExist_("OnDeliverMessage");
         has_on_backup_completed_ = DoesFunctionExist_("OnBackupCompleted");
         has_on_backup_failed_ = DoesFunctionExist_("OnBackupFailed");
         has_on_delivery_start_ = DoesFunctionExist_("OnDeliveryStart");
         has_on_error_ = DoesFunctionExist_("OnError");
         has_on_delivery_failed_ = DoesFunctionExist_("OnDeliveryFailed");
         has_on_external_account_download_ = DoesFunctionExist_("OnExternalAccountDownload");
         has_on_smtpdata_ = DoesFunctionExist_("OnSMTPData");

      }
      catch (...)
      {
         ErrorManager::Instance()->ReportError(ErrorManager::High, 5017, "ScriptServer::LoadScripts", "An exception was thrown when loading scripts.");
      }
   }


   String
   ScriptServer::CheckSyntax()
   {
      String sEventsDir = IniFileSettings::Instance()->GetEventDirectory();      

      String sScriptLanguage = Configuration::Instance()->GetScriptLanguage();
      String sScriptExtension;
      if (sScriptLanguage == _T("VBScript"))
         sScriptExtension = "vbs";
      else if (sScriptLanguage == _T("JScript"))
         sScriptExtension = "js";
      else
         sScriptExtension = "";

      String sErrorMessage;

      // Compile the scripts.
      sErrorMessage = Compile_(sScriptLanguage, sEventsDir + "\\EventHandlers." + sScriptExtension);
      if (!sErrorMessage.IsEmpty())
         return sErrorMessage;

      return String("");
   }

   bool 
   ScriptServer::DoesFunctionExist_(const String &sProcedure)
   {
      // Create an instance of the script engine and execute the script.
      CComObject<CScriptSiteBasic>* pBasic;
      CComObject<CScriptSiteBasic>::CreateInstance(&pBasic);

      CComQIPtr<IActiveScriptSite> spUnk;

      if (!pBasic)
      {
         ErrorManager::Instance()->ReportError(ErrorManager::High, 5017, "ScriptServer::FireEvent", "Failed to create instance of script site.");
         return false;
      }

      spUnk = pBasic; 
      pBasic->Initiate(script_language_, NULL);
      pBasic->AddScript(script_contents_);
      pBasic->Run();
      bool bExists = pBasic->ProcedureExists(sProcedure);
      pBasic->Terminate();
      
      return bExists;
   }


   String
   ScriptServer::Compile_(const String &sLanguage, const String &sFilename)
   {
      String sContents = FileUtilities::ReadCompleteTextFile(sFilename);

      if (sContents.IsEmpty())
         return "";

      // Create an instance of the script engine and execute the script.
      CComObject<CScriptSiteBasic>* pBasic;
      CComObject<CScriptSiteBasic>::CreateInstance(&pBasic);

      CComQIPtr<IActiveScriptSite> spUnk;

      if (!pBasic)
         return "ScriptServer:: Failed to create instance of script site.";

      spUnk = pBasic; // let CComQIPtr tidy up for us
      pBasic->Initiate(sLanguage, NULL);
      // pBasic->SetObjectContainer(pObjects);
      pBasic->AddScript(sContents);
      pBasic->Run();
      pBasic->Terminate();

      String sErrorMessage = pBasic->GetLastError();

      if (!sErrorMessage.IsEmpty())
         sErrorMessage = "File: " + sFilename + "\r\n" + sErrorMessage;

      return sErrorMessage;

   }

   void 
   ScriptServer::FireEvent(Event e,  const String &sEventCaller, shared_ptr<ScriptObjectContainer> pObjects)
   {
      if (!Configuration::Instance()->GetUseScriptServer())
         return;

	  // JDR: stores the name of the method that is fired in the script. http://www.hmailserver.com/forum/viewtopic.php?f=2&t=25497
	  String event_name_ = _T("Unknown");

      switch (e)
      {
      case EventOnClientConnect:
		 event_name_ = _T("OnClientConnect");
         if (!has_on_client_connect_)
            return;
         break;
      case EventOnAcceptMessage:
		 event_name_ = _T("OnAcceptMessage");
         if (!has_on_accept_message_)
            return;
         break;
      case EventOnMessageDeliver:
	     event_name_ = _T("OnMessageDeliver");
         if (!has_on_deliver_message_)
            return;
         break;
      case EventOnBackupCompleted:
		  event_name_ = _T("OnBackupCompleted");
         if (!has_on_backup_completed_)
            return;
         break;
      case EventOnBackupFailed:
		  event_name_ = _T("OnBackupFailed");
         if (!has_on_backup_failed_)
            return;
         break;
      case EventOnError:
		  event_name_ = _T("OnError");
         if (!has_on_error_)
            return;
         break;
      case EventOnDeliveryStart:
		  event_name_ = _T("OnDeliveryStart");
         if (!has_on_delivery_start_)
            return;
         break;
      case EventOnDeliveryFailed:
		  event_name_ = _T("OnDeliveryFailed");
         if (!has_on_delivery_failed_)
            return;
         break;
      case EventOnExternalAccountDownload:
		  event_name_ = _T("OnExternalAccountDownload");
         if (!has_on_external_account_download_)
            return;
         break;
      case EventOnSMTPData:
		  event_name_ = _T("OnSMTPData");
         if (!has_on_smtpdata_)
            return;
         break;

      case EventCustom:
         break;
      default:
         {
            return;
         }
         
      }

	  // JDR: Added event name to the debug log. http://www.hmailserver.com/forum/viewtopic.php?f=2&t=25497
	  LOG_DEBUG("ScriptServer::FireEvent-" + event_name_);

      String sScript;

      // Build the script.
      if (script_language_ == _T("VBScript"))
         sScript = script_contents_ + "\r\n\r\n" + "Call " + sEventCaller + "\r\n";
      else if (script_language_ == _T("JScript"))
         sScript = script_contents_ + "\r\n\r\n" + sEventCaller + ";\r\n";

      CComObject<CScriptSiteBasic>* pBasic;
      CComObject<CScriptSiteBasic>::CreateInstance(&pBasic);
      CComQIPtr<IActiveScriptSite> spUnk;
      
      if (!pBasic)
      {
         ErrorManager::Instance()->ReportError(ErrorManager::High, 5018, "ScriptServer::FireEvent", "Failed to create instance of script site.");
         return;
      }
      
      spUnk = pBasic; // let CComQIPtr tidy up for us
      pBasic->Initiate(script_language_, NULL);
      pBasic->SetObjectContainer(pObjects);
      pBasic->AddScript(sScript);
      pBasic->Run();
      pBasic->Terminate();

      LOG_DEBUG("ScriptServer:~FireEvent");
   }

}