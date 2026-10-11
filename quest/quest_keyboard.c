#include "quest_keyboard.h"
#include "quest_lobby.h"
static bool active;
void qkeyboard_update(void){
    bool editing=qlobby_editing();if(editing==active)return;active=editing;
    if(!editing){SDL_StopTextInput();return;}
#ifdef TCVR_STEAM_FRAME
    /* SDL's Android IME request is not a proven SteamVR keyboard bridge.
     * Accept delivered text, but do not pretend we opened a keyboard. */
    SDL_EventState(SDL_TEXTINPUT,SDL_ENABLE);
    qlobby_keyboard_result(false);
#else
    SDL_Rect area={24,150,592,54};SDL_SetTextInputRect(&area);SDL_StartTextInput();
    qlobby_keyboard_result(true); /* request only, not runtime display evidence */
#endif
}
bool qkeyboard_event(const SDL_Event *event){
    if(!qlobby_editing())return false;
    if(event->type==SDL_TEXTINPUT){qlobby_text(event->text.text);return true;}
    if(event->type!=SDL_KEYDOWN)return false;
#ifdef TCVR_PC
    if(event->key.keysym.sym==SDLK_v&&(event->key.keysym.mod&(KMOD_CTRL|KMOD_GUI))){
        char *text=SDL_GetClipboardText();
        if(text){qlobby_text(text);SDL_free(text);}
        return true;
    }
#endif
    switch(event->key.keysym.sym){
        case SDLK_BACKSPACE:qlobby_backspace();break;
        case SDLK_RETURN:case SDLK_KP_ENTER:qlobby_input(QL_ACCEPT);break;
        case SDLK_ESCAPE:qlobby_input(QL_BACK);break;
        default:break;
    }
    return true;
}
