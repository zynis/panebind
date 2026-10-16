#pragma once

// Guest driver only: one bounded, handle-stable physical product-log snapshot.
// Never use path metadata followed by a second path open across log rotation.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace panebind::platform::windows::product::guest {

enum class ProductLogPhysicalState { Ready, Missing, Failure };
struct ProductLogPhysicalRead {
    ProductLogPhysicalState state{ProductLogPhysicalState::Failure};
    std::string bytes;
    const char* stage{"open"};
    DWORD win32_error{};
    std::uint64_t actual_size{};
    bool actual_size_known{};
};

class ProductLogPhysicalFile final {
public:
    explicit ProductLogPhysicalFile(const std::filesystem::path& path){
        file_=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
            nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        if(file_==INVALID_HANDLE_VALUE){
            opening_.win32_error=GetLastError();
            if(opening_.win32_error==ERROR_FILE_NOT_FOUND||opening_.win32_error==ERROR_PATH_NOT_FOUND)
                opening_.state=ProductLogPhysicalState::Missing;
            return;
        }
        SetLastError(ERROR_SUCCESS);
        if(GetFileType(file_)!=FILE_TYPE_DISK){
            opening_.stage="file_type";opening_.win32_error=GetLastError();return;
        }
        BY_HANDLE_FILE_INFORMATION info{};
        if(!GetFileInformationByHandle(file_,&info)){
            opening_.stage="handle_attributes";opening_.win32_error=GetLastError();return;
        }
        if(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)){
            opening_.stage="non_plain_handle_attributes";return;
        }
        opening_.state=ProductLogPhysicalState::Ready;
        opening_.stage="ready";
    }
    ProductLogPhysicalFile(const ProductLogPhysicalFile&)=delete;
    ProductLogPhysicalFile& operator=(const ProductLogPhysicalFile&)=delete;
    ~ProductLogPhysicalFile(){if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);}

    [[nodiscard]] bool opened_plain_file()const noexcept{return opening_.state==ProductLogPhysicalState::Ready;}
    [[nodiscard]] ProductLogPhysicalRead snapshot(std::size_t cap=2*1024*1024){
        if(!opened_plain_file())return opening_;
        ProductLogPhysicalRead result;result.stage="size";
        LARGE_INTEGER size{};
        if(!GetFileSizeEx(file_,&size)){result.win32_error=GetLastError();return result;}
        if(size.QuadPart<0){result.stage="negative_size";return result;}
        result.actual_size_known=true;result.actual_size=static_cast<std::uint64_t>(size.QuadPart);
        if(result.actual_size>cap||result.actual_size>MAXDWORD){result.stage="size_limit";return result;}
        result.stage="seek";
        LARGE_INTEGER begin{};
        if(!SetFilePointerEx(file_,begin,nullptr,FILE_BEGIN)){result.win32_error=GetLastError();return result;}
        result.bytes.resize(static_cast<std::size_t>(result.actual_size));
        if(result.actual_size){
            DWORD read{};result.stage="read";
            if(!ReadFile(file_,result.bytes.data(),static_cast<DWORD>(result.actual_size),&read,nullptr)){
                result.win32_error=GetLastError();result.bytes.clear();return result;
            }
            if(read!=result.actual_size){result.stage="read_short";result.bytes.clear();return result;}
        }
        result.state=ProductLogPhysicalState::Ready;result.stage="ready";return result;
    }
private:
    HANDLE file_{INVALID_HANDLE_VALUE};
    ProductLogPhysicalRead opening_{};
};

}
