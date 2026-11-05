#pragma once

// TEST ONLY. The ordinary product does not depend on this capture/input path.
// Included after the existing exact-guest GUI helpers. PNGs contain actual
// desktop pixels at their original size; no repaint, enlargement or DPI spoof.
template<class T> struct ReviewCom {
    T* value{};
    ~ReviewCom(){if(value)value->Release();}
    T** receive(){return &value;}
    T* operator->()const{return value;}
};
void review_png(const fs::path& target,HBITMAP bitmap,UINT width,UINT height){
    if(fs::exists(target))throw std::runtime_error("review_PNG_requires_new_path");
    ReviewCom<IWICImagingFactory> factory;ReviewCom<IWICBitmap> image;
    ReviewCom<IWICStream> stream;ReviewCom<IWICBitmapEncoder> encoder;
    ReviewCom<IWICBitmapFrameEncode> frame;ReviewCom<IPropertyBag2> options;
    const auto checked=[](HRESULT hr){if(FAILED(hr))throw std::runtime_error("review_actual_pixel_PNG_encoding_failed");};
    checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.receive())));
    checked(factory->CreateBitmapFromHBITMAP(bitmap,nullptr,WICBitmapIgnoreAlpha,image.receive()));
    checked(factory->CreateStream(stream.receive()));
    checked(stream->InitializeFromFilename(target.c_str(),GENERIC_WRITE));
    checked(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.receive()));
    checked(encoder->Initialize(stream.value,WICBitmapEncoderNoCache));
    checked(encoder->CreateNewFrame(frame.receive(),options.receive()));
    checked(frame->Initialize(options.value));checked(frame->SetSize(width,height));
    WICPixelFormatGUID format=GUID_WICPixelFormat24bppBGR;
    checked(frame->SetPixelFormat(&format));checked(frame->WriteSource(image.value,nullptr));
    checked(frame->Commit());checked(encoder->Commit());
}
void review_capture_settings(State& s,const SettingsGui& gui,unsigned page,std::string_view part){
    if((s.suite!="SettingsUiReviewOnly"&&s.suite!="SettingsEditorsOnly")||page>=6||part.empty()||part.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")!=part.npos||
       !gui_identity(s,gui,gui.root)||GetForegroundWindow()!=gui.root||
       gui_read(s,gui,gui.tab,TCM_GETCURSEL)!=page||!gui_identity(s,gui,gui_focus(gui)))
        throw std::runtime_error("review_capture_exact_foreground_page_required");
    RECT bounds{};if(!GetWindowRect(gui.root,&bounds))throw std::runtime_error("review_capture_window_rect_unknown");
    const LONG width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
    const LONG x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN);
    const LONG w=GetSystemMetrics(SM_CXVIRTUALSCREEN),h=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const UINT dpi=GetDpiForWindow(gui.root);
    if(!dpi||width<=0||height<=0||width>4096||height>4096||std::uint64_t(width)*height>8ULL*1024*1024||
       bounds.left<x||bounds.top<y||bounds.right>x+w||bounds.bottom>y+h)
        throw std::runtime_error("review_capture_actual_rectangle_or_DPI_invalid");
    const auto file=s.run+L"-"+std::wstring(s.configuration.begin(),s.configuration.end())+L"-ui-page-"+
        std::to_wstring(page)+L"-"+std::wstring(part.begin(),part.end())+L".png";
    const auto path=s.output/file;
    if(!plain_directory(s.output)||fs::exists(path))throw std::runtime_error("review_capture_new_output_required");
    struct Pixels {
        HDC screen{},memory{};HBITMAP bitmap{};HGDIOBJ previous{};
        ~Pixels(){if(memory&&previous&&previous!=HGDI_ERROR)SelectObject(memory,previous);
            if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);if(screen)ReleaseDC(nullptr,screen);}
    } pixels;
    pixels.screen=GetDC(nullptr);if(!pixels.screen)throw std::runtime_error("review_capture_GetDC_failed");
    pixels.memory=CreateCompatibleDC(pixels.screen);if(!pixels.memory)throw std::runtime_error("review_capture_CreateCompatibleDC_failed");
    pixels.bitmap=CreateCompatibleBitmap(pixels.screen,width,height);if(!pixels.bitmap)throw std::runtime_error("review_capture_CreateCompatibleBitmap_failed");
    pixels.previous=SelectObject(pixels.memory,pixels.bitmap);
    if(!pixels.previous||pixels.previous==HGDI_ERROR)throw std::runtime_error("review_capture_SelectObject_failed");
    // Flush only this observed UI's pending drawing. Never render PrintWindow
    // or change another process's layout to manufacture a screenshot.
    RECT fresh{};
    if(!gui_identity(s,gui,gui.root)||GetForegroundWindow()!=gui.root||!GetWindowRect(gui.root,&fresh)||
       !EqualRect(&fresh,&bounds)||!BitBlt(pixels.memory,0,0,width,height,pixels.screen,bounds.left,bounds.top,SRCCOPY|CAPTUREBLT)||!GdiFlush())
        throw std::runtime_error("review_capture_fresh_identity_or_BitBlt_failed");
    review_png(path,pixels.bitmap,static_cast<UINT>(width),static_cast<UINT>(height));
    const auto digest=hash(path);if(digest.empty())throw std::runtime_error("review_capture_PNG_hash_failed");
    SCROLLINFO scroll{sizeof(scroll),SIF_RANGE|SIF_PAGE|SIF_POS};const bool known=GetScrollInfo(gui_page(s,gui),SB_VERT,&scroll)!=FALSE;
    s.log.emit("settings_review_actual_PNG","\"page\":"+std::to_string(page)+",\"part\":"+quote(part)+
        ",\"product_source_sha\":"+quote(s.product_source)+",\"product_binary_sha256\":"+quote(hash(s.product))+
        ",\"driver_source_sha\":"+quote(PANEBIND_BUILD_SHA)+",\"pid\":"+std::to_string(s.product_pid)+
        ",\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.root))+",\"tid\":"+std::to_string(gui.tid)+
        ",\"actual_window_rect\":"+rect({bounds.left,bounds.top,bounds.right,bounds.bottom})+
        ",\"dpi\":"+std::to_string(dpi)+",\"width\":"+std::to_string(width)+",\"height\":"+std::to_string(height)+
        ",\"file\":"+quote(utf8(file))+",\"sha256\":"+quote(digest)+
        ",\"page_scroll_known\":"+(known?"true":"false")+",\"scroll_position\":"+(known?std::to_string(scroll.nPos):"null")+
        ",\"scroll_maximum\":"+(known?std::to_string(scroll.nMax):"null")+
        ",\"original_screen_pixels\":true,\"visual_review\":\"NOT_RUN\",\"whole_contract_PASS\":false");
}
void review_tab_to(State& s,const SettingsGui& gui,HWND target,bool reverse=false){
    if(!gui_identity(s,gui,target)||!IsWindowVisible(target)||!IsWindowEnabled(target)||
       !(GetWindowLongPtrW(target,GWL_STYLE)&WS_TABSTOP))throw std::runtime_error("review_Tab_target_not_owned_tabstop");
    unsigned count{};
    for(;count<100&&gui_focus(gui)!=target;++count){
        const auto before=gui_focus(gui);if(reverse)key(s,VK_SHIFT,true);
        gui_press(s,gui,VK_TAB);if(reverse)key(s,VK_SHIFT,false);
        if(!wait([&]{const auto actual=gui_focus(gui);return actual&&actual!=before&&gui_identity(s,gui,actual);},500))
            throw std::runtime_error("review_actual_Tab_or_ShiftTab_did_not_advance");
    }
    s.log.check("settings_review_actual_keyboard_focus_target",gui_focus(gui)==target,
        "\"target\":"+std::to_string(reinterpret_cast<std::uintptr_t>(target))+",\"control_id\":"+std::to_string(GetDlgCtrlID(target))+
        ",\"key_pairs\":"+std::to_string(count)+",\"reverse\":"+(reverse?"true":"false")+",\"SetFocus_used\":false");
}
void review_page_bounds(State& s,const SettingsGui& gui,unsigned page){
    const auto panel=gui_page(s,gui);RECT visible{};if(!GetWindowRect(panel,&visible))throw std::runtime_error("review_actual_page_rect_unknown");
    unsigned count{};std::string rows="[";
    for(HWND child=GetWindow(panel,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
        if(++count>160||!gui_identity(s,gui,child))throw std::runtime_error("review_page_child_identity_or_count_invalid");
        RECT actual{};if(!GetWindowRect(child,&actual))throw std::runtime_error("review_child_rectangle_unknown");
        if(rows.size()>1)rows+=',';
        rows+="{\"id\":"+std::to_string(GetDlgCtrlID(child))+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(child))+
            ",\"class\":"+quote(utf8(gui_class(child)))+",\"label\":"+quote(utf8(settings_guest_text(s,gui,child)))+
            ",\"rect\":"+rect({actual.left,actual.top,actual.right,actual.bottom})+
            ",\"enabled\":"+(IsWindowEnabled(child)?"true":"false")+
            ",\"within_page\":"+(actual.left>=visible.left&&actual.top>=visible.top&&actual.right<=visible.right&&actual.bottom<=visible.bottom?"true":"false")+"}";
    }rows+=']';
    s.log.emit("settings_review_actual_control_bounds","\"page\":"+std::to_string(page)+",\"page_rect\":"+
        rect({visible.left,visible.top,visible.right,visible.bottom})+",\"controls\":"+rows+
        ",\"scrollable_offscreen_controls_are_not_automatically_defects\":true,\"Chinese_readability_PASS_inferred\":false");
}
void owned_settings_ui_review_capture(State& s){
    settings_guest_scope(s);const auto before=read(s.product.parent_path()/L"settings.ini");
    const auto gui=gui_open(s);
    for(unsigned page=0;page<6;++page){
        gui_page_select(s,gui,page);const auto panel=gui_page(s,gui);
        std::vector<HWND> stops;
        for(HWND child=GetWindow(panel,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
            if((GetWindowLongPtrW(child,GWL_STYLE)&WS_TABSTOP)&&IsWindowVisible(child)&&IsWindowEnabled(child))stops.push_back(child);
        if(stops.empty())throw std::runtime_error("review_page_has_no_actual_keyboard_target");
        review_tab_to(s,gui,stops.front());review_capture_settings(s,gui,page,"top");review_page_bounds(s,gui,page);
        const auto before_focus=gui_focus(gui);key(s,VK_SHIFT,true);gui_press(s,gui,VK_TAB);key(s,VK_SHIFT,false);
        s.log.check("settings_review_actual_ShiftTab",wait([&]{const auto focus=gui_focus(gui);return focus&&focus!=before_focus&&gui_identity(s,gui,focus);},500),
            "\"page\":"+std::to_string(page)+",\"visible_focus_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui_focus(gui))));
        review_tab_to(s,gui,stops.back());
        SCROLLINFO scroll{sizeof(scroll),SIF_RANGE|SIF_PAGE|SIF_POS};
        if(!GetScrollInfo(panel,SB_VERT,&scroll))throw std::runtime_error("review_page_scroll_range_unknown");
        if(scroll.nPos>0){review_capture_settings(s,gui,page,"bottom");review_page_bounds(s,gui,page);}
    }
    gui_press(s,gui,VK_ESCAPE);
    s.log.check("settings_review_six_pages_actual_Esc_preserves_configuration",wait([&]{return !IsWindow(gui.root);})&&
        read(s.product.parent_path()/L"settings.ini")==before,
        "\"actual_input\":true,\"six_original_size_PNGs\":true,\"150_is_not_200\":true,\"human_UAT\":\"NOT_RUN\"");
}
