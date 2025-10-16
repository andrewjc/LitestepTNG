//-------------------------------------------------------------------------------------------------
// /Calendar/Calendar.hpp
// The nModules Project
//
// A calendar.
//-------------------------------------------------------------------------------------------------
#pragma once

#include "../ModuleKit/Drawable.hpp"
#include "../ModuleKit/StateRender.hpp"

class Calendar : public Drawable {
private:
  enum class State {
    Base,
    Count
  };

public:
  explicit Calendar(LPCWSTR calendarName);
    
  // MessageHandler
public:
  LRESULT WINAPI HandleMessage(HWND window, UINT msg, WPARAM wParam, LPARAM lParam, LPVOID) override;

private:
  StateRender<State> mStateRender;
};

