#pragma once

// A single guest-driver observation after Terminal's real HTCLIENT rejection.
// Like the existing SettingsGui BMP capture, this reads bounded visible screen
// pixels using GDI; it never renders with PrintWindow or supplies input rights.
void guest_observe_terminal_client_rejection(State& s,const ApplicationTarget& target,
    const product::Snapshot& source,POINT point){
    const auto exact_root=[&]{
        DWORD pid{};const DWORD tid=GetWindowThreadProcessId(source.hwnd,&pid);
        std::optional<product::Snapshot> fresh;
        return target.kind=="Terminal"&&target.accepted&&target.identity&&source.hwnd==source_window(s,0)&&
            product::Catalog::same(*target.identity,source)&&pid==source.pid&&tid==source.tid&&
            GetAncestor(source.hwnd,GA_ROOT)==source.hwnd&&IsWindowVisible(source.hwnd)&&!IsIconic(source.hwnd)&&
            GetForegroundWindow()==source.hwnd&&authorized_source(s,source.hwnd)&&
            application_fresh(s,target,source.hwnd,&fresh)&&fresh&&product::Catalog::same(source,*fresh);
    };
    std::array<wchar_t,128> user{};DWORD user_size=static_cast<DWORD>(user.size());
    const bool exact_run=s.run.size()==32&&std::all_of(s.run.begin(),s.run.end(),[](wchar_t c){return
        (c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');});
    if(!exact_run||(s.configuration!="Debug"&&s.configuration!="Release")||
        !GetUserNameW(user.data(),&user_size)||!equal(user.data(),L"WDAGUtilityAccount")||
        !medium(GetCurrentProcess())||!desktop()||s.left||s.control||
        !product::guest::same_exact_directory(s.output.lexically_normal(),L"C:\\PaneBindV1\\Output")||
        !plain_directory(s.output)||!marker(L"C:\\PaneBindV1\\Input\\panebind-v1-input.marker",s.run)||
        !marker(s.output/L"panebind-v1-output.marker",s.run)||!exact_root())
        throw std::runtime_error("Terminal_observation_exact_guest_run_foreground_root_required");
    const auto bounds=visible(source.hwnd);if(!bounds)throw std::runtime_error("Terminal_observation_visible_rectangle_unknown");
    const LONG width=bounds->width(),height=bounds->height();
    const long long left=GetSystemMetrics(SM_XVIRTUALSCREEN),top=GetSystemMetrics(SM_YVIRTUALSCREEN);
    const long long right=left+GetSystemMetrics(SM_CXVIRTUALSCREEN),bottom=top+GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(width<=0||height<=0||width>4096||height>4096||static_cast<std::uint64_t>(width)*height>8ULL*1024*1024||
        bounds->l<left||bounds->t<top||bounds->r>right||bounds->b>bottom)
        throw std::runtime_error("Terminal_observation_visible_rectangle_not_fully_visible_or_bounded");
    struct ChildFact {
        HWND hwnd{};DWORD pid{},tid{};std::array<wchar_t,128> name{};int name_length{};
        RECT rectangle{};bool visible{},rectangle_known{},point_inside{},query_attempted{},query_returned{};
        DWORD_PTR hit{};DWORD query_error{};
    };
    struct Children {
        HWND root{};DWORD pid{},tid{};POINT point{};std::array<ChildFact,64> facts{};
        std::size_t count{};unsigned queries{};bool limit_reached{},root_changed{};
    } children;children.root=source.hwnd;children.pid=source.pid;children.tid=source.tid;children.point=point;
    SetLastError(ERROR_SUCCESS);
    const BOOL enumerated=EnumChildWindows(source.hwnd,[](HWND child,LPARAM opaque)->BOOL{
        auto& list=*reinterpret_cast<Children*>(opaque);DWORD pid{};
        if(GetWindowThreadProcessId(list.root,&pid)!=list.tid||pid!=list.pid||GetForegroundWindow()!=list.root){
            list.root_changed=true;return FALSE;
        }
        if(!IsChild(list.root,child)||GetAncestor(child,GA_ROOT)!=list.root)return TRUE;
        auto& fact=list.facts[list.count++];fact.hwnd=child;fact.tid=GetWindowThreadProcessId(child,&fact.pid);
        fact.name_length=GetClassNameW(child,fact.name.data(),static_cast<int>(fact.name.size()));
        fact.visible=IsWindowVisible(child)!=FALSE;fact.rectangle_known=GetWindowRect(child,&fact.rectangle)!=FALSE;
        fact.point_inside=fact.rectangle_known&&PtInRect(&fact.rectangle,list.point)!=FALSE;
        if(fact.visible&&fact.point_inside&&fact.tid&&list.queries<8&&
            list.point.x>=std::numeric_limits<short>::min()&&list.point.x<=std::numeric_limits<short>::max()&&
            list.point.y>=std::numeric_limits<short>::min()&&list.point.y<=std::numeric_limits<short>::max()){
            ++list.queries;fact.query_attempted=true;SetLastError(ERROR_SUCCESS);
            fact.query_returned=SendMessageTimeoutW(child,WM_NCHITTEST,0,
                MAKELPARAM(static_cast<short>(list.point.x),static_cast<short>(list.point.y)),
                SMTO_ABORTIFHUNG|SMTO_BLOCK,30,&fact.hit)!=FALSE;
            fact.query_error=fact.query_returned?ERROR_SUCCESS:GetLastError();
        }
        if(list.count==list.facts.size()){list.limit_reached=true;return FALSE;}return TRUE;
    },reinterpret_cast<LPARAM>(&children));
    const DWORD enumeration_error=enumerated?ERROR_SUCCESS:GetLastError();
    std::string rows="[";
    for(std::size_t n=0;n<children.count;++n){const auto& fact=children.facts[n];if(n)rows+=',';
        rows+="{\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(fact.hwnd))+",\"pid\":"+std::to_string(fact.pid)+
            ",\"tid\":"+std::to_string(fact.tid)+",\"class\":"+(fact.name_length>0?quote(utf8(fact.name.data())):"null")+
            ",\"visible\":"+(fact.visible?"true":"false")+",\"rect\":"+(fact.rectangle_known?
                rect({fact.rectangle.left,fact.rectangle.top,fact.rectangle.right,fact.rectangle.bottom}):"null")+
            ",\"point_inside_rect\":"+(fact.point_inside?"true":"false")+",\"WM_NCHITTEST_attempted\":"+(fact.query_attempted?"true":"false")+
            ",\"WM_NCHITTEST_returned\":"+(fact.query_attempted?(fact.query_returned?"true":"false"):"null")+
            ",\"WM_NCHITTEST\":"+(fact.query_returned?std::to_string(fact.hit):"null")+
            ",\"WM_NCHITTEST_error\":"+(fact.query_attempted?std::to_string(fact.query_error):"null")+"}";}
    rows+=']';
    s.log.emit("Terminal_client_rejection_child_inventory","\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source.hwnd))+
        ",\"source_pid\":"+std::to_string(source.pid)+",\"source_tid\":"+std::to_string(source.tid)+
        ",\"point\":["+std::to_string(point.x)+","+std::to_string(point.y)+"],\"EnumChildWindows_return\":"+(enumerated?"true":"false")+
        ",\"EnumChildWindows_last_error\":"+std::to_string(enumeration_error)+",\"descendant_budget\":64,\"count\":"+std::to_string(children.count)+
        ",\"budget_limit_reached\":"+(children.limit_reached?"true":"false")+",\"root_identity_changed\":"+(children.root_changed?"true":"false")+
        ",\"point_covering_hit_query_budget\":8,\"hit_query_timeout_ms\":30,\"hit_queries\":"+std::to_string(children.queries)+
        ",\"descendants\":"+rows+",\"input_or_permission_inferred\":false,\"atomic_snapshot_inferred\":false");
    if(children.root_changed||!exact_root()||visible(source.hwnd)!=bounds)
        throw std::runtime_error("Terminal_observation_root_or_rectangle_changed_after_inventory");
    const auto name=s.run+L"-"+std::wstring(s.configuration.begin(),s.configuration.end())+L"-terminal-client-rejection.bmp";
    const auto path=s.output/name;if(fs::exists(path))throw std::runtime_error("Terminal_observation_new_BMP_required");
    struct Pixels {
        HDC screen{},memory{};HBITMAP bitmap{};HGDIOBJ previous{};
        ~Pixels(){if(memory&&previous&&previous!=HGDI_ERROR)SelectObject(memory,previous);
            if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);if(screen)ReleaseDC(nullptr,screen);}
    } pixels;
    pixels.screen=GetDC(nullptr);if(!pixels.screen)throw std::runtime_error("Terminal_observation_GetDC_failed");
    pixels.memory=CreateCompatibleDC(pixels.screen);if(!pixels.memory)throw std::runtime_error("Terminal_observation_CreateCompatibleDC_failed");
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    const DWORD bytes=static_cast<DWORD>(static_cast<std::uint64_t>(width)*height*4);info.bmiHeader.biSizeImage=bytes;
    void* data{};pixels.bitmap=CreateDIBSection(pixels.screen,&info,DIB_RGB_COLORS,&data,nullptr,0);
    if(!pixels.bitmap||!data)throw std::runtime_error("Terminal_observation_CreateDIBSection_failed");
    pixels.previous=SelectObject(pixels.memory,pixels.bitmap);
    if(!pixels.previous||pixels.previous==HGDI_ERROR)throw std::runtime_error("Terminal_observation_SelectObject_failed");
    if(!exact_root()||visible(source.hwnd)!=bounds||
        !BitBlt(pixels.memory,0,0,width,height,pixels.screen,bounds->l,bounds->t,SRCCOPY|CAPTUREBLT)||!GdiFlush())
        throw std::runtime_error("Terminal_observation_identity_geometry_or_BitBlt_failed");
    if(!exact_root()||visible(source.hwnd)!=bounds)throw std::runtime_error("Terminal_observation_root_changed_after_BitBlt");
    BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+bytes;
    Handle file(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Terminal_observation_BMP_CREATE_NEW_failed");
    const auto write_bytes=[&](const void* buffer,DWORD size){DWORD written{};return WriteFile(file.value,buffer,size,&written,nullptr)&&written==size;};
    const bool written=write_bytes(&header,sizeof(header))&&write_bytes(&info.bmiHeader,sizeof(info.bmiHeader))&&write_bytes(data,bytes)&&FlushFileBuffers(file.value);
    const bool closed=CloseHandle(file.value)!=FALSE;file.value=nullptr;const auto digest=hash(path);
    if(!written||!closed||digest.empty())throw std::runtime_error("Terminal_observation_BMP_write_close_or_hash_failed");
    s.log.emit("Terminal_client_rejection_actual_pixel_capture","\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source.hwnd))+
        ",\"source_pid\":"+std::to_string(source.pid)+",\"source_tid\":"+std::to_string(source.tid)+",\"actual_visible_rect\":"+rect(*bounds)+
        ",\"file\":"+quote(utf8(name))+",\"bytes\":"+std::to_string(header.bfSize)+",\"sha256\":"+quote(digest)+
        ",\"actual_screen_region_only\":true,\"occlusion_absence_not_inferred\":true,\"original_HTCLIENT_rejection_preserved\":true,"
        "\"client_input_sent\":false,\"visual_review\":\"NOT_RUN\",\"UI_or_content_PASS_inferred\":false");
}
