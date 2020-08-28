#include <cstddef>
#include "wxvbam.h"
#include "wx/sdljoy.h"
#include "SDL.h"
#include <SDL_events.h>
#include <wx/window.h>
#include "../common/range.hpp"
#include "../common/contains.h"

using namespace Range;

// For testing a GameController as a Joystick:
//#define SDL_IsGameController(x) false

DEFINE_EVENT_TYPE(wxEVT_SDLJOY)

wxSDLJoy::wxSDLJoy()
    : wxTimer()
    , evthandler(nullptr)
{
    // Start up joystick if not already started
    // FIXME: check for errors
    SDL_Init(SDL_INIT_JOYSTICK);
    SDL_Init(SDL_INIT_GAMECONTROLLER);
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
}

wxSDLJoy::~wxSDLJoy()
{
    // SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

static int16_t axisval(int16_t x)
{
    if (x > 0x1fff)
        return 1;
    else if (x < -0x1fff)
        return -1;

    return 0;
}

void wxSDLJoy::Poll()
{
    wxEvtHandler* handler = evthandler ? evthandler : wxWindow::FindFocus();
    SDL_Event e;

    bool got_event = false;

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
            {
                SDL_JoystickID instance_id = e.cbutton.which;

                if (contains(gcstate, instance_id)) {
                    auto but      = e.cbutton.button;
                    auto val      = e.cbutton.state;
                    auto prev_val = gcstate[instance_id].button[but];
                    auto player_index = SDL_GameControllerGetPlayerIndex(gcstate[instance_id].dev);

                    if (handler && val != prev_val) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.player_index  = player_index;
                        ev.ctrl_type     = WXSDLJOY_BUTTON;
                        ev.ctrl_idx      = but;
                        ev.ctrl_val      = val;
                        ev.prev_val      = prev_val;

                        handler->ProcessEvent(ev);
                    }

                    gcstate[instance_id].button[but] = val;

                    wxLogDebug("GOT SDL_CONTROLLERBUTTON: joy:%d but:%d val:%d prev_val:%d", player_index, but, val, prev_val);
                }

                got_event = true;

                break;
            }
            case SDL_CONTROLLERAXISMOTION:
            {
                SDL_JoystickID instance_id = e.caxis.which;

                if (contains(gcstate, instance_id)) {
                    auto axis     = e.caxis.axis;
                    auto val      = axisval(e.caxis.value);
                    auto prev_val = gcstate[instance_id].axis[axis];

                    if (handler && val != prev_val) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.player_index  = SDL_GameControllerGetPlayerIndex(gcstate[instance_id].dev);
                        ev.ctrl_type     = WXSDLJOY_AXIS;
                        ev.ctrl_idx      = axis;
                        ev.ctrl_val      = val;
                        ev.prev_val      = prev_val;

                        handler->ProcessEvent(ev);

                        gcstate[instance_id].axis[axis] = val;

                        wxLogDebug("GOT SDL_CONTROLLERAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", instance_id, axis, val, prev_val);
                    }
                }

                got_event = true;

                break;
            }
            case SDL_CONTROLLERDEVICEADDED:
            {
                //Game Controller index for ADDED
                int joystick_index = e.cdevice.which;
                std::cout << "CONTROLLERDEVICEADDED Joy index = " << joystick_index << std::endl;

                ConnectController(joystick_index);

                systemScreenMessage(wxString::Format(_("Connected game controller %d"), joystick_index + 1));

                got_event = true;

                break;
            }
            case SDL_CONTROLLERDEVICEREMAPPED:
            {
                SDL_JoystickID instance_id = e.cdevice.which;
                std::cout << "CONTROLLERDEVICEREMOVED Joy IID = " << instance_id << std::endl;

                //I don't think we have to do anything here
#if 0
                for (auto&& joy : joystate) {
                    if (joy.second.dev == gc) {
                        DisconnectController(joy.first);
                        ConnectController(joy.first);

                        systemScreenMessage(wxString::Format(_("Connected game controller %d"), joy.first + 1));
                        break;
                    }
                }
#endif

                got_event = true;

                break;
            }
            case SDL_CONTROLLERDEVICEREMOVED:
            {
                //Joystick Instance ID
                SDL_JoystickID instance_id = e.cdevice.which;
                std::cout << "CONTROLLERDEVICEREMOVED Joy IID = " << instance_id << std::endl;

                gcstate.erase(instance_id);

                got_event = true;

                break;
            }

            // Joystck events for non-GameControllers.

            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
            {
                SDL_JoystickID instance_id = e.jbutton.which;

                if (contains(joystate, instance_id)) {
                    auto but      = e.jbutton.button;
                    auto val      = e.jbutton.state;
                    auto prev_val = joystate[instance_id].button[but];

                    if (handler && val != prev_val) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.player_index  = SDL_JoystickGetPlayerIndex(joystate[instance_id].dev);
                        ev.ctrl_type     = WXSDLJOY_BUTTON;
                        ev.ctrl_idx      = but;
                        ev.ctrl_val      = val;
                        ev.prev_val      = prev_val;

                        handler->ProcessEvent(ev);
                    }

                    joystate[instance_id].button[but] = val;

                    wxLogDebug("GOT SDL_JOYBUTTON: joy:%d but:%d val:%d prev_val:%d", instance_id, but, val, prev_val);
                }

                got_event = true;

                break;
            }
            case SDL_JOYAXISMOTION:
            {
                SDL_JoystickID instance_id = e.jaxis.which;

                if (contains(joystate, instance_id)) {
                    auto axis     = e.jaxis.axis;
                    auto val      = axisval(e.jaxis.value);
                    auto prev_val = joystate[instance_id].axis[axis];

                    if (handler && val != prev_val) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.player_index  = SDL_JoystickGetPlayerIndex(joystate[instance_id].dev);
                        ev.ctrl_type     = WXSDLJOY_AXIS;
                        ev.ctrl_idx      = axis;
                        ev.ctrl_val      = val;
                        ev.prev_val      = prev_val;

                        handler->ProcessEvent(ev);

                        joystate[instance_id].axis[axis] = val;

                        wxLogDebug("GOT SDL_JOYAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", instance_id, axis, val, prev_val);
                    }
                }

                got_event = true;

                break;
            }
            case SDL_JOYDEVICEADDED:
            {
                //Joystick device index
                int joystick_index = e.jdevice.which;
                std::cout << "JOYDEVICEADDED Joy index = " << joystick_index << std::endl;

                ConnectController(joystick_index);

                got_event = true;

                break;
            }
            case SDL_JOYDEVICEREMOVED:
            {
                //InstanceID
                SDL_JoystickID instance_id = e.jdevice.which;

                joystate.erase(instance_id);

                got_event = true;

                break;
            }
        }
    }

    bool do_poll  = false;
    wxLongLong tm = wxGetUTCTimeMillis();

    if (got_event)
        last_poll = tm;
    else if (tm - last_poll > POLL_TIME_MS) {
        do_poll   = true;
        last_poll = tm;
    }

    if (do_poll) {
        for (auto&& gc : gcstate) {
            for (uint8_t but = 0; but < SDL_CONTROLLER_BUTTON_MAX; but++) {
                auto last_state = gc.second.button[but];
                auto state = SDL_GameControllerGetButton(gc.second.dev, static_cast<SDL_GameControllerButton>(but));

                if (last_state != state) {
                    if (handler) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.player_index = SDL_GameControllerGetPlayerIndex(gc.second.dev);
                        ev.ctrl_type = WXSDLJOY_BUTTON;
                        ev.ctrl_idx = but;
                        ev.ctrl_val = state;
                        ev.prev_val = last_state;

                        handler->ProcessEvent(ev);
                    }

                    gc.second.button[but] = state;

                    wxLogDebug("POLLED SDL_CONTROLLERBUTTON: joy:%d but:%d val:%d prev_val:%d", gc.first, but, state, last_state);
                }
            }

            for (uint8_t axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
                auto val = axisval(SDL_GameControllerGetAxis(gc.second.dev, static_cast<SDL_GameControllerAxis>(axis)));
                auto prev_val = gc.second.axis[axis];

                if (handler && val != prev_val) {
                    wxSDLJoyEvent ev(wxEVT_SDLJOY);
                    ev.player_index = SDL_GameControllerGetPlayerIndex(gc.second.dev);
                    ev.ctrl_type = WXSDLJOY_AXIS;
                    ev.ctrl_idx = axis;
                    ev.ctrl_val = val;
                    ev.prev_val = prev_val;

                    handler->ProcessEvent(ev);

                    gc.second.axis[axis] = val;

                    wxLogDebug("POLLED SDL_CONTROLLERAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", gc.first, axis, val, prev_val);
                }
            }
        }
        for (auto&& joy : joystate) {
            for (uint8_t but = 0; but < SDL_JoystickNumButtons(joy.second.dev); but++) {
                auto last_state = joy.second.button[but];
                auto state      = SDL_JoystickGetButton(joy.second.dev, but);

                if (last_state != state) {
                    if (handler) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.player_index = SDL_JoystickGetPlayerIndex(joy.second.dev);
                        ev.ctrl_type     = WXSDLJOY_BUTTON;
                        ev.ctrl_idx      = but;
                        ev.ctrl_val      = state;
                        ev.prev_val      = last_state;

                        handler->ProcessEvent(ev);
                    }

                    joy.second.button[but] = state;

                    wxLogDebug("POLLED SDL_JOYBUTTON: joy:%d but:%d val:%d prev_val:%d", joy.first, but, state, last_state);
                }
            }

            for (uint8_t axis = 0; axis < SDL_JoystickNumAxes(joy.second.dev); axis++) {
                auto val      = axisval(SDL_JoystickGetAxis(joy.second.dev, axis));
                auto prev_val = joy.second.axis[axis];

                if (handler && val != prev_val) {
                    wxSDLJoyEvent ev(wxEVT_SDLJOY);
                    ev.player_index = SDL_JoystickGetPlayerIndex(joy.second.dev);
                    ev.ctrl_type     = WXSDLJOY_AXIS;
                    ev.ctrl_idx      = axis;
                    ev.ctrl_val      = val;
                    ev.prev_val      = prev_val;

                    handler->ProcessEvent(ev);

                    joy.second.axis[axis] = val;

                    wxLogDebug("POLLED SDL_JOYAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", joy.first, axis, val, prev_val);
                }
            }
        }
    }
}

void wxSDLJoy::ConnectController(int joystick_index)
{
    if (SDL_GameController* dev = SDL_GameControllerOpen(joystick_index)) {
        SDL_Joystick* dev_js = SDL_GameControllerGetJoystick(dev);
        SDL_JoystickID instance_id = SDL_JoystickInstanceID(dev_js);
        std::cout << "Connected GameController" << std::endl;

        gcstate[instance_id].dev = dev;
    }
    else if (SDL_Joystick* dev = SDL_JoystickOpen(joystick_index)) {
        SDL_JoystickID instance_id = SDL_JoystickInstanceID(dev);
        std::cout << "Connected Joystick" << std::endl;
        
        joystate[instance_id].dev = dev;
    }
    else {
        wxLogDebug("ConnectController (%d) failed: %s", joystick_index, SDL_GetError());
        return;
    }
}

void wxSDLJoy::DisconnectController(SDL_JoystickID instance_id)
{
    if (auto& dev = gcstate[instance_id].dev) {
        std::cout << "GameControllerClose " << (SDL_GameController*)dev << std::endl;

        SDL_GameControllerClose(dev);
        gcstate.erase(instance_id);
    }
    if (auto& dev = joystate[instance_id].dev) {
        std::cout << "JoystickClose " << (SDL_Joystick*)dev << std::endl;

        SDL_JoystickClose(dev);
        joystate.erase(instance_id);
    }
}

wxEvtHandler* wxSDLJoy::Attach(wxEvtHandler* handler)
{
    wxEvtHandler* prev = evthandler;
    evthandler = handler;
    return prev;
}

void wxSDLJoy::Add()
{
    for (uint8_t joy : range(0, SDL_NumJoysticks()))
        ConnectController(joy);

    add_all = true;

    return;
}

void wxSDLJoy::Remove()
{
    add_all = false;

    auto gc = gcstate.begin();

    while (gc != gcstate.end()) {
        SDL_JoystickID instance_id = gc->first;
        ++gc;
        DisconnectController(instance_id);
    }

    auto joy = joystate.begin();

    while (joy != joystate.end()) {
        SDL_JoystickID instance_id = joy->first;
        ++joy;
        DisconnectController(instance_id);
    }

    return;
}

void wxSDLJoy::SetRumble(bool do_rumble)
{
    rumbling = do_rumble;
}

void wxSDLJoy::Notify()
{
    SetRumble(rumbling);
}

wxSDLJoyDev::operator SDL_GameController*&()
{
    return dev_gc;
}

SDL_GameController*& wxSDLJoyDev::operator=(SDL_GameController* ptr)
{
    dev_gc = ptr;
    return dev_gc;
}


wxSDLJoyDev::operator SDL_Joystick*&()
{
    return dev_js;
}

SDL_Joystick*& wxSDLJoyDev::operator=(SDL_Joystick* ptr)
{
    dev_js = ptr;
    return dev_js;
}

wxSDLJoyDev::operator bool()
{
    return dev_gc != nullptr;
}

std::nullptr_t& wxSDLJoyDev::operator=(std::nullptr_t&& null_ptr)
{
    dev_gc = null_ptr;
    return null_ptr;
}
