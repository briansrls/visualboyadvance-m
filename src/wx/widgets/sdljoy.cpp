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
                auto joy_iid = e.cbutton.which;

                SDL_GameController* gc = SDL_GameControllerFromInstanceID(joy_iid);

                int joy_index = -1;

                for (auto&& joy : joystate) {
                    if (joy.second.dev == gc) {
                        joy_index = joy.first;
                        break;
                    }
                }

                if (!SDL_IsGameController(joy_index))
                    break;

                if (contains(joystate, joy_index)) {
                    auto but      = e.cbutton.button;
                    auto val      = e.cbutton.state;
                    auto prev_val = joystate[joy_index].button[but];

                    if (handler && val != prev_val) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.joy           = joy_index;
                        ev.ctrl_type     = WXSDLJOY_BUTTON;
                        ev.ctrl_idx      = but;
                        ev.ctrl_val      = val;
                        ev.prev_val      = prev_val;

                        handler->ProcessEvent(ev);
                    }

                    joystate[joy_index].button[but] = val;

                    wxLogDebug("GOT SDL_CONTROLLERBUTTON: joy:%d but:%d val:%d prev_val:%d", joy_index, but, val, prev_val);
                }

                got_event = true;

                break;
            }
            case SDL_CONTROLLERAXISMOTION:
            {
                auto joy_iid = e.caxis.which;

                SDL_GameController* gc = SDL_GameControllerFromInstanceID(joy_iid);

                int16_t joy_index = -1;

                for (auto&& joy : joystate) {
                    if (joy.second.dev == gc) {
                        joy_index = joy.first;
                        break;
                    }
                }

                if (!SDL_IsGameController(joy_index))
                    break;

                if (contains(joystate, joy_index)) {
                    auto axis     = e.caxis.axis;
                    auto val      = axisval(e.caxis.value);
                    auto prev_val = joystate[joy_index].axis[axis];

                    if (handler && val != prev_val) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.joy           = joy_index;
                        ev.ctrl_type     = WXSDLJOY_AXIS;
                        ev.ctrl_idx      = axis;
                        ev.ctrl_val      = val;
                        ev.prev_val      = prev_val;

                        handler->ProcessEvent(ev);

                        joystate[joy_index].axis[axis] = val;

                        wxLogDebug("GOT SDL_CONTROLLERAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", joy_index, axis, val, prev_val);
                    }
                }

                got_event = true;

                break;
            }
            case SDL_CONTROLLERDEVICEADDED:
            {
                //Game Controller index for ADDED
                auto joy_index = e.cdevice.which;
                std::cout << "CONTROLLERDEVICEADDED Joy index = " << joy_index << std::endl;

                if (!SDL_IsGameController(joy_index))
                    break;

                if (add_all || contains(joystate, joy_index)) {
                    DisconnectController(joy_index);
                    ConnectController(joy_index);

                    systemScreenMessage(wxString::Format(_("Connected game controller %d"), joy_index + 1));
                }

                got_event = true;

                break;
            }
            case SDL_CONTROLLERDEVICEREMAPPED:
            {
                //Game Controller Instance ID
                auto joy_iid = e.cdevice.which;
                std::cout << "CONTROLLERDEVICEREMOVED Joy IID = " << joy_iid << std::endl;

                SDL_GameController* gc = SDL_GameControllerFromInstanceID(joy_iid);

                for (auto&& joy : joystate) {
                    if (joy.second.dev == gc) {
                        DisconnectController(joy.first);
                        ConnectController(joy.first);

                        systemScreenMessage(wxString::Format(_("Connected game controller %d"), joy.first + 1));
                        break;
                    }
                }

                got_event = true;

                break;
            }
            case SDL_CONTROLLERDEVICEREMOVED:
            {
                //Joystick Instance ID
                auto joy_iid = e.cdevice.which;
                std::cout << "CONTROLLERDEVICEREMOVED Joy IID = " << joy_iid << std::endl;

                SDL_GameController* gc = SDL_GameControllerFromInstanceID(joy_iid);

                for (auto&& joy : joystate) {
                    if (joy.second.dev == gc) {
                        systemScreenMessage(wxString::Format(_("Disconnected game controller %d"), joy.first + 1));

                        DisconnectController(joy.first);

                        break;
                    }
                }

                got_event = true;

                break;
            }

            // Joystck events for non-GameControllers.

            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
            {
                auto joy_iid = e.jbutton.which;

                SDL_Joystick* js = SDL_JoystickFromInstanceID(joy_iid);

                int joy_index = -1;

                for (auto&& joy : joystate) {
                    if (joy.second.dev == js) {
                        joy_index = joy.first;
                        break;
                    }
                }

                if (SDL_IsGameController(joy_index))
                    break;

                if (contains(joystate, joy_index)) {
                    auto but      = e.jbutton.button;
                    auto val      = e.jbutton.state;
                    auto prev_val = joystate[joy_index].button[but];

                    if (handler && val != prev_val) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.joy           = joy_index;
                        ev.ctrl_type     = WXSDLJOY_BUTTON;
                        ev.ctrl_idx      = but;
                        ev.ctrl_val      = val;
                        ev.prev_val      = prev_val;

                        handler->ProcessEvent(ev);
                    }

                    joystate[joy_index].button[but] = val;

                    wxLogDebug("GOT SDL_JOYBUTTON: joy:%d but:%d val:%d prev_val:%d", joy_index, but, val, prev_val);
                }

                got_event = true;

                break;
            }
            case SDL_JOYAXISMOTION:
            {
                auto joy_iid = e.jaxis.which;

                SDL_Joystick* js = SDL_JoystickFromInstanceID(joy_iid);

                int joy_index = -1;

                for (auto&& joy : joystate) {
                    if (joy.second.dev == js) {
                        joy_index = joy.first;
                        break;
                    }
                }

                if (SDL_IsGameController(joy_index))
                    break;

                if (contains(joystate, joy_index)) {
                    auto axis     = e.jaxis.axis;
                    auto val      = axisval(e.jaxis.value);
                    auto prev_val = joystate[joy_index].axis[axis];

                    if (handler && val != prev_val) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.joy           = joy_index;
                        ev.ctrl_type     = WXSDLJOY_AXIS;
                        ev.ctrl_idx      = axis;
                        ev.ctrl_val      = val;
                        ev.prev_val      = prev_val;

                        handler->ProcessEvent(ev);

                        joystate[joy_index].axis[axis] = val;

                        wxLogDebug("GOT SDL_JOYAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", joy_index, axis, val, prev_val);
                    }
                }

                got_event = true;

                break;
            }
            case SDL_JOYDEVICEADDED:
            {
                //Joystick device index
                auto joy_index = e.jdevice.which;
                std::cout << "JOYDEVICEADDED Joy index = " << joy_index << std::endl;

                if (SDL_IsGameController(joy_index))
                    break;

                if (add_all || contains(joystate, joy_index)) {
                    DisconnectController(joy_index);
                    ConnectController(joy_index);

                    systemScreenMessage(wxString::Format(_("Connected joystick %d"), joy_index + 1));
                }

                got_event = true;

                break;
            }
            case SDL_JOYDEVICEREMOVED:
            {
                //InstanceID
                auto joy_iid = e.jdevice.which;
                std::cout << "JOYDEVICEREMOVED Joy IID = " << joy_iid << std::endl;
                SDL_Joystick* js = SDL_JoystickFromInstanceID(joy_iid);

                for (auto&& joy : joystate) {
                    if (joy.second.dev == js) {
                        DisconnectController(joy.first);

                        systemScreenMessage(wxString::Format(_("Disconnected joystick %d"), joy.first + 1));
                        break;
                    }
                }

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
        for (auto&& joy : joystate) {
            if (!joy.second.dev) continue;

            if (SDL_IsGameController(joy.first)) {
                for (uint8_t but = 0; but < SDL_CONTROLLER_BUTTON_MAX; but++) {
                    auto last_state = joy.second.button[but];
                    auto state      = SDL_GameControllerGetButton(joy.second.dev, static_cast<SDL_GameControllerButton>(but));

                    if (last_state != state) {
                        if (handler) {
                            wxSDLJoyEvent ev(wxEVT_SDLJOY);
                            ev.joy           = joy.first;
                            ev.ctrl_type     = WXSDLJOY_BUTTON;
                            ev.ctrl_idx      = but;
                            ev.ctrl_val      = state;
                            ev.prev_val      = last_state;

                            handler->ProcessEvent(ev);
                        }

                        joy.second.button[but] = state;

                        wxLogDebug("POLLED SDL_CONTROLLERBUTTON: joy:%d but:%d val:%d prev_val:%d", joy.first, but, state, last_state);
                    }
                }

                for (uint8_t axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
                    auto val      = axisval(SDL_GameControllerGetAxis(joy.second.dev, static_cast<SDL_GameControllerAxis>(axis)));
                    auto prev_val = joy.second.axis[axis];

                    if (handler && val != prev_val) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.joy           = joy.first;
                        ev.ctrl_type     = WXSDLJOY_AXIS;
                        ev.ctrl_idx      = axis;
                        ev.ctrl_val      = val;
                        ev.prev_val      = prev_val;

                        handler->ProcessEvent(ev);

                        joy.second.axis[axis] = val;

                        wxLogDebug("POLLED SDL_CONTROLLERAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", joy.first, axis, val, prev_val);
                    }
                }
            }
            else {
                for (uint8_t but = 0; but < SDL_JoystickNumButtons(joy.second.dev); but++) {
                    auto last_state = joy.second.button[but];
                    auto state      = SDL_JoystickGetButton(joy.second.dev, but);

                    if (last_state != state) {
                        if (handler) {
                            wxSDLJoyEvent ev(wxEVT_SDLJOY);
                            ev.joy           = joy.first;
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
                        ev.joy           = joy.first;
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
}

void wxSDLJoy::ConnectController(uint8_t joy)
{
    if (SDL_IsGameController(joy)) {
        if (!(joystate[joy].dev = SDL_GameControllerOpen(joy))) {
            wxLogDebug("SDL_GameControllerOpen(%d) failed: %s", joy, SDL_GetError());
            return;
        }
    }
    else {
        if (!(joystate[joy].dev = SDL_JoystickOpen(joy))) {
            wxLogDebug("SDL_JoystickOpen(%d) failed: %s", joy, SDL_GetError());
            return;
        }
    }

    std::cout << "ConnectController " << (SDL_GameController*)joystate[joy].dev << std::endl;
}

void wxSDLJoy::DisconnectController(uint8_t joy)
{
    if (auto& dev = joystate[joy].dev) {
        if (SDL_IsGameController(joy)) {
            if (SDL_GameControllerGetAttached(dev)) {
                std::cout << "GameControllerClose " << (SDL_GameController*)dev << std::endl;
                SDL_GameControllerClose(dev);
            }
        }
        else {
            if (SDL_JoystickGetAttached(dev)) {
                std::cout << "JoystickClose " << (SDL_Joystick*)dev << std::endl;
                SDL_JoystickClose(dev);
            }
        }

        joystate.erase(joy);
    }
}

wxEvtHandler* wxSDLJoy::Attach(wxEvtHandler* handler)
{
    wxEvtHandler* prev = evthandler;
    evthandler = handler;
    return prev;
}

void wxSDLJoy::Add(int8_t joy_n)
{
    if (joy_n < 0) {
        for (uint8_t joy : range(0, SDL_NumJoysticks()))
            ConnectController(joy);

        add_all = true;

        return;
    }

    ConnectController(joy_n);
}

void wxSDLJoy::Remove(int8_t joy_n)
{
    add_all = false;

    if (joy_n < 0) {
        for (auto&& joy : joystate)
            DisconnectController(joy.first);

        joystate.clear();

        return;
    }

    DisconnectController(joy_n);
    joystate.erase(joy_n);
}

void wxSDLJoy::SetRumble(bool do_rumble)
{
    rumbling = do_rumble;

#if SDL_VERSION_ATLEAST(2, 0, 9)
    // Do rumble only on device 0, and only if it's a GameController.
    auto dev = joystate[0].dev;
    if (dev && SDL_IsGameController(0)) {
        if (rumbling) {
            SDL_GameControllerRumble(dev, 0xFFFF, 0xFFFF, 300);
            if (!IsRunning())
                Start(150);
        }
        else {
            SDL_GameControllerRumble(dev, 0, 0, 0);
            Stop();
        }
    }
#endif
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
