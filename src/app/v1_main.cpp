#include "platform/windows/product/product_app.h"
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int){
    return panebind::platform::windows::product::run(instance,command);
}
