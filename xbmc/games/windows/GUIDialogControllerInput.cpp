/*
 *      Copyright (C) 2015 Team XBMC
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

#include "GUIDialogControllerInput.h"
#include "games/addons/GameController.h"
#include "guilib/Geometry.h"
#include "guilib/GUIButtonControl.h"
#include "guilib/GUIControl.h"
#include "guilib/GUIControlGroupList.h"
#include "guilib/GUIFocusPlane.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "input/Key.h"
#include "peripherals/Peripherals.h"
#include "peripherals/devices/Peripheral.h"
#include "utils/log.h"
#include "utils/StringUtils.h"

using namespace GAME;
using namespace PERIPHERALS;

#define GROUP_LIST             996
#define BUTTON_TEMPLATE       1000
#define BUTTON_START          1001

#define AXIS_THRESHOLD  0.5f

// --- CGUIJoystickDriverHandler -----------------------------------------------

CGUIJoystickDriverHandler::CGUIJoystickDriverHandler(CGUIDialogControllerInput* dialog, CPeripheral* device)
  : m_dialog(dialog),
    m_device(device)
{
  assert(m_dialog);
}

bool CGUIJoystickDriverHandler::OnButtonMotion(unsigned int buttonIndex, bool bPressed)
{
  if (bPressed)
    return m_dialog->OnButton(m_device, buttonIndex);

  return false;
}

bool CGUIJoystickDriverHandler::OnHatMotion(unsigned int hatIndex, HatDirection direction)
{
  if (direction == HatDirectionUp    ||
      direction == HatDirectionRight ||
      direction == HatDirectionDown  ||
      direction == HatDirectionLeft)
  {
    return m_dialog->OnHat(m_device, hatIndex, direction);
  }

  return false;
}

bool CGUIJoystickDriverHandler::OnAxisMotion(unsigned int axisIndex, float position)
{
  if (position >= AXIS_THRESHOLD)
    return m_dialog->OnAxis(m_device, axisIndex);

  return false;
}

// --- CGUIDialogControllerInput -----------------------------------------------

CGUIDialogControllerInput::CGUIDialogControllerInput(void)
  : CGUIDialog(WINDOW_DIALOG_CONTROLLER_INPUT, "DialogControllerInput.xml"),
    CThread("CtrlrInput"),
    m_focusControl(NULL),
    m_promptIndex(-1)
{
  m_loadType = KEEP_IN_MEMORY;
}

void CGUIDialogControllerInput::Process(void)
{
  AddDriverHandlers();

  AbortableWait(m_inputEvent);

  ClearDriverHandlers();
}

bool CGUIDialogControllerInput::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
    case GUI_MSG_CLICKED:
    {
      if (OnClick(message.GetSenderId()))
        return true;
      break;
    }
    case GUI_MSG_FOCUSED:
    {
      int focusedControl = message.GetControlId();
      OnFocus(focusedControl);
      break;
    }
    case GUI_MSG_LOSTFOCUS:
    case GUI_MSG_UNFOCUS_ALL:
    {
      OnFocus(-1);
      break;
    }
  }

  return CGUIDialog::OnMessage(message);
}

bool CGUIDialogControllerInput::OnAction(const CAction& action)
{
  if (action.GetID() == ACTION_CONTEXT_MENU)
  {
    Close();
    return true;
  }

  return CGUIDialog::OnAction(action);
}

void CGUIDialogControllerInput::OnInitWindow(void)
{
  CGUIDialog::OnInitWindow();

  // disable the template button control
  CGUIButtonControl* pButtonTemplate = GetButtonTemplate();
  if (pButtonTemplate)
    pButtonTemplate->SetVisible(false);

  SetFocusedControl(GROUP_LIST, m_lastControlID);
}

void CGUIDialogControllerInput::OnDeinitWindow(int nextWindowID)
{
  if (m_focusControl)
    m_focusControl->Unfocus();

  // save selected item for next time
  if (m_controller)
  {
    int iFocusedControl = GetFocusedControl(GROUP_LIST);
    if (iFocusedControl >= BUTTON_START)
      m_lastControlIds[m_controller] = iFocusedControl;
  }

  CGUIDialog::OnDeinitWindow(nextWindowID);
}

void CGUIDialogControllerInput::DoModal(const GameControllerPtr& controller, CGUIFocusPlane* focusControl)
{
  if (IsDialogRunning())
    return;

  Initialize();

  if (SetupButtons(controller, focusControl))
    CGUIDialog::DoModal();

  CleanupButtons();
}

bool CGUIDialogControllerInput::OnButton(PERIPHERALS::CPeripheral* device, unsigned int buttonIndex)
{
  if (IsPrompting())
  {
    // TODO
    //mapper->MapButton(m_promptIndex, CJoystickDriverPrimitive(buttonIndex));

    CancelPrompt();

    return true;
  }

  return false;
}

bool CGUIDialogControllerInput::OnHat(PERIPHERALS::CPeripheral* device, unsigned int hatIndex, HatDirection direction)
{
  if (IsPrompting())
  {
    // TODO
    //mapper->MapButton(m_promptIndex, CJoystickDriverPrimitive(hatIndex, direction));

    CancelPrompt();

    return true;
  }

  return false;
}

bool CGUIDialogControllerInput::OnAxis(PERIPHERALS::CPeripheral* device, unsigned int axisIndex)
{
  // TODO
  return false;
}

void CGUIDialogControllerInput::OnFocus(int iFocusedControl)
{
  if (m_controller && m_focusControl)
  {
    const std::vector<GAME::CGameControllerFeature>& features = m_controller->Layout().Features();

    int iFocusedIndex = iFocusedControl - BUTTON_START;

    if (IsPrompting() && iFocusedIndex != m_promptIndex)
      CancelPrompt();

    if (0 <= iFocusedIndex && iFocusedIndex < (int)features.size())
      m_focusControl->SetFocus(features[iFocusedIndex].Geometry());
    else
      m_focusControl->Unfocus();
  }
}

bool CGUIDialogControllerInput::OnClick(int iSelectedControl)
{
  if (m_controller && m_focusControl && iSelectedControl >= BUTTON_START)
  {
    PromptForInput(iSelectedControl - BUTTON_START);
    return true;
  }

  return false;
}

void CGUIDialogControllerInput::PromptForInput(unsigned int buttonIndex)
{
  if (IsPrompting())
    return;

  const std::vector<GAME::CGameControllerFeature>& features = m_controller->Layout().Features();
  if (buttonIndex < features.size())
  {
    const GAME::CGameControllerFeature& feature = features[buttonIndex];

    // Update label
    std::string promptMsg = g_localizeStrings.Get(35051); // "Press %s"
    std::string prompt = StringUtils::Format(promptMsg.c_str(), m_controller->GetString(feature.Label()).c_str());
    SET_CONTROL_LABEL(BUTTON_START + buttonIndex, prompt);

    m_promptIndex = buttonIndex;

    m_inputEvent.Reset();
    Create();
  }
}

void CGUIDialogControllerInput::CancelPrompt(void)
{
  m_inputEvent.Set();

  if (!IsPrompting())
    return;

  const std::vector<GAME::CGameControllerFeature>& features = m_controller->Layout().Features();
  if (m_promptIndex < (int)features.size())
  {
    const GAME::CGameControllerFeature& feature = features[m_promptIndex];

    // Change label back
    SET_CONTROL_LABEL(BUTTON_START + m_promptIndex, m_controller->GetString(feature.Label()));

    m_promptIndex = -1;
  }
}

int CGUIDialogControllerInput::GetFocusedControl(int iControl)
{
  CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), iControl);

  if (CGUIWindow::OnMessage(msg))
    return msg.GetParam1() >= 0 ? msg.GetParam1() : -1;

  return -1;
}

void CGUIDialogControllerInput::SetFocusedControl(int iControl, int iFocusedControl)
{
  CGUIMessage msg(GUI_MSG_ITEM_SELECT, GetID(), iControl, iFocusedControl);
  OnMessage(msg);
}

bool CGUIDialogControllerInput::SetupButtons(const GameControllerPtr& controller, CGUIFocusPlane* focusControl)
{
  if (!controller || !focusControl)
    return false;

  CGUIButtonControl* pButtonTemplate = GetButtonTemplate();
  CGUIControlGroupList* pGroupList = dynamic_cast<CGUIControlGroupList*>(GetControl(GROUP_LIST));

  if (!pButtonTemplate || !pGroupList)
    return false;

  const std::vector<GAME::CGameControllerFeature>& features = controller->Layout().Features();

  unsigned int buttonId = BUTTON_START;
  for (std::vector<GAME::CGameControllerFeature>::const_iterator it = features.begin(); it != features.end(); ++it)
  {
    CGUIButtonControl* pButton = MakeButton(controller->GetString(it->Label()), buttonId++, pButtonTemplate);

    // Try inserting context buttons at position specified by template button,
    // if template button is not in grouplist fallback to adding new buttons at
    // the end of grouplist
    if (!pGroupList->InsertControl(pButton, pButtonTemplate))
      pGroupList->AddControl(pButton);
  }

  // Update our default control
  m_defaultControl = GROUP_LIST;
  m_lastControlID = BUTTON_START;

  m_controller = controller;
  m_focusControl = focusControl;

  // Restore last selected control
  std::map<GAME::GameControllerPtr, unsigned int>::const_iterator it = m_lastControlIds.find(m_controller);
  if (it != m_lastControlIds.end())
    m_lastControlID = it->second;

  return true;
}

void CGUIDialogControllerInput::CleanupButtons(void)
{
  CGUIControlGroupList* pGroupList = dynamic_cast<CGUIControlGroupList*>(GetControl(GROUP_LIST));
  if (pGroupList)
    pGroupList->ClearAll();

  m_controller = NULL;
  m_focusControl = NULL;
}

CGUIButtonControl* CGUIDialogControllerInput::GetButtonTemplate(void)
{
  CGUIButtonControl* pButtonTemplate = dynamic_cast<CGUIButtonControl*>(GetFirstFocusableControl(BUTTON_TEMPLATE));
  if (!pButtonTemplate)
    pButtonTemplate = dynamic_cast<CGUIButtonControl*>(GetControl(BUTTON_TEMPLATE));
  return pButtonTemplate;
}

CGUIButtonControl* CGUIDialogControllerInput::MakeButton(const std::string& strLabel,
                                                         unsigned int       id,
                                                         CGUIButtonControl* pButtonTemplate)
{
  CGUIButtonControl* pButton = new CGUIButtonControl(*pButtonTemplate);

  // Set the button's ID and position
  pButton->SetID(id);
  pButton->SetVisible(true);
  pButton->SetLabel(strLabel);
  pButton->SetPosition(pButtonTemplate->GetXPosition(), pButtonTemplate->GetYPosition());

  return pButton;
}

void CGUIDialogControllerInput::AddDriverHandlers(void)
{
  // TODO: not thread-safe, peripheral object may be deleted in another thread
  std::vector<CPeripheral*> peripherals = ScanPeripherals();

  for (std::vector<CPeripheral*>::iterator it = peripherals.begin(); it != peripherals.end(); ++it)
  {
    CGUIJoystickDriverHandler* handler = new CGUIJoystickDriverHandler(this, *it);
    m_driverHandlers.push_back(handler);
    (*it)->RegisterJoystickDriverHandler(handler);
  }
}

void CGUIDialogControllerInput::ClearDriverHandlers(void)
{
  // TODO: not thread-safe, peripheral object may be deleted in another thread
  std::vector<CPeripheral*> peripherals = ScanPeripherals();

  for (std::vector<CPeripheral*>::iterator it = peripherals.begin(); it != peripherals.end(); ++it)
  {
    CGUIJoystickDriverHandler* handler = GetDriverHandler(*it);
    if (handler)
      (*it)->UnregisterJoystickDriverHandler(handler);
  }

  for (std::vector<CGUIJoystickDriverHandler*>::iterator it = m_driverHandlers.begin(); it != m_driverHandlers.end(); ++it)
    delete *it;

  m_driverHandlers.clear();
}

CGUIJoystickDriverHandler* CGUIDialogControllerInput::GetDriverHandler(CPeripheral* peripheral) const
{
  for (std::vector<CGUIJoystickDriverHandler*>::const_iterator it = m_driverHandlers.begin(); it != m_driverHandlers.end(); ++it)
  {
    if ((*it)->Device() == peripheral)
      return *it;
  }

  return NULL;
}

std::vector<CPeripheral*> CGUIDialogControllerInput::ScanPeripherals(void)
{
  std::vector<CPeripheral*> peripherals;

  g_peripherals.GetPeripheralsWithFeature(peripherals, FEATURE_JOYSTICK);
  g_peripherals.GetPeripheralsWithFeature(peripherals, FEATURE_KEYBOARD);

  return peripherals;
}
