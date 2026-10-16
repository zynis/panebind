#pragma once

#include "guest_path_scope.h"
#include <wincrypt.h>
#include <wtsapi32.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace panebind::platform::windows::product::guest {
// This is a narrow experimental-run guard, not product window eligibility.
// File work occurs once on the input owner, never in a hook/resource callback.
struct CaptureScope { bool enabled{};std::string reason{"ordinary_host_original_input_route"},run_id; };
inline std::string capture_read(const std::filesystem::path& path,std::size_t cap=262144){
    const auto attrs=GetFileAttributesW(path.c_str());std::error_code ec;
    if(attrs==INVALID_FILE_ATTRIBUTES||(attrs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
       std::filesystem::file_size(path,ec)>cap||ec)return {};
    std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};
}
inline bool capture_plain_directory(const std::filesystem::path& p){
    const auto a=GetFileAttributesW(p.c_str());std::error_code ec;
    return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)&&!(a&FILE_ATTRIBUTE_REPARSE_POINT)&&
        same_exact_directory(std::filesystem::canonical(p,ec),p)&&!ec;
}
inline std::string capture_field(std::string_view text,std::string_view key){
    const auto token="\""+std::string(key)+"\"";auto at=text.find(token);
    if(at==text.npos||text.find(token,at+token.size())!=text.npos)return {};
    at+=token.size();const auto skip=[&]{while(at<text.size()&&std::isspace(static_cast<unsigned char>(text[at])))++at;};
    skip();if(at==text.size()||text[at++]!=':')return {};skip();if(at==text.size()||text[at++]!='\"')return {};
    const auto begin=at;while(at<text.size()&&text[at]!='\"'){
        if(text[at]=='\\'||static_cast<unsigned char>(text[at])<32)return {};++at;}
    return at<text.size()?std::string(text.substr(begin,at-begin)):std::string{};
}
inline std::string capture_hash(const std::filesystem::path& path){
    HCRYPTPROV provider{};HCRYPTHASH h{};
    if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT|CRYPT_SILENT))return {};
    if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&h)){CryptReleaseContext(provider,0);return {};}
    std::ifstream in(path,std::ios::binary);std::array<char,65536> block{};bool ok=static_cast<bool>(in);
    while(ok&&in){in.read(block.data(),block.size());if(in.gcount()>0)
        ok=CryptHashData(h,reinterpret_cast<const BYTE*>(block.data()),static_cast<DWORD>(in.gcount()),0)!=FALSE;}
    std::array<BYTE,32> digest{};DWORD length=static_cast<DWORD>(digest.size());
    ok=ok&&in.eof()&&CryptGetHashParam(h,HP_HASHVAL,digest.data(),&length,0)&&length==digest.size();
    CryptDestroyHash(h);CryptReleaseContext(provider,0);if(!ok)return {};
    constexpr char digits[]="0123456789ABCDEF";std::string out;for(auto b:digest){out+=digits[b>>4];out+=digits[b&15];}return out;
}
inline bool capture_interactive_medium(){
    HANDLE token{};if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return false;
    alignas(TOKEN_MANDATORY_LABEL) std::array<BYTE,512> data{};DWORD size{};TOKEN_ELEVATION elevation{};
    bool ok=GetTokenInformation(token,TokenIntegrityLevel,data.data(),static_cast<DWORD>(data.size()),&size)&&
        GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&size);CloseHandle(token);
    const auto sid=reinterpret_cast<TOKEN_MANDATORY_LABEL*>(data.data())->Label.Sid;
    if(!ok||!IsValidSid(sid)||!*GetSidSubAuthorityCount(sid)||elevation.TokenIsElevated||
       *GetSidSubAuthority(sid,*GetSidSubAuthorityCount(sid)-1)!=SECURITY_MANDATORY_MEDIUM_RID)return false;
    DWORD session{};if(!ProcessIdToSessionId(GetCurrentProcessId(),&session)||!session)return false;
    LPWSTR state{};DWORD bytes{};ok=WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,session,WTSConnectState,&state,&bytes)&&
        bytes>=sizeof(WTS_CONNECTSTATE_CLASS)&&*reinterpret_cast<WTS_CONNECTSTATE_CLASS*>(state)==WTSActive;
    if(state)WTSFreeMemory(state);if(!ok)return false;
    std::array<wchar_t,128> station{},desktop{},input{};DWORD needed{};
    const auto current=GetThreadDesktop(GetCurrentThreadId());const auto active=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
    ok=active&&GetUserObjectInformationW(GetProcessWindowStation(),UOI_NAME,station.data(),sizeof(station),&needed)&&
       GetUserObjectInformationW(current,UOI_NAME,desktop.data(),sizeof(desktop),&needed)&&
       GetUserObjectInformationW(active,UOI_NAME,input.data(),sizeof(input),&needed)&&
       _wcsicmp(station.data(),L"WinSta0")==0&&_wcsicmp(desktop.data(),L"Default")==0&&_wcsicmp(input.data(),L"Default")==0;
    if(active)CloseDesktop(active);return ok;
}
inline CaptureScope detect_capture_scope(std::string_view source_sha){
    CaptureScope out;try{
        std::array<wchar_t,128> name{};DWORD n=static_cast<DWORD>(name.size());
        if(!GetUserNameW(name.data(),&n)||_wcsicmp(name.data(),L"WDAGUtilityAccount"))return out;
        out.reason="guest_scope_not_verified";
        const std::filesystem::path input=L"C:\\PaneBindV1\\Input",output=L"C:\\PaneBindV1\\Output";
        if(!capture_plain_directory(L"C:\\PaneBindV1")||!capture_plain_directory(input)||!capture_plain_directory(output)||
           !capture_interactive_medium())return out;
        auto run=capture_read(output/L"panebind-v1-output.marker",128);
        if(run.ends_with("\r\n"))run.resize(run.size()-2);else if(run.ends_with('\n'))run.pop_back();
        if(run.size()!=32||!std::all_of(run.begin(),run.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');}))return out;
        const std::wstring wrun(run.begin(),run.end());auto marker=capture_read(input/L"panebind-v1-input.marker",128);
        if(marker.ends_with("\r\n"))marker.resize(marker.size()-2);else if(marker.ends_with('\n'))marker.pop_back();if(marker!=run)return out;
        const auto manifest=capture_read(input/L"manifest.json");const auto files=manifest.find("\"files\"");
        if(files==manifest.npos)return out;const std::string_view identity(manifest.data(),files);
        if(capture_field(identity,"run_id")!=run||capture_field(identity,"product_source_commit")!=source_sha||
           capture_field(identity,"source_commit")!=source_sha||capture_field(identity,"guest_contract")!="panebind-v1-guest-contract/2"||
           capture_field(identity,"product_interface")!="panebind-v1-product-guest-interface/1")return out;
        std::array<wchar_t,32768> image{};const auto len=GetModuleFileNameW(nullptr,image.data(),static_cast<DWORD>(image.size()));
        if(!len||len>=image.size())return out;const std::filesystem::path executable=image.data();
        const auto parent=executable.parent_path();const bool debug=same_exact_directory(parent,output/(wrun+L"-product-Debug"));
        const bool release=same_exact_directory(parent,output/(wrun+L"-product-Release"));
        const bool portable=same_exact_directory(parent,output/(wrun+L"-便携 中文 空格")/L"PaneBindV1");
        if((!debug&&!release&&!portable)||_wcsicmp(executable.filename().c_str(),L"panebind.exe")||!capture_plain_directory(parent))return out;
        std::error_code ec;const auto canonical=std::filesystem::canonical(executable,ec);if(ec||canonical!=executable)return out;
        if(debug&&!capture_plain_directory(input/L"Debug"))return out;
        const auto actual=capture_hash(executable),reference=capture_hash(debug?input/L"Debug"/L"panebind.exe":input/L"panebind.exe");
        if(actual.empty()||actual!=reference||actual!=capture_field(identity,debug?"product_debug_sha256":"product_release_sha256"))return out;
        if(portable){
            if(!capture_plain_directory(parent.parent_path())||capture_read(parent/L"portable.flag",128)!="PaneBind V1 portable candidate\n")return out;
            const auto at=manifest.find("\"portable_test\""),inventory=at==manifest.npos?manifest.npos:manifest.find("\"files\"",at);
            if(at==manifest.npos||inventory==manifest.npos)return out;const std::string_view spec(manifest.data()+at,inventory-at);
            std::istringstream lines(capture_read(output/(wrun+L"-portable-ready.marker"),512));std::array<std::string,6> ready;
            for(auto& line:ready)if(!std::getline(lines,line)||line.find('\r')!=line.npos)return out;std::string extra;if(std::getline(lines,extra))return out;
            if(ready[0]!=run||ready[1].size()!=32||ready[2]!=source_sha||ready[3].size()!=64||ready[4].size()!=64||ready[5]!=actual||
               capture_field(spec,"kind")!="PaneBindReleasePortable"||capture_field(spec,"relative_path")!="PaneBindV1-portable-test.zip"||
               capture_field(spec,"package_id")!=ready[1]||capture_field(spec,"source_commit")!=ready[2]||
               capture_field(spec,"zip_sha256")!=ready[3]||capture_field(spec,"manifest_sha256")!=ready[4]||capture_field(spec,"exe_sha256")!=actual||
               capture_hash(input/L"PaneBindV1-portable-test.zip")!=ready[3]||capture_hash(parent/L"manifest.json")!=ready[4])return out;
        }else if(!capture_read(parent/L"portable.flag",128).starts_with("PaneBind V1 portable test candidate"))return out;
        out.enabled=true;out.run_id=run;out.reason="exact_Medium_disposable_guest_product_run";return out;
    }catch(...){out.reason="guest_scope_query_failed";return out;}
}
} // namespace panebind::platform::windows::product::guest
