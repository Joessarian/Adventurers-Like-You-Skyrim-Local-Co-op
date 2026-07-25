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
		activeControllerCount = 0;
		// Set up LS, RS, and button state data structures.
		lsStatesList.fill(AnalogStickState());
		rsStatesList.fill(AnalogStickState());
		inputMasksList.fill(0);
		inputStatesList.fill(std::vector<InputState>(!InputAction::kInputTotal, InputState()));
		firstPressTPsList.fill
		(
			std::vector<SteadyClock::time_point>(!InputAction::kInputTotal, SteadyClock::now())
		);
		lastReleaseTPsList.fill
		(
			std::vector<SteadyClock::time_point>(!InputAction::kInputTotal, SteadyClock::now())
		);
	}

	std::vector<uint32_t> ControllerDataHolder::SetupConnectedInputDevices()
	{
		// Get and return a list of all co-op usable input devices' IDs.
		// NOTE: 
		// Called in papyrus script before summoning players.
		
		std::vector<uint32_t> inputDeviceIDs{ };
		// If P1's DID is not set yet, return an empty list to end co-op.
		// P1's DID must be set before other players' DIDs are checked.
		if (glob.player1DID == -1)
		{
			RE::DebugMessageBox
			(
				"[ALYSLC]\nPlayer 1's device ID has not been assigned "
				"before setting all co-op input devices.\n"
				"Try summoning again or assign Player 1's controller ID "
				"through the Debug Menu before summoning:\n"
				"1. Hold the 'Pause/Journal' bind.\n"
				"2. Press and release the 'Wait' bind.\n"
				"3. Select 'Miscellaneous Options'.\n"
				"4. Select 'Assign Player 1 Controller ID'."
			);
			return inputDeviceIDs;
		}

		// NOTE:
		// P1's DID is always first.
		// Either a controller or keyboard + mouse.
		inputDeviceIDs.push_back(glob.player1DID);
		DBG
		(
			"P1 is using {}. DID: {}.", 
			glob.player1DID == -1 ? 
			"NONE" :
			glob.player1DID >= ALYSLC_MAX_CONTROLLER_COUNT ?
			"the keyboard + mouse" :
			"a controller",
			glob.player1DID
		);

		// Check for controllers first.
		XINPUT_STATE inputState{ };
		ZeroMemory(&inputState, sizeof(XINPUT_STATE));
		uint32_t controllerIndex = 0;
		while (controllerIndex < ALYSLC_MAX_CONTROLLER_COUNT)
		{
			if (controllerIndex != glob.player1DID)
			{
				auto errorNum = XInputGetState(controllerIndex, &inputState);
				if (errorNum == ERROR_SUCCESS)
				{
					INF
					(
						"Co-op player controller {} has been registered.", controllerIndex
					);
					inputDeviceIDs.push_back(controllerIndex);
				}
				else
				{
					DBG
					(
						"No controller connected at index {}. Result #{}.", 
						controllerIndex, errorNum
					);
				}
			}

			controllerIndex++;
		}
		
		// 0 controllers plugged in.
		if (auto numInputDevices = inputDeviceIDs.size(); numInputDevices <= 1)
		{
			// TODO: 
			// Via RawInput, check for multiple keyboards and mice and assign their device IDs here
			// if no controllers are plugged in.
			if (numInputDevices == 1 && inputDeviceIDs[0] >= ALYSLC_MAX_CONTROLLER_COUNT)
			{
				DBG("Only the keyboard and mouse is connected. Cannot start co-op.");
			}

			// For now, return an empty list since at least two input devices must be connected.
			return std::vector<uint32_t>();
		}
		else if (numInputDevices == 2 && inputDeviceIDs[0] >= ALYSLC_MAX_CONTROLLER_COUNT)
		{
			// Hybrid mode active when only 1 controller is plugged in.
			DBG("Hybrid mode active. Device IDs are {} and {}.",
				inputDeviceIDs[0], inputDeviceIDs[1]);
			glob.hybridModeActive = true;
		}
		else
		{
			DBG("Hybrid mode inactive. {} input devices. First CID: {}", 
				inputDeviceIDs.size(), inputDeviceIDs[0]);
			glob.hybridModeActive = false;
		}

		return inputDeviceIDs;
	}

	void ControllerDataHolder::UpdateAnalogStickState
	(
		const int32_t& a_controllerID, 
		const int32_t& a_playerID, 
		const bool& a_isLS, 
		const bool& a_isControllingMenus
	)
	{
		// Update the left/right analog stick data for the controller with the given ID.
		// The deadzone is modified based on the player's settings (player given by their ID) 
		// and if they are controlling menus.
		// Function was adapted from code snippets found here:
		// https://docs.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput

		XINPUT_STATE inputState{ };
		ZeroMemory(&inputState, sizeof(XINPUT_STATE));
		if (XInputGetState(a_controllerID, &inputState) != ERROR_SUCCESS)
		{
			if (a_controllerID > -1 && 
				a_controllerID < ALYSLC_MAX_PLAYER_COUNT && 
				glob.coopSessionActive &&
				glob.coopPlayers[a_controllerID]->isActive)
			{
				DBG
				(
					"Could not get input state for active controller {}", a_controllerID
				);
			}

			return;
		}

		// Invalid ID, cannot continue.
		if (a_playerID <= -1) 
		{
			return;
		}

		auto& data = (a_isLS) ? lsStatesList[a_controllerID] : rsStatesList[a_controllerID];
		// Larger deadzone when controlling menus to prevent slight analog stick displacements
		// from changing the currently-selected menu element.
		float deadZone = 
		(
			SHRT_MAX * 
			(
				a_isControllingMenus ? 
				min(0.5f, Settings::vfAnalogDeadzoneRatio[a_playerID]) : 
				Settings::vfAnalogDeadzoneRatio[a_playerID]
			)
		);
		// Maximum displacement magnitude, accounting for deadzone.
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
			// Account for deadzone now.
			if (tempMag > deadZone)
			{
				xComp = xDisp / tempMag;
				yComp = yDisp / tempMag;
				if (tempMag > SHRT_MAX)
				{
					tempMag = SHRT_MAX;
				}

				tempMag -= deadZone;
				// Normalized displacement magnitude based on the maxiumum mag.
				newNormMag = data.maxMag > 0.0f ? min(1.0f, tempMag / data.maxMag) : 0.0f;
				//newNormMag *= newNormMag;
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

		// Normalized X, Y components this and last frame.
		float newNormXPos = xComp * newNormMag;
		float newNormYPos = yComp * newNormMag;
		float oldNormXPos = data.xComp * data.normMag;
		float oldNormYPos = data.yComp * data.normMag;

		// Moving to/from center (no angular change in orientation).
		if ((newNormXPos == 0.0f && newNormYPos == 0.0f) || 
			(oldNormXPos == 0.0f && oldNormYPos == 0.0f))
		{
			data.stickAngularSpeed = 0.0f;
		}
		else
		{
			data.stickAngularSpeed = 
			(
				fabsf
				(
					Util::NormalizeAngTo2Pi
					(
						atan2f(newNormYPos, newNormXPos) - atan2f(oldNormYPos, oldNormXPos)
					) / *g_deltaTimeRealTime
				)
			);
		}

		data.stickLinearSpeed = 
		(
			Util::GetXYDistance(newNormXPos, newNormYPos, oldNormXPos, oldNormYPos) / 
			*g_deltaTimeRealTime
		);
		data.prevXComp = data.xComp;
		data.prevYComp = data.yComp;
		data.prevNormMag = data.normMag;
		data.xComp = xComp;
		data.yComp = yComp;
		data.normMag = newNormMag;
	}

	void ControllerDataHolder::UpdateInputStatesAndMask
	(
		const int32_t& a_controllerID, const int32_t& a_playerID
	)
	{
		// Update input press/release state data for the controller with the given ID.
		// The given player ID determines the deadzone for the controller.

		XINPUT_STATE inputState{ };
		ZeroMemory(&inputState, sizeof(XINPUT_STATE));
		if (XInputGetState(a_controllerID, &inputState) != ERROR_SUCCESS)
		{
			if (a_controllerID > -1 &&
				a_controllerID < ALYSLC_MAX_PLAYER_COUNT && 
				glob.coopSessionActive && 
				glob.coopPlayers[a_controllerID]->isActive)
			{
				DBG
				(
					"Could not get input state for active controller {}", a_controllerID
				);
			}

			return;
		}

		// Invalid player ID, so return early.
		if (a_playerID <= -1) 
		{
			return;
		}

		const auto& paInfo = glob.paInfoHolder;
		auto& inputStates = inputStatesList[a_controllerID];

		// Used to diff button state changes.
		const auto prevMask = inputMasksList[a_controllerID];
		auto& currentMask = inputMasksList[a_controllerID];
		auto& firstPressTPs = firstPressTPsList[a_controllerID];
		auto& lastReleaseTPs = lastReleaseTPsList[a_controllerID];

		// DXScancode for the button to check.
		uint32_t dxsc = FIRST_CTRLR_DXSC;
		bool buttonPressed = false;
		bool isAnalogStick = false;
		bool isButton = true;
		bool isTrigger = false;
		bool ltPressed = false; 
		bool rtPressed = false;
		bool lsMoved = false;
		bool rsMoved = false;
		// Get deadzone from player settings.
		const BYTE triggerDeadzone = 
		(
			static_cast<BYTE>
			(
				UCHAR_MAX * Settings::vfTriggerDeadzoneRatio[a_playerID]
			)
		);
		// Increase frame window at higher framerates
		// to ensure that the player has enough time to double tap inputs.
		uint32_t analogConsecTapFrames = max
		(
			static_cast<uint32_t>
			(
				(1.0f / (60.0f * *g_deltaTimeRealTime)) * 
				static_cast<float>(Settings::fAnalogConsecTapsFrameCountWindow)
			), 
			1
		);
		uint32_t buttonConsecTapFrames = max
		(
			static_cast<uint32_t>
			(
				(1.0f / (60.0f * *g_deltaTimeRealTime)) * 
				static_cast<float>(Settings::fButtonConsecTapsFrameCountWindow)
			), 
			1
		);
		for (uint32_t i = !InputAction::kFirst; i < !InputAction::kInputTotal; ++i, ++dxsc) 
		{
			isAnalogStick = i == !InputAction::kLS || i == !InputAction::kRS;
			isTrigger = dxsc == DXSC_LT || dxsc == DXSC_RT;
			isButton = !isAnalogStick && !isTrigger;
			buttonPressed = 
			(
				isButton && 
				(inputState.Gamepad.wButtons & DXSC_TO_XIMASK.at(dxsc)) != 0
			);
			ltPressed = dxsc == DXSC_LT && inputState.Gamepad.bLeftTrigger > triggerDeadzone;
			rtPressed = dxsc == DXSC_RT && inputState.Gamepad.bRightTrigger > triggerDeadzone;
			lsMoved = 
			(
				i == !InputAction::kLS && GetAnalogStickState(a_controllerID, true).normMag > 0.0f
			);
			rsMoved = 
			(
				i == !InputAction::kRS && GetAnalogStickState(a_controllerID, false).normMag > 0.0f
			);
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
					state.justPressed = true;
					state.pressedMag = 1.0f;
					// Only inc consecutive presses to > 1 if pressed again 
					// within between-presses threshold.
					float secsSinceLastRelease = Util::GetElapsedSeconds(lastReleaseTP);
					// Increment consecutive taps if tapped again 
					// within the frame count limit for consecutive taps.
					if (buttonPressed)
					{
						state.consecPresses = 
						(
							secsSinceLastRelease <= 
							buttonConsecTapFrames * 
							*g_deltaTimeRealTime ? 
							state.consecPresses + 1 : 
							1
						);
					}
					else
					{
						state.consecPresses = 
						(
							secsSinceLastRelease <= 
							analogConsecTapFrames * 
							*g_deltaTimeRealTime ? 
							state.consecPresses + 1 : 
							1
						);
					}
					
					// Update input mask and first press time point.
					currentMask |= 1 << i;
					firstPressTP = SteadyClock::now();
				}
				else if (state.justPressed)
				{
					// Already pressed, so clear just pressed flag.
					state.justPressed = false;
				}

				// Update held time.
				state.heldTimeSecs = Util::GetElapsedSeconds(firstPressTP);

				// Set trigger press magnitudes if pressed.
				// Normalize based on trigger deadzone threshold.
				if (ltPressed)
				{
					state.pressedMag = 
					(
						(inputState.Gamepad.bLeftTrigger - triggerDeadzone) / 
						(UCHAR_MAX - triggerDeadzone)
					);
				}
				else if (rtPressed)
				{
					state.pressedMag = 
					(
						(inputState.Gamepad.bRightTrigger - triggerDeadzone) / 
						(UCHAR_MAX - triggerDeadzone)
					);
				}
			}
			else
			{
				// Button/trigger not pressed or analog stick centered.
				auto& state = inputStates[i];
				// Just released.
				if (state.isPressed) 
				{
					state.isPressed = false;
					state.justReleased = true;
					state.pressedMag = 0.0f;
					// Clear bit flag in mask.
					currentMask &= ~(1 << i);
					lastReleaseTPs[i] = SteadyClock::now();
				}
				else if (state.justReleased)
				{
					state.justReleased = false;
				}

				// Reset consecutive presses to 1 if time between presses exceeds limit.
				if (state.consecPresses > 0) 
				{
					const auto& firstPressTP = firstPressTPs[i];
					float secsSinceLastPress = Util::GetElapsedSeconds(firstPressTP);
					bool canResetConsecPresses = 
					(
						(
							(isButton || isTrigger) && 
							(secsSinceLastPress > buttonConsecTapFrames * *g_deltaTimeRealTime) 
						) ||
						( 
							isAnalogStick && 
							secsSinceLastPress > analogConsecTapFrames * *g_deltaTimeRealTime 
						)
					);
					if (canResetConsecPresses)
					{
						state.consecPresses = 0;
					}
				}
			}
		}
	}

	void ControllerDataHolder::UpdatePlayerControllerStates()
	{
		// Update button and left and right stick data for all players.

		if (!glob.globalDataInit) 
		{
			return;
		}

		auto oldControllerCount = activeControllerCount;
		uint8_t controllerCount = 0;
		XINPUT_STATE inputState{ };
		ZeroMemory(&inputState, sizeof(XINPUT_STATE));
		for (auto i = 0; i < Settings::fMaxNumControllers; ++i)
		{
			auto errorNum = XInputGetState(i, &inputState);
			if (errorNum == ERROR_SUCCESS)
			{
				controllerCount++;
			}
		}

		activeControllerCount = controllerCount;
		// Check if a new controller was plugged in, and end any active co-op session if so.
		// Must set P1's DID again just in case P! wasn't using a controller before,
		// otherwise, triggering the summoning menu while in co-op and re-summoning players
		// will cause P1 to control multiple players.
		if (glob.coopSessionActive && activeControllerCount > oldControllerCount)
		{
			DBG
			(
				"Controller count changed: {} -> {}. "
				"Tear down co-op session and prompt for P1 DID again.",
				oldControllerCount, activeControllerCount
			);

			RE::DebugNotification
			(
				fmt::format
				(
					"[ALYSLC] New controller was plugged in. Please re-summon."
				).data()
			);
			GlobalCoopData::TearDownCoopSession(true, true);
			// Must re-assign P1 device ID upon re-summoning.
			glob.player1DID = -1;
			return;
		}

		if (glob.coopSessionActive) 
		{
			auto ui = RE::UI::GetSingleton();
			bool isControllingMenus = false;
			for (const auto& p : glob.coopPlayers) 
			{
				if (!p->isActive || p->deviceID >= ALYSLC_MAX_CONTROLLER_COUNT) 
				{
					continue;
				}

				isControllingMenus = 
				(
					(p->pam->isControllingUnpausedMenu) || 
					(ui && ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME))
				);
				UpdateAnalogStickState(p->deviceID, p->playerID, true, isControllingMenus);
				UpdateAnalogStickState(p->deviceID, p->playerID, false, isControllingMenus);
				UpdateInputStatesAndMask(p->deviceID, p->playerID);
			}
		}
		else
		{
			// Maintain updated state even when out of co-op
			// to ensure smoother transition once a co-op session starts.
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

