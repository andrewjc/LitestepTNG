//-------------------------------------------------------------------------------------------------
// /Icon/TileSettings.hpp
// The nModules Project
//
// Contains all the settings used by an icon tile.
//-------------------------------------------------------------------------------------------------
#pragma once

#include "Tile.hpp"

#include "../ModuleKit/Settings.hpp"
#include "../ModuleKit/StateRender.hpp"
#include "../ModuleKit/WindowSettings.hpp"

class TileSettings {
public:
  WindowSettings mTileWindowSettings;
  StateRender<Tile::State> mTileStateRender;
  int mIconSize;
  float mGhostOpacity;

public:
  void Load(Settings *settings);
};

