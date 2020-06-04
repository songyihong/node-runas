#include "runas.h"

#include <windows.h>

using namespace std;

namespace runas {

void UTF82ASC(const char* szUTF8, std::string& szAscii) {
  int len = MultiByteToWideChar(CP_UTF8, 0, szUTF8, -1, NULL,0); 
  wchar_t * wszUtf8 = new wchar_t[len+1]; 
  memset(wszUtf8, 0, len * 2 + 2); 
  MultiByteToWideChar(CP_UTF8, 0, szUTF8, -1, wszUtf8, len); 
  len = WideCharToMultiByte(CP_ACP, 0, wszUtf8, -1, NULL, 0, NULL, NULL); 
  char *szTemp = new char[len + 1]; 
  memset(szTemp, 0, len + 1); 
  WideCharToMultiByte (CP_ACP, 0, wszUtf8, -1, szTemp, len, NULL,NULL); 
	
  szAscii = szTemp;
  delete[] szTemp;
  delete[] wszUtf8; 
}

std::string QuoteCmdArg(const std::string& arg) {
  if (arg.size() == 0)
    return arg;

  std::string argAsc;

  // No quotation needed.
  if (arg.find_first_of(" \t\"") == std::string::npos) {
    UTF82ASC(arg.c_str(), argAsc);
    return argAsc;
  }

  // No embedded double quotes or backlashes, just wrap quote marks around
  // the whole thing.
  if (arg.find_first_of("\"\\") == std::string::npos) {
    UTF82ASC((std::string("\"") + argAsc + '"').c_str(), argAsc);
    return argAsc;
  }

  // Expected input/output:
  //   input : hello"world
  //   output: "hello\"world"
  //   input : hello""world
  //   output: "hello\"\"world"
  //   input : hello\world
  //   output: hello\world
  //   input : hello\\world
  //   output: hello\\world
  //   input : hello\"world
  //   output: "hello\\\"world"
  //   input : hello\\"world
  //   output: "hello\\\\\"world"
  //   input : hello world\
  //   output: "hello world\"
  std::string quoted;
  bool quote_hit = true;
  for (size_t i = arg.size(); i > 0; --i) {
    quoted.push_back(arg[i - 1]);

    if (quote_hit && arg[i - 1] == '\\') {
      quoted.push_back('\\');
    } else if (arg[i - 1] == '"') {
      quote_hit = true;
      quoted.push_back('\\');
    } else {
      quote_hit = false;
    }
  }
  UTF82ASC((std::string("\"") + std::string(quoted.rbegin(), quoted.rend()) + '"').c_str(), argAsc);
  return argAsc;
}

bool Runas(const std::string& command,
           const std::vector<std::string>& args,
           const std::string& std_input,
           std::string* std_output,
           std::string* std_error,
           int options,
           int* exit_code) {
  std::string commandAsc;
  UTF82ASC(command.c_str(), commandAsc);
  CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  std::string parameters;
  for (size_t i = 0; i < args.size(); ++i)
    parameters += QuoteCmdArg(args[i]) + ' ';

  SHELLEXECUTEINFOA sei = { sizeof(sei) };
  sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
  sei.lpVerb = (options & OPTION_ADMIN) ? "runas" : "open";
  sei.lpFile = commandAsc.c_str();
  sei.lpParameters = parameters.c_str();
  sei.nShow = SW_NORMAL;

  if (options & OPTION_HIDE)
    sei.nShow = SW_HIDE;

  if (::ShellExecuteExA(&sei) == FALSE || sei.hProcess == NULL)
    return false;

  // Wait for the process to complete.
  ::WaitForSingleObject(sei.hProcess, INFINITE);

  DWORD code;
  if (::GetExitCodeProcess(sei.hProcess, &code) == 0)
    return false;

  *exit_code = code;
  return true;
}

}  // namespace runas
