#pragma once

// TEST ONLY. One read-only observation after the existing shell-ready failure,
// before ordinary close. No extra launch, input, readiness wait or authority.
// Included after MediumTerminalClientObservation and exact-Run fixture facts.
bool medium_terminal_observe_ready_failure(MediumTerminalClientObservation& out,
    const std::wstring& run,const std::string& configuration,HDESK desktop,HWND source,
    DWORD pid,DWORD tid,HANDLE process,std::uint64_t birth,
    const std::function<bool()>& exact_active_foreground,ULONGLONG caller_deadline){
    const auto began=GetTickCount64();const auto end=(std::min)(caller_deadline,began+500);
    bool cleanup_known=true;const char* stage="admission";
    const auto current=[&]{return GetTickCount64()<end&&exact_active_foreground&&exact_active_foreground();};
    const auto bound=[&]{
        if(!current()||!desktop||!source||!pid||!tid||!process||!birth||
            GetProcessId(process)!=pid||process_birth(process)!=birth||WaitForSingleObject(process,0)!=WAIT_TIMEOUT)return false;
        const auto first=MediumDesktopWindows::read(desktop);const auto one=MediumDesktopWindowFact::read(source,pid,process,birth,L"PaneBindV1_"+run);
        const auto second=MediumDesktopWindows::read(desktop);const auto two=MediumDesktopWindowFact::read(source,pid,process,birth,L"PaneBindV1_"+run);
        return first.complete&&second.complete&&
            std::find(first.windows.begin(),first.windows.end(),source)!=first.windows.end()&&
            std::find(second.windows.begin(),second.windows.end(),source)!=second.windows.end()&&
            one.readonly_same(two)&&current()&&GetForegroundWindow()==source;
    };
    try{
        if((configuration!="Debug"&&configuration!="Release")||run.size()!=32||
            run.find_first_not_of(L"0123456789abcdef")!=std::wstring::npos||!bound())
            throw std::runtime_error("fresh_exact_active_Run_source_required_observation_skipped");
        out.emit("medium_terminal_ready_failure_observation_admission","\"source\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(source))+",\"PID\":"+std::to_string(pid)+",\"TID\":"+
            std::to_string(tid)+",\"birth\":"+std::to_string(birth)+",\"before_tick\":"+std::to_string(began)+
            ",\"observation_deadline_tick\":"+std::to_string(end)+",\"additional_budget_ms\":0,\"within_existing_Run_deadline\":true,\"two_complete_held_Run_memberships\":true,\"input_sent\":false");

        // A single bounded process snapshot. Parent edges require retained
        // handles and compatible birth/exit times, not a process-name guess.
        // Unknown/query failures and truncation forbid any absence conclusion.
        stage="held_launch_tree";
        struct Node {DWORD pid{},parent{};std::uint64_t birth{};HANDLE handle{};bool owned{};};
        struct Tree {
            bool& known;std::vector<Node> nodes;HANDLE snapshot{INVALID_HANDLE_VALUE};bool closed{true};
            void finish()noexcept{for(auto& n:nodes)if(n.owned&&n.handle){const auto h=n.handle;n.handle=nullptr;
                    if(!CloseHandle(h))closed=false;}
                if(snapshot!=INVALID_HANDLE_VALUE){const auto h=snapshot;snapshot=INVALID_HANDLE_VALUE;if(!CloseHandle(h))closed=false;}
                known=known&&closed;}
            ~Tree(){finish();}
        } tree{cleanup_known};
        tree.nodes.reserve(32);tree.nodes.push_back({pid,0,birth,process,false});
        std::vector<PROCESSENTRY32W> entries;entries.reserve(256);SetLastError(ERROR_SUCCESS);
        tree.snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);const DWORD snapshot_error=tree.snapshot==INVALID_HANDLE_VALUE?GetLastError():0;
        const bool snapshot_return=tree.snapshot!=INVALID_HANDLE_VALUE;
        bool enumerated=false,truncated=false;DWORD enumeration_error{};unsigned queries{},query_failures{},rejected_edges{};
        std::string rows="[";unsigned row_count{};std::vector<DWORD> attempted;attempted.reserve(64);
        if(tree.snapshot!=INVALID_HANDLE_VALUE&&current()){
            PROCESSENTRY32W entry{sizeof(entry)};SetLastError(ERROR_SUCCESS);bool more=Process32FirstW(tree.snapshot,&entry)!=FALSE;
            if(!more)enumeration_error=GetLastError();
            while(more){
                if(entries.size()>=4096||!current()){truncated=true;break;}entries.push_back(entry);SetLastError(ERROR_SUCCESS);
                more=Process32NextW(tree.snapshot,&entry)!=FALSE;if(!more)enumeration_error=GetLastError();
            }
            enumerated=!truncated&&enumeration_error==ERROR_NO_MORE_FILES;
            for(unsigned pass=0;pass<8&&current();++pass){bool added=false;
                for(const auto& candidate:entries){
                    if(!current()||tree.nodes.size()>=32||queries>=64){truncated=true;break;}
                    if(candidate.th32ProcessID==GetCurrentProcessId()||std::any_of(tree.nodes.begin(),tree.nodes.end(),[&](const auto& n){return n.pid==candidate.th32ProcessID;}))continue;
                    const auto parent=std::find_if(tree.nodes.begin(),tree.nodes.end(),[&](const auto& n){return n.pid==candidate.th32ParentProcessID;});
                    if(parent==tree.nodes.end()||std::find(attempted.begin(),attempted.end(),candidate.th32ProcessID)!=attempted.end())continue;
                    attempted.push_back(candidate.th32ProcessID);++queries;SetLastError(ERROR_SUCCESS);
                    struct ChildHandle {HANDLE value{};bool& known;~ChildHandle(){if(value&&!CloseHandle(value))known=false;}};
                    ChildHandle child_owner{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,candidate.th32ProcessID),cleanup_known};
                    HANDLE child=child_owner.value;
                    const DWORD open_error=child?0:GetLastError();std::uint64_t child_birth{};bool identity{};
                    FILETIME pb{},pe{},pk{},pu{};bool parent_times{};
                    if(child){child_birth=process_birth(child);parent_times=GetProcessTimes(parent->handle,&pb,&pe,&pk,&pu)!=FALSE;
                        const auto parent_exit=(static_cast<std::uint64_t>(pe.dwHighDateTime)<<32)|pe.dwLowDateTime;
                        identity=GetProcessId(child)==candidate.th32ProcessID&&child_birth&&child_birth>=birth&&
                            child_birth>=parent->birth&&process_birth(parent->handle)==parent->birth&&parent_times&&
                            (!parent_exit||child_birth<=parent_exit);}
                    if(!child||!child_birth||!parent_times)++query_failures;
                    else if(!identity)++rejected_edges;
                    if(row_count++)rows+=",";
                    rows+="{\"snapshot_PID\":"+std::to_string(candidate.th32ProcessID)+",\"snapshot_parent_PID\":"+
                        std::to_string(candidate.th32ParentProcessID)+",\"OpenProcess_return\":"+(child?"true":"false")+
                        ",\"OpenProcess_error\":"+(child?"null":std::to_string(open_error))+
                        ",\"actual_birth\":"+(child_birth?std::to_string(child_birth):"null")+
                        ",\"held_parent_birth_verified_edge\":"+(child?(identity?"true":"false"):"null");
                    if(identity){
                        const auto image=medium_desktop_image(child);DWORD session{},exit_code{},token_error{};int rid=-1;bool elevated{};
                        const bool session_known=ProcessIdToSessionId(candidate.th32ProcessID,&session)!=FALSE;
                        SetLastError(ERROR_SUCCESS);const bool exit_known=GetExitCodeProcess(child,&exit_code)!=FALSE;const auto exit_error=exit_known?0:GetLastError();
                        const bool medium_value=medium(child,&rid,&elevated,&token_error);const bool token_known=rid>=0;
                        const auto waited=WaitForSingleObject(child,0);
                        rows+=",\"actual_image\":"+(image.empty()?std::string("null"):quote(utf8(image)))+
                            ",\"actual_session\":"+(session_known?std::to_string(session):"null")+
                            ",\"integrity_RID\":"+(token_known?std::to_string(rid):"null")+
                            ",\"elevated\":"+(token_known?(elevated?"true":"false"):"null")+
                            ",\"Medium_non_elevated\":"+(token_known?(medium_value?"true":"false"):"null")+
                            ",\"token_query_error\":"+(token_known?"null":std::to_string(token_error))+
                            ",\"process_wait\":"+std::to_string(waited)+",\"GetExitCodeProcess_return\":"+(exit_known?"true":"false")+
                            ",\"exit_code\":"+(exit_known?std::to_string(exit_code):"null")+
                            ",\"exit_query_error\":"+(exit_known?"null":std::to_string(exit_error));
                        tree.nodes.push_back({candidate.th32ProcessID,candidate.th32ParentProcessID,child_birth,child,true});child_owner.value=nullptr;child=nullptr;added=true;
                    }
                    rows+="}";if(child){const bool closed=CloseHandle(child)!=FALSE;child_owner.value=nullptr;if(!closed)cleanup_known=false;}
                }
                if(!added||truncated)break;if(pass==7)truncated=true;
            }
        }
        if(!current())truncated=true;
        rows+="]";tree.finish();cleanup_known=cleanup_known&&tree.closed;
        out.emit("medium_terminal_ready_failure_held_launch_tree","\"snapshot_attempted\":true,\"snapshot_return\":"+
            std::string(snapshot_return?"true":"false")+",\"snapshot_error\":"+(snapshot_return?"null":std::to_string(snapshot_error))+
            ",\"Process32_enumeration_complete\":"+(enumerated?"true":"false")+",\"enumeration_error\":"+std::to_string(enumeration_error)+
            ",\"snapshot_entry_count\":"+std::to_string(entries.size())+",\"candidate_query_count\":"+std::to_string(queries)+
            ",\"query_unknown_count\":"+std::to_string(query_failures)+",\"rejected_identity_edges\":"+std::to_string(rejected_edges)+
            ",\"bounded_or_deadline_truncated\":"+(truncated?"true":"false")+",\"held_tree_nodes_including_root\":"+std::to_string(tree.nodes.size())+
            ",\"observed_descendants\":"+rows+",\"local_handles_closed\":"+(tree.closed?"true":"false")+
            ",\"snapshot_is_not_atomic\":true,\"empty_does_not_prove_cmd_never_started\":true,\"descendants_controlled\":false,\"authority_inferred\":false");

        stage="exact_foreground_pixel_capture";
        if(!bound())throw std::runtime_error("fresh_Run_foreground_or_deadline_changed_before_capture");
        const auto bounds=visible(source);if(!bounds)throw std::runtime_error("actual_visible_rect_unknown");
        const auto width=bounds->r-bounds->l,height=bounds->b-bounds->t;
        const auto vx=GetSystemMetrics(SM_XVIRTUALSCREEN),vy=GetSystemMetrics(SM_YVIRTUALSCREEN);
        if(width<=0||height<=0||width>4096||height>4096||static_cast<std::uint64_t>(width)*height>8*1024*1024||
            bounds->l<vx||bounds->t<vy||bounds->r>static_cast<std::int64_t>(vx)+GetSystemMetrics(SM_CXVIRTUALSCREEN)||
            bounds->b>static_cast<std::int64_t>(vy)+GetSystemMetrics(SM_CYVIRTUALSCREEN))
            throw std::runtime_error("actual_foreground_rect_outside_bounded_virtual_screen");
        const auto name=run+L"-"+std::wstring(configuration.begin(),configuration.end())+L"-terminal-client-rejection.bmp";
        const auto output=fs::path(L"C:\\PaneBindV1\\Output");const auto path=output/name;
        if(!plain_directory(output)||!marker(output/L"panebind-v1-output.marker",run))throw std::runtime_error("exact_output_marker_unconfirmed");
        struct Pixels {
            bool& known;HDC screen{},memory{};HBITMAP bitmap{};HGDIOBJ previous{};bool cleaned{},closed{true};
            void finish()noexcept{if(cleaned)return;cleaned=true;
                if(memory&&previous&&previous!=HGDI_ERROR){const auto result=SelectObject(memory,previous);if(!result||result==HGDI_ERROR)closed=false;}
                if(bitmap&&!DeleteObject(bitmap))closed=false;if(memory&&!DeleteDC(memory))closed=false;
                if(screen&&!ReleaseDC(nullptr,screen))closed=false;known=known&&closed;}
            ~Pixels(){finish();}
        } pixels{cleanup_known};
        pixels.screen=GetDC(nullptr);if(!pixels.screen)throw std::runtime_error("GetDC_failed");
        pixels.memory=CreateCompatibleDC(pixels.screen);if(!pixels.memory)throw std::runtime_error("CreateCompatibleDC_failed");
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=static_cast<LONG>(width);
        info.bmiHeader.biHeight=-static_cast<LONG>(height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        info.bmiHeader.biCompression=BI_RGB;const DWORD bytes=static_cast<DWORD>(static_cast<std::uint64_t>(width)*height*4);
        info.bmiHeader.biSizeImage=bytes;void* data{};
        pixels.bitmap=CreateDIBSection(pixels.screen,&info,DIB_RGB_COLORS,&data,nullptr,0);
        if(!pixels.bitmap||!data)throw std::runtime_error("CreateDIBSection_failed");
        pixels.previous=SelectObject(pixels.memory,pixels.bitmap);
        if(!pixels.previous||pixels.previous==HGDI_ERROR)throw std::runtime_error("SelectObject_failed");
        if(!bound()||visible(source)!=bounds)throw std::runtime_error("foreground_or_rect_changed_before_BitBlt");
        SetLastError(ERROR_SUCCESS);const bool blit=BitBlt(pixels.memory,0,0,static_cast<int>(width),static_cast<int>(height),pixels.screen,
            static_cast<int>(bounds->l),static_cast<int>(bounds->t),SRCCOPY|CAPTUREBLT)!=FALSE;const auto blit_error=blit?0:GetLastError();
        const bool flushed=blit&&GdiFlush()!=FALSE;
        if(!blit||!flushed)throw std::runtime_error("BitBlt_or_GdiFlush_failed_error_"+std::to_string(blit_error));
        if(!bound()||visible(source)!=bounds)throw std::runtime_error("foreground_or_rect_changed_after_BitBlt_pixels_not_saved");
        BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+bytes;
        Handle file(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("BMP_CREATE_NEW_failed_error_"+std::to_string(GetLastError()));
        const auto write_bytes=[&](const void* buffer,DWORD size){DWORD written{};return WriteFile(file.value,buffer,size,&written,nullptr)&&written==size;};
        const bool written=write_bytes(&header,sizeof(header))&&write_bytes(&info.bmiHeader,sizeof(info.bmiHeader))&&write_bytes(data,bytes)&&FlushFileBuffers(file.value);
        const bool file_closed=CloseHandle(file.value)!=FALSE;file.value=nullptr;cleanup_known=cleanup_known&&file_closed;
        pixels.finish();cleanup_known=cleanup_known&&pixels.closed;const auto digest=written&&file_closed?hash(path):std::string{};
        out.emit("medium_terminal_ready_failure_pixel_capture","\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+
            ",\"actual_visible_rect\":"+rect(*bounds)+",\"file\":"+quote(utf8(name))+",\"CREATE_NEW_return\":true,\"write_and_flush_return\":"+
            (written?"true":"false")+",\"CloseHandle_return\":"+(file_closed?"true":"false")+",\"bytes\":"+std::to_string(header.bfSize)+
            ",\"sha256\":"+(digest.empty()?std::string("null"):quote(digest))+",\"GDI_resources_closed\":"+(pixels.closed?"true":"false")+
            ",\"actual_screen_region_only\":true,\"occlusion_absence_not_inferred\":true,\"visual_review\":\"NOT_RUN\",\"input_sent\":false,\"client_or_content_PASS_inferred\":false");
        if(!written||!file_closed||digest.empty())throw std::runtime_error("BMP_write_close_or_hash_unconfirmed");
    }catch(const std::exception& error){
        out.emit("medium_terminal_ready_failure_observation_rejected","\"stage\":"+quote(stage)+",\"reason\":"+quote(error.what())+
            ",\"unobserved_facts\":null,\"input_sent\":false,\"original_shell_ready_failure_preserved\":true");
    }
    out.emit("medium_terminal_ready_failure_observation_end","\"before_tick\":"+std::to_string(began)+",\"after_tick\":"+
        std::to_string(GetTickCount64())+",\"observation_deadline_tick\":"+std::to_string(end)+",\"local_resource_cleanup_known\":"+
        (cleanup_known?"true":"false")+",\"normal_product_cleanup_inferred\":false,\"extra_launch_or_input\":false");
    return cleanup_known;
}
