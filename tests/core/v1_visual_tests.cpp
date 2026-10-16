#include "core/v1/visual_ownership.h"

#include <cstdlib>
#include <iostream>

namespace v=panebind::core::v1;
int checks{},failures{};
void expect(bool value,const char* message){++checks;if(!value){++failures;std::cerr<<"FAIL "<<message<<'\n';}}
constexpr std::uint32_t alpha_flag=2,color_flag=1;
v::VisualState alpha(v::VisualState value,int opacity){value.layered=true;value.attributes_known=true;value.alpha=static_cast<std::uint8_t>(opacity);value.flags|=alpha_flag;return value;}
int main(){
    v::VisualState plain;
    v::VisualOwnership state{plain,plain};
    auto current=plain;current.topmost=true;v::record_top(state,current);
    expect(state.changed_top&&v::owns_top(state,current)&&!state.changed_alpha,"topmost starts separate ownership");
    current=alpha(current,v::stepped_alpha(v::effective_alpha(current,alpha_flag),false));v::record_alpha(state,current);
    expect(current.alpha==223&&state.changed_top&&state.changed_alpha,"opacity down does not undo topmost");
    current=alpha(current,v::stepped_alpha(v::effective_alpha(current,alpha_flag),false));v::record_alpha(state,current);
    expect(current.alpha==191&&state.original.alpha==255,"second opacity down advances not toggle original");
    current.topmost=false;v::record_top(state,current);
    expect(!state.changed_top&&state.changed_alpha&&v::owns_alpha(state,current),"return topmost original keeps alpha ownership");
    current=alpha(current,v::stepped_alpha(v::effective_alpha(current,alpha_flag),true));v::record_alpha(state,current);
    expect(current.alpha==223&&state.changed_alpha,"opacity up advances from current");
    v::copy_alpha(current,state.original);v::record_alpha(state,current);
    expect(!current.layered&&!v::visual_changes(state),"return opacity initial restores nonlayered tuple and retires last ownership");
    // Existing colorkey windows have no effective global alpha unless the
    // adapter's alpha flag is actually enabled. Their color/flags survive.
    v::VisualState keyed;keyed.layered=true;keyed.attributes_known=true;keyed.alpha=0;keyed.color=0x102030;keyed.flags=color_flag;
    expect(v::effective_alpha(keyed,alpha_flag)==255,"colorkey-only raw alpha is not effective opacity");
    state={keyed,keyed};current=alpha(keyed,223);v::record_alpha(state,current);
    expect(current.flags==(alpha_flag|color_flag)&&current.color==keyed.color&&state.original.flags==color_flag,"alpha change preserves colorkey and original flags");
    current=keyed;v::record_alpha(state,current);
    expect(!state.changed_alpha&&current.alpha==0&&current.flags==color_flag,"return original restores exact ignored alpha and flags");
    state={plain,plain};current=alpha(plain,190);v::record_alpha(state,current);
    for(int changed=0;changed<5;++changed){
        auto external=current;
        if(changed==0)external.alpha=189;
        if(changed==1)external.color=0x223344;
        if(changed==2)external.flags=alpha_flag|color_flag;
        if(changed==3)external.layered=false;
        if(changed==4)external.attributes_known=false;
        expect(!v::owns_alpha(state,external),"any observed alpha tuple mismatch blocks restoration");
        auto retired=state;v::observe_visual(retired,external);
        expect(!retired.changed_alpha,"external change retires only alpha ownership");
    }
    current.topmost=true;v::record_top(state,current);
    auto external=current;external.flags|=color_flag;external.color=0x334455;
    v::observe_visual(state,external);
    expect(state.changed_top&&!state.changed_alpha&&v::owns_top(state,external),"app alpha change preserves independent topmost ownership");
    auto newly_requested=alpha(external,158);v::record_alpha(state,newly_requested);
    expect(state.original.flags==external.flags&&state.original.color==external.color&&state.changed_alpha,"new explicit action captures external tuple not abandoned old original");
    external=newly_requested;external.topmost=false;v::observe_visual(state,external);
    expect(!state.changed_top&&state.changed_alpha&&v::owns_alpha(state,external),"app topmost change preserves independent alpha ownership");
    // Temporary drag snapshots are born from the actual current tuple; they
    // are not copied from the permanent owner's initial baseline.
    v::VisualOwnership drag{external,external};
    auto dragging=alpha(external,170);v::record_alpha(drag,dragging);
    expect(drag.original.alpha==158&&state.original.alpha==190,"temporary drag records current permanent alpha not permanent original");
    expect(v::same_alpha(drag.original,external)&&v::owns_alpha(drag,dragging),"temporary restoration target exact observed tuple");
    // A native/context rejection does not mutate the actual tuple. The
    // production shake-restoration branch retains this still-owned record;
    // it must not drop both attributes merely because the restore was tried.
    const auto shake_original=alpha(plain,173);
    auto shaking=alpha(shake_original,190);shaking.topmost=true;
    v::VisualOwnership shake_owned{shake_original,shaking,true,true};
    v::observe_visual(shake_owned,shaking);
    expect(v::visual_changes(shake_owned)&&v::owns_alpha(shake_owned,shaking)&&v::owns_top(shake_owned,shaking),
        "unchanged actual state after failed restoration keeps both cleanup responsibilities");
    auto partially_restored=shaking;v::copy_alpha(partially_restored,shake_original);v::record_alpha(shake_owned,partially_restored);
    expect(v::visual_changes(shake_owned)&&!shake_owned.changed_alpha&&v::owns_top(shake_owned,partially_restored),
        "exact alpha restoration cannot retire an uncompleted topmost restoration");
    partially_restored.topmost=shake_original.topmost;v::record_top(shake_owned,partially_restored);
    expect(!v::visual_changes(shake_owned),"both exactly restored attributes retire the shake record");
    shake_owned={shake_original,shaking,true,true};
    auto app_changed=shaking;app_changed.alpha=199;v::observe_visual(shake_owned,app_changed);
    expect(!shake_owned.changed_alpha&&shake_owned.changed_top&&!v::owns_alpha(shake_owned,app_changed),
        "retaining failed cleanup never grants restoration over an application's changed alpha");
    app_changed.topmost=false;v::observe_visual(shake_owned,app_changed);
    expect(!v::visual_changes(shake_owned),"independent app changes can retire ownership without overwriting them");
    expect(v::stepped_alpha(255,false)==223&&v::stepped_alpha(223,false)==191&&v::stepped_alpha(191,true)==223,"multiple opacity steps deterministic");
    expect(v::stepped_alpha(20,false)==16&&v::stepped_alpha(8,false)==8&&v::stepped_alpha(0,true)==32&&v::stepped_alpha(250,true)==255,"opacity edges never reverse requested direction");
    auto unknown=external;unknown.attributes_known=false;
    expect(!v::same_alpha(unknown,unknown),"unknown layered attributes cannot prove own restoration");
    expect(v::same_alpha(plain,v::VisualState{}),"nonlayered state is independently representable");
    if(failures){std::cerr<<failures<<'/'<<checks<<" failed\n";return EXIT_FAILURE;}
    std::cout<<"V1 visual ownership "<<checks<<" checks passed\n";return EXIT_SUCCESS;
}
