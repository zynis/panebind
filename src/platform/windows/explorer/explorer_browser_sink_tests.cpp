#include "platform/windows/explorer/explorer_browser_sink_test_support.h"
#include "platform/windows/explorer/explorer_consent_validation.h"
#include <exdispid.h>
#include <objbase.h>
#include <iostream>
#include <thread>
namespace e=panebind::platform::windows::explorer;
struct FakeBrowser final:IDispatch {
    ULONG refs{1};bool reject_identity{};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) noexcept override {
        if(!out)return E_POINTER;*out=nullptr;
        if(reject_identity)return E_NOINTERFACE;
        if(id!=IID_IUnknown&&id!=IID_IDispatch)return E_NOINTERFACE;
        *out=static_cast<IDispatch*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() noexcept override{return --refs;}
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT*) noexcept override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo**) noexcept override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*) noexcept override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*) noexcept override{return E_NOTIMPL;}
};
int main(){
    if(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)!=S_OK)return 2;
    int failures{},checks{};const auto check=[&](bool ok){++checks;if(!ok)++failures;};
    FakeBrowser browser;
    for(const DISPID id:{DISPID_WINDOWSETLEFT,DISPID_WINDOWSETTOP,DISPID_WINDOWSETWIDTH,DISPID_WINDOWSETHEIGHT}) {
        auto* sink=e::testing::create_browser_sink(&browser);
        VARIANT arg{};arg.vt=VT_I4;arg.lVal=123;
        DISPPARAMS args{&arg,nullptr,1,0};
        for(int i=0;i<100;++i)check(sink->Invoke(id,IID_NULL,0,DISPATCH_METHOD,&args,nullptr,nullptr,nullptr)==S_OK);
        const auto facts=e::testing::browser_sink_facts(sink);
        check(facts.malformed_count==0&&facts.overflow_count==0&&facts.callback_sequence==0&&facts.latest_sequence==0);
        check(facts.geometry_event_count==100&&facts.last_geometry_dispid==id);
        sink->Release();
    }
    for(int fault=0;fault<7;++fault) {
        auto* sink=e::testing::create_browser_sink(&browser);
        VARIANT arg{};arg.vt=VT_I4;DISPPARAMS args{&arg,nullptr,1,0};
        DISPID id=DISPID_WINDOWSETLEFT;WORD flags=DISPATCH_METHOD;
        if(fault==0)id=9999;if(fault==1)arg.vt=VT_BSTR;if(fault==2)args.cArgs=0;
        if(fault==3)args.cNamedArgs=1;if(fault==4)args.rgvarg=nullptr;
        if(fault==5)flags=DISPATCH_PROPERTYGET;
        check(FAILED(sink->Invoke(id,fault==6?IID_IUnknown:IID_NULL,0,flags,&args,nullptr,nullptr,nullptr)));
        check(e::testing::browser_sink_facts(sink).malformed_count==1);sink->Release();
    }
    for(int source=0;source<3;++source) {
        FakeBrowser other;auto* sink=e::testing::create_browser_sink(&browser);
        browser.reject_identity=source==2;
        VARIANT url{};url.vt=VT_EMPTY;VARIANT args[2]{};
        args[0].vt=VT_VARIANT|VT_BYREF;args[0].pvarVal=&url;
        args[1].vt=VT_DISPATCH;args[1].pdispVal=source==1?&other:&browser;
        DISPPARAMS params{args,nullptr,2,0};
        check(sink->Invoke(DISPID_NAVIGATECOMPLETE2,IID_NULL,0,DISPATCH_METHOD,&params,nullptr,nullptr,nullptr)==S_OK);
        e::testing::drain_browser_sink(sink);const auto f=e::testing::browser_sink_facts(sink);
        check(source==0?f.matching_navigate_complete_count==1:source==1?f.unrelated_navigate_complete_count==1:f.identity_query_failure_count==1);
        browser.reject_identity=false;sink->Release();
    }
    auto* sink=e::testing::create_browser_sink(&browser);DISPPARAMS empty{};
    check(sink->Invoke(DISPID_ONQUIT,IID_NULL,0,DISPATCH_METHOD,&empty,nullptr,nullptr,nullptr)==S_OK);
    e::testing::drain_browser_sink(sink);check(e::testing::browser_sink_facts(sink).quit_count==1);
    e::testing::retire_browser_sink(sink);
    sink->Invoke(DISPID_ONQUIT,IID_NULL,0,DISPATCH_METHOD,&empty,nullptr,nullptr,nullptr);
    check(e::testing::browser_sink_facts(sink).post_retirement_count==1);sink->Release();
    sink=e::testing::create_browser_sink(&browser);
    std::thread foreign([&]{sink->Invoke(DISPID_ONQUIT,IID_NULL,0,DISPATCH_METHOD,&empty,nullptr,nullptr,nullptr);});foreign.join();
    check(e::testing::browser_sink_facts(sink).wrong_thread_count==1);sink->Release();
    check(browser.refs==1);CoUninitialize();
    std::cout<<"browser-sink checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
