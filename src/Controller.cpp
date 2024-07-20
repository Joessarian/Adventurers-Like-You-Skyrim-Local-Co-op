#include "Controller.h"
#include <GlobalCoopData.h>
#include <PlayerActionInfoHolder.h>
#include <unordered_map>
#include <Xinput.h>
#pragma comment (lib, "xinput.lib")

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	ControllerDataHolder::ControllerDataHolder()
	{
		// Set up LS, RS, and button state data structures.
		lsStatesList.fill(AnalogStickState());
		rsStatesList.fill(AnalogStickState());
		inputMasksList.fill(0);
		inputStatesList.fill(std::vector<InputState>(!InputAction::kInputTotal, InputState()));
		firstPressTPsList.fill(std::vector<SteadyClock::time_point>(!InputAction::kInputTotal, SteadyClock::now()));
		lastReleaseTPsList.fill(std::vector<SteadyClock::time_point>(!InputAction::kInputTotal, SteadyClock::now()));
	}

	std::vector<uint32_t> ControllerDataHolder::SetupConnectedCoopControllers()
	{
		logger::debug("[CDH] SetupConnectedCoopControllers");
		// Called in papyrus script before summoning players.
		std::vector<uint32_t> controllerIDs;
		// If P1's CID is not set yet, return empty list to end co-op.
		// P1's CID must be set before other players' CIDs are checked.
		if (glob.player1CID == -1)
		{
			RE::DebugMessageBox("[ALYSLC] Player 1's controller ID has not been assigned.\nTry summoning again or assign Player 1's controller ID through the co-op debug menu before summoning.");
			return controllerIDs;
		}

		XINPUT_STATE inputState;
		ZeroMemory(&inputState, sizeof(XINPUT_STATE));
		uint32_t controllerIndex = 0;
		// NOTE: P1's CID must always be first.
		controllerIDs.push_back(glob.player1CID);
		while (controllerIndex < Settings::fMaxNumControllers)
		{
			if (controllerIndex != glob.player1CID)
			{
				auto errorNum = XInputGetState(controllerIndex, &inputState);
				if (errorNum == ERROR_SUCCESS)
				{
					logger::debug("[CDH] SetupConnectedCoopControllers: Co-op player controller {} has been registered.", controllerIndex);
					controllerIDs.push_back(controllerIndex);
				}
				else
				{
					logger::debug("[CDH] SetupConnectedCoopControllers: No controller connected at index {}. Error #{}.", controllerIndex, errorNum);
				}
			}

			controllerIndex++;
		}

		return controllerIDs;
	}

	void ControllerDataHolder::UpdateAnalogStickState(const int32_t& a_controllerID, const int32_t& a_playerID, const bool& a_isLS, const bool& a_isControllingMenus)
	{
		// Code snippet adapted from here: https://docs.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput
		XINPUT_STATE inputState;
		ZeroMemory(&inputState, sizeof(XINPUT_STATE));
		if (XInputGetState(a_controllerID, &inputState) != ERROR_SUCCESS)
		{
			if (a_controllerID > -1 && a_controllerID < ALYSLC_MAX_PLAYER_COUNT && 
				glob.coopSessionActive && glob.coopPlayers[a_controllerID]->isActive)
			{
				logger::error("[CDH] ERR: UpdateAnalogStickState: Could not get input state for active controller {}", a_controllerID);
			}

			return;
		}

		if (a_playerID <= -1) 
		{
			logger::error("[CDH] ERR: UpdateAnalogStickState: Invalid player ID ({}) for CID {}.", a_playerID, a_controllerID);
			return;
		}

		auto& data = (a_isLS) ? lsStatesList[a_controllerID] : rsStatesList[a_controllerID];
		data.secsSincePrevStateSet = Util::GetElapsedSeconds(data.setPrevStateTP);
		if (data.secsSincePrevStateSet > *g_deltaTimeRealTime)
		{
			data.prevNormMag = data.normMag;
			data.setPrevStateTP = SteadyClock::now();
		}

		// Larger deadzone when controlling menus to prevent slight analog stick displacements from
		// changing the currently selected menu element.
		float deadZone = 
		(
			SHRT_MAX * (a_isControllingMenus ? 
			min(0.5f, Settings::vfAnalogDeadzoneRatio[a_playerID]) : 
			Settings::vfAnalogDeadzoneRatio[a_playerID])
		);
		data.maxMag = SHRT_MAX - deadZone;
		float xDisp, yDisp = 0.0f;
		if (a_isLS)
		{
			xDisp = inputState.Gamepad.sThumbLX;
			yDisp = inputState.Gamepad.sThumbLY;
		}
		else
		{
			xDisp = inputState.Gamepad.sThumbRX;
			yDisp = inputState.Gamepad.sThumbRY;
		}

		float tempMag = sqrtf(xDisp * xDisp + yDisp * yDisp);
		float xComp = 0.0f;
		float yComp = 0.0f;
		float newNormMag = 0.0f;
		if (tempMag > 0.0f)
		{
			if (tempMag > deadZone)
			{
				xComp = xDisp / tempMag;
				yComp = yDisp / tempMag;
				if (tempMag > SHRT_MAX)
				{
					tempMag = SHRT_MAX;
				}

				tempMag -= deadZone;
				newNormMag = data.maxMag > 0.0f ? min(1.0f, tempMag / data.maxMag) : 0.0f;
			}
		}

		// Factor in deadzone.
		if (xDisp >= 0.0f)
		{
			xDisp = max(0.0f, xDisp - deadZone);
		}
		else
		{
			xDisp = min(0.0f, xDisp + deadZone);
		}

		if (yDisp >= 0.0f)
		{
			yDisp = max(0.0f, yDisp - deadZone);
		}
		else
		{
			yDisp = min(0.0f, yDisp + deadZone);
		}

		float newNormXPos = xComp * newNormMag;
		float newNormYPos = yComp * newNormMag;
		float oldNormXPos = data.xComp * data.normMag;
		float oldNormYPos = data.yComp * data.normMag;

		// Moving to/from center (no angular change in orientation).
		if ((newNormXPos == 0.0f && newNormYPos == 0.0f) || (oldNormXPos == 0.0f && oldNormYPos == 0.0f))
		{
			data.stickAngularSpeed = 0.0f;
		}
		else
		{
			data.stickAngularSpeed = 
			(
				abs(Util::NormalizeAngToPi(atan2(newNormYPos, newNormXPos) - atan2(oldNormYPos, oldNormXPos)) / *g_deltaTimeRealTime)
			);
		}

		data.stickLinearSpeed = Util::GetXYDistance(newNormXPos, newNormYPos, oldNormXPos, oldNormYPos) / *g_deltaTimeRealTime;
		data.xComp = xComp;
		data.yComp = yComp;
		data.normMag = newNormMag;

		if (data.normMag != data.prevNormMag)
		{
			data.prevNormMag = data.normMag;
		}
	}

	void ControllerDataHolder::UpdateInputStatesAndMask(const int32_t& a_controllerID, const int32_t& a_playerID)
	{
		XINPUT_STATE inputState;
		ZeroMemory(&inputState, sizeof(XINPUT_STATE));
		if (XInputGetState(a_controllerID, &inputState) != ERROR_SUCCESS)
		{
			if (a_controllerID > -1 && a_controllerID < ALYSLC_MAX_PLAYER_COUNT && 
				glob.coopSessionActive && glob.coopPlayers[a_controllerID]->isActive)
			{
				logger::error("[CDH] ERR: UpdateInputStatesAndMask: Could not get input state for active controller {}", a_controllerID);
			}

			return;
		}

		if (a_playerID <= -1) 
		{
			logger::error("[CDH] ERR: UpdateInputStatesAndMask: Invalid player ID ({}) for CID {}.", a_playerID, a_controllerID);
			return;
		}

		const auto& paInfo = glob.paInfoHolder;
		auto& inputStates = inputStatesList[a_controllerID];
		const auto prevMask = inputMasksList[a_controllerID];
		auto& currentMask = inputMasksList[a_controllerID];
		auto& firstPressTPs = firstPressTPsList[a_controllerID];
		auto& lastReleaseTPs = lastReleaseTPsList[a_controllerID];

		uint32_t dxsc = FIRST_CTRLR_DXSC;
		bool buttonPressed = false;
		bool isButton = true;
		bool ltPressed = false, rtPressed = false, lsMoved = false, rsMoved = false;
		const BYTE triggerDeadzone = static_cast<BYTE>(UCHAR_MAX * Settings::vfTriggerDeadzoneRatio[a_playerID]);
		for (uint32_t i = !InputAction::kFirst; i < !InputAction::kInputTotal; ++i, ++dxsc) 
		{
			isButton = i != !InputAction::kLS && i != !InputAction::kRS && dxsc != DXSC_LT && dxsc != DXSC_RT;
			buttonPressed = isButton && (inputState.Gamepad.wButtons & DXSC_TO_XIMASK.at(dxsc)) != 0;
			ltPressed = dxsc == DXSC_LT && inputState.Gamepad.bLeftTrigger > triggerDeadzone;
			rtPressed = dxsc == DXSC_RT && inputState.Gamepad.bRightTrigger > triggerDeadzone;
			lsMoved = i == !InputAction::kLS && GetAnalogStickState(a_controllerID, true).normMag > 0.0f;
			rsMoved = i == !InputAction::kRS && GetAnalogStickState(a_controllerID, false).normMag > 0.0f;
				
			// Button/trigger pressed or analog stick moved.
			if (buttonPressed || ltPressed || rtPressed || lsMoved || rsMoved) 
			{
				auto& state = inputStates[i];
				auto& firstPressTP = firstPressTPs[i];
				auto& lastReleaseTP = lastReleaseTPs[i];

				// First press.
				if (!state.isPressed) 
				{
					state.isPressed = true;
					state.pressedMag = 1.0f;
					// Only inc consecutive presses to > 1 if pressed again within between-presses threshold.
					float secsSinceLastRelease = Util::GetElapsedSeconds(lastReleaseTP);
					state.consecPresses = 
					(
						secsSinceLastRelease <= Settings::fMaxFramesBetweenConsecTaps * *g_deltaTimeRealTime ? 
						state.consecPresses + 1 : 
						1
					);
					currentMask |= 1 << i;
					firstPressTP = SteadyClock::now();
				}

				// Update held time.
				state.heldTimeSecs = Util::GetElapsedSeconds(firstPressTP);

				// Set trigger press magnitudes if pressed.
				// Normalize based on trigger deadzone threshold.
				if (ltPressed)
				{
					state.pressedMag = (inputState.Gamepad.bLeftTrigger - triggerDeadzone) / (UCHAR_MAX - triggerDeadzone);
				}
				else if (rtPressed)
				{
					state.pressedMag = (inputState.Gamepad.bRightTrigger - triggerDeadzone) / (UCHAR_MAX - triggerDeadzone);
				}
			}
			// Button/trigger not pressed or analog stick centered.
			else
			{
				auto& state = inputStates[i];
				// Just released.
				if (state.isPressed) 
				{
					state.isPressed = false;
					state.pressedMag = 0.0f;
					currentMask &= ~(1 << i);
					lastReleaseTPs[i] = SteadyClock::now();
				}

				// Reset consecutive presses to 1 if time between presses exceeds limit.
				if (state.consecPresses > 0) 
				{
					const auto& firstPressTP = firstPressTPs[i];
					float secsSinceLastPress = Util::GetElapsedSeconds(firstPressTP);
					if (secsSinceLastPress > Settings::fMaxFramesBetweenConsecTaps * *g_deltaTimeRealTime)
					{
						state.consecPresses = 0;
					}
				}
			}
		}
	}

	void ControllerDataHolder::UpdatePlayerControllerStates()
	{
		if (!glob.globalDataInit) 
		{
			return;
		}

		if (glob.coopSessionActive) 
		{
			auto ui = RE::UI::GetSingleton();
			bool isControllingMenus = false;
			for (const auto& p : glob.coopPlayers) 
			{
				if (p->isActive) 
				{
					isControllingMenus = p->pam->isControllingUnpausedMenu || (ui && ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME));
					UpdateAnalogStickState(p->controllerID, p->playerID, true, isControllingMenus);
					UpdateAnalogStickState(p->controllerID, p->playerID, false, isControllingMenus);
					UpdateInputStatesAndMask(p->controllerID, p->playerID);
				}
			}
		}
		else
		{
			uint32_t i = 0;
			while (i < ALYSLC_MAX_PLAYER_COUNT)
			{
				UpdateAnalogStickState(i, i, true, false);
				UpdateAnalogStickState(i, i, false, false);
				UpdateInputStatesAndMask(i, i);
				++i;
			}
		}
	}
};

