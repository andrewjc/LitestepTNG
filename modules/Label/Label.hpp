//-------------------------------------------------------------------------------------------------
// /Label/Label.hpp
// The nModules Project
//
// Declaration of the Label class.
//-------------------------------------------------------------------------------------------------
#pragma once

#include "../ModuleKit/Drawable.hpp"
#include "../ModuleKit/StateRender.hpp"
#include "../ModuleKit/Window.hpp"

#include <forward_list>
#include <unordered_map>

class Label : public Drawable {
public:
  explicit Label(LPCWSTR name);
  Label(LPCWSTR name, Drawable * parent);
  ~Label();

  // IDrawable
private:
  LRESULT WINAPI HandleMessage(HWND, UINT, WPARAM, LPARAM, LPVOID) override;

public:
  enum class States {
    Base,
    Hover,
    Pressed,
    Count
  };

private:
  void Initalize();

private:
  StateRender<States> mStateRender;
  std::forward_list<Label> mOverlays;
  int mButtonsPressed;
};

