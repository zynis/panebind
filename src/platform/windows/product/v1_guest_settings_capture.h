#pragma once

// Guest-only: include inside the fixture's unnamed namespace after gui_*.
// This captures actual visible pixels, not a product-side PrintWindow render.
// The BMP is review material; its existence never awards a visual/layout PASS.
void guest_capture_settings_page(State& s,const SettingsGui& gui,unsigned page){
    if(page>=6||!gui_identity(s,gui,gui.root)||GetForegroundWindow()!=gui.root||
        gui_read(s,gui,gui.tab,TCM_GETCURSEL)!=page||!gui_identity(s,gui,gui_focus(gui)))
        throw std::runtime_error("settings_capture_exact_foreground_product_page_required");
    RECT window{};if(!GetWindowRect(gui.root,&window))throw std::runtime_error("settings_capture_actual_window_rect_unavailable");
    const LONG width=window.right-window.left,height=window.bottom-window.top;
    const long long virtual_left=GetSystemMetrics(SM_XVIRTUALSCREEN),virtual_top=GetSystemMetrics(SM_YVIRTUALSCREEN);
    const long long virtual_right=virtual_left+GetSystemMetrics(SM_CXVIRTUALSCREEN),virtual_bottom=virtual_top+GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(width<=0||height<=0||width>4096||height>4096||static_cast<std::uint64_t>(width)*height>8ULL*1024*1024||
        window.left<virtual_left||window.top<virtual_top||window.right>virtual_right||window.bottom>virtual_bottom)
        throw std::runtime_error("settings_capture_rectangle_not_fully_visible_or_bounded");
    const UINT dpi=GetDpiForWindow(gui.root);if(!dpi)throw std::runtime_error("settings_capture_actual_DPI_unknown");
    const auto name=s.run+L"-"+std::wstring(s.configuration.begin(),s.configuration.end())+L"-settings-page-"+std::to_wstring(page)+L".bmp";
    const auto target=s.output/name;
    if(!plain_directory(s.output)||fs::exists(target))throw std::runtime_error("settings_capture_new_exact_output_file_required");
    struct Pixels {
        HDC screen{},memory{};HBITMAP bitmap{};HGDIOBJ previous{};
        ~Pixels(){if(memory&&previous&&previous!=HGDI_ERROR)SelectObject(memory,previous);
            if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);if(screen)ReleaseDC(nullptr,screen);}
    } pixels;
    pixels.screen=GetDC(nullptr);if(!pixels.screen)throw std::runtime_error("settings_capture_GetDC_failed");
    pixels.memory=CreateCompatibleDC(pixels.screen);if(!pixels.memory)throw std::runtime_error("settings_capture_CreateCompatibleDC_failed");
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    const auto bytes=static_cast<DWORD>(static_cast<std::uint64_t>(width)*height*4);info.bmiHeader.biSizeImage=bytes;
    void* data{};pixels.bitmap=CreateDIBSection(pixels.screen,&info,DIB_RGB_COLORS,&data,nullptr,0);
    if(!pixels.bitmap||!data)throw std::runtime_error("settings_capture_CreateDIBSection_failed");
    pixels.previous=SelectObject(pixels.memory,pixels.bitmap);
    if(!pixels.previous||pixels.previous==HGDI_ERROR)throw std::runtime_error("settings_capture_SelectObject_failed");
    // The source is only the exact settings rectangle in the guest's current
    // desktop. No host, whole-desktop, hidden-window or fabricated UI capture.
    Sleep(100);RECT confirmed{};
    if(!gui_identity(s,gui,gui.root)||GetForegroundWindow()!=gui.root||!GetWindowRect(gui.root,&confirmed)||
        !EqualRect(&window,&confirmed)||!BitBlt(pixels.memory,0,0,width,height,pixels.screen,window.left,window.top,SRCCOPY|CAPTUREBLT))
        throw std::runtime_error("settings_capture_identity_geometry_or_BitBlt_failed");
    BITMAPFILEHEADER file_header{};file_header.bfType=0x4d42;
    file_header.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);file_header.bfSize=file_header.bfOffBits+bytes;
    Handle output(CreateFileW(target.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(output.value==INVALID_HANDLE_VALUE)throw std::runtime_error("settings_capture_CREATE_NEW_failed");
    const auto write_bytes=[&](const void* buffer,DWORD size){DWORD written{};return WriteFile(output.value,buffer,size,&written,nullptr)&&written==size;};
    const bool written=write_bytes(&file_header,sizeof(file_header))&&write_bytes(&info.bmiHeader,sizeof(info.bmiHeader))&&
        write_bytes(data,bytes)&&FlushFileBuffers(output.value);
    const bool closed=CloseHandle(output.value)!=FALSE;output.value=nullptr;
    if(!closed)throw std::runtime_error("settings_capture_CloseHandle_failed");
    const auto digest=hash(target);
    if(!written||digest.empty())throw std::runtime_error("settings_capture_BMP_write_or_hash_failed");
    s.log.emit("settings_actual_pixel_capture","\"page\":"+std::to_string(page)+",\"pid\":"+std::to_string(s.product_pid)+
        ",\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.root))+",\"tid\":"+std::to_string(gui.tid)+
        ",\"actual_window_rect\":"+rect({window.left,window.top,window.right,window.bottom})+",\"dpi\":"+std::to_string(dpi)+
        ",\"width\":"+std::to_string(width)+",\"height\":"+std::to_string(height)+",\"focus\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(gui_focus(gui)))+",\"file\":"+quote(utf8(name))+",\"sha256\":"+quote(digest)+
        ",\"actual_screen_region_only\":true,\"visual_review\":\"NOT_RUN\",\"UI_pass_inferred\":false");
}
[[maybe_unused]] void owned_settings_screenshots(State& s){
    const auto before=read(s.product.parent_path()/L"settings.ini");const auto gui=gui_open(s);
    for(unsigned page=0;page<6;++page){gui_page_select(s,gui,page);guest_capture_settings_page(s,gui,page);}
    gui_press(s,gui,VK_ESCAPE);
    s.log.check("settings_screenshots_actual_Esc_no_configuration_change",wait([&]{return !IsWindow(gui.root);})&&
        read(s.product.parent_path()/L"settings.ini")==before);
}
