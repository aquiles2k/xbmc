/*
 *      Copyright (C) 2014 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "AddonJoystickButtonMapper.h"
#include "addons/AddonManager.h"
#include "addons/include/kodi_peripheral_utils.hpp"
#include "games/addons/GameController.h"
#include "peripherals/Peripherals.h"

using namespace ADDON;
using namespace GAME;
using namespace PERIPHERALS;

CAddonJoystickButtonMapper::CAddonJoystickButtonMapper(CPeripheral* device, const std::string& strControllerId)
  : m_device(device),
    m_addon(g_peripherals.GetAddon(device)),
    m_strControllerId(strControllerId)
{
}

bool CAddonJoystickButtonMapper::Load(void)
{
  if (m_addon)
  {
    AddonPtr addon;
    if (CAddonMgr::Get().GetAddon(m_strControllerId, addon, ADDON_GAME_CONTROLLER))
      m_controller = std::dynamic_pointer_cast<CGameController>(addon);
  }

  return m_controller.get() != NULL;
}

bool CAddonJoystickButtonMapper::MapButton(unsigned int featureIndex, const CJoystickDriverPrimitive& primitive)
{
  bool retVal(false);

  const std::string& strFeatureName = GetFeatureName(featureIndex);
  if (!strFeatureName.empty())
  {
    switch (primitive.Type())
    {
      case DriverPrimitiveTypeButton:
      {
        ADDON::DriverButton driverButton(featureIndex, strFeatureName, primitive.Index());
        retVal = m_addon->MapJoystickFeature(m_device, m_strControllerId, &driverButton);
        break;
      }
      case DriverPrimitiveTypeHatDirection:
      {
        ADDON::DriverHat driverHat(featureIndex, strFeatureName, primitive.Index(), ToHatDirection(primitive.HatDir()));
        retVal = m_addon->MapJoystickFeature(m_device, m_strControllerId, &driverHat);
        break;
      }
      case DriverPrimitiveTypeSemiAxis:
      {
        ADDON::DriverSemiAxis driverSemiAxis(featureIndex, strFeatureName, primitive.Index(), ToSemiAxisDirection(primitive.SemiAxisDir()));
        retVal = m_addon->MapJoystickFeature(m_device, m_strControllerId, &driverSemiAxis);
        break;
      }
      default:
        break;
    }
  }

  return retVal;
}

bool CAddonJoystickButtonMapper::MapAnalogStick(unsigned int featureIndex,
                                                int horizIndex, bool horizInverted,
                                                int vertIndex,  bool vertInverted)
{
  bool retVal(false);

  const std::string& strFeatureName = GetFeatureName(featureIndex);
  if (!strFeatureName.empty())
  {
    ADDON::DriverAnalogStick driverAnalogStick(featureIndex, strFeatureName,
                                               horizIndex, horizInverted,
                                               vertIndex,  vertInverted);

    retVal = m_addon->MapJoystickFeature(m_device, m_strControllerId, &driverAnalogStick);
  }

  return retVal;
}

bool CAddonJoystickButtonMapper::MapAccelerometer(unsigned int featureIndex,
                                                  int xIndex, bool xInverted,
                                                  int yIndex, bool yInverted,
                                                  int zIndex, bool zInverted)
{
  bool retVal(false);

  const std::string& strFeatureName = GetFeatureName(featureIndex);
  if (!strFeatureName.empty())
  {
    ADDON::DriverAccelerometer driverAccelerometer(featureIndex, strFeatureName,
                                                   xIndex, xInverted,
                                                   yIndex, yInverted,
                                                   zIndex, zInverted);

    retVal = m_addon->MapJoystickFeature(m_device, m_strControllerId, &driverAccelerometer);
  }

  return retVal;
}

const std::string& CAddonJoystickButtonMapper::GetFeatureName(unsigned int featureIndex) const
{
  if (m_controller)
  {
    const std::vector<CGameControllerFeature>& features = m_controller->Layout().Features();
    if (featureIndex < features.size())
      return features[featureIndex].Name();
  }

  static std::string empty;
  return empty;
}

JOYSTICK_DRIVER_HAT_DIRECTION CAddonJoystickButtonMapper::ToHatDirection(HatDirection dir)
{
  switch (dir)
  {
    case HatDirectionLeft:   return JOYSTICK_DRIVER_HAT_LEFT;
    case HatDirectionRight:  return JOYSTICK_DRIVER_HAT_RIGHT;
    case HatDirectionUp:     return JOYSTICK_DRIVER_HAT_UP;
    case HatDirectionDown:   return JOYSTICK_DRIVER_HAT_DOWN;
    default:                 break;
  }
  return JOYSTICK_DRIVER_HAT_UNKNOWN;
}

JOYSTICK_DRIVER_SEMIAXIS_DIRECTION CAddonJoystickButtonMapper::ToSemiAxisDirection(SemiAxisDirection dir)
{
  switch (dir)
  {
    case SemiAxisDirectionNegative: return JOYSTICK_DRIVER_SEMIAXIS_DIRECTION_NEGATIVE;
    case SemiAxisDirectionPositive: return JOYSTICK_DRIVER_SEMIAXIS_DIRECTION_POSITIVE;
    default:                        break;
  }
  return JOYSTICK_DRIVER_SEMIAXIS_DIRECTION_UNKNOWN;
}
