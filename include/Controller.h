#pragma once
#include <Enums.h>
#include <Settings.h>
#include <Util.h>
#include <unordered_map>
#include <Xinput.h>
#pragma comment (lib, "xinput.lib")

// DXScanCode constants
#define FIRST_CTRLR_DXSC	266
#define DXSC_DPAD_UP		266
#define DXSC_DPAD_DOWN		267
#define DXSC_DPAD_LEFT		268
#define DXSC_DPAD_RIGHT		269
#define DXSC_START			270
#define DXSC_BACK			271
#define DXSC_LEFT_THUMB		272
#define DXSC_RIGHT_THUMB	273
#define DXSC_LEFT_SHOULDER	274
#define DXSC_RIGHT_SHOULDER 275
#define DXSC_A				276
#define DXSC_B				277
#define DXSC_X				278
#define DXSC_Y				279
#define DXSC_LT				280
#define DXSC_RT				281
#define LAST_CTRLR_DXSC		281
#define DXSC_LMB			256
#define DXSC_RMB			257

// Additional unofficial XInput masks to cover the rest of the controller input options.
#define XMASK_LT			0x10000
#define XMASK_RT			0x20000
#define XMASK_LS			0x40000
#define XMASK_RS			0x80000

// Credits to dunc001 for posting the mask values here:
// https://www.nexusmods.com/skyrimspecialedition/mods/44160/?tab=posts
#define GAMEPAD_MASK_DPAD_UP 0x0001
#define GAMEPAD_MASK_DPAD_DOWN 0x0002
#define GAMEPAD_MASK_DPAD_LEFT 0x0004
#define GAMEPAD_MASK_DPAD_RIGHT 0x0008
#define GAMEPAD_MASK_START 0x0010
#define GAMEPAD_MASK_BACK 0x0020
#define GAMEPAD_MASK_LEFT_THUMB 0x0040
#define GAMEPAD_MASK_RIGHT_THUMB 0x0080
#define GAMEPAD_MASK_LEFT_SHOULDER 0x0100
#define GAMEPAD_MASK_RIGHT_SHOULDER 0x0200
#define GAMEPAD_MASK_A 0x1000
#define GAMEPAD_MASK_B 0x2000
#define GAMEPAD_MASK_X 0x4000
#define GAMEPAD_MASK_Y 0x8000
#define GAMEPAD_MASK_LT 0x0009
#define GAMEPAD_MASK_RT 0x000A
#define GAMEPAD_MASK_LS 0x000B
#define GAMEPAD_MASK_RS 0x000C

namespace ALYSLC
{
	using SteadyClock = std::chrono::steady_clock;
	using StickInfo = std::pair<std::pair<float, float>, std::pair<float, float>>;
	// Class containing that handles bindings between DXSCs and player actions, 
	// in addition to providing info on button press states 
	// and LS/RS positioning for all connnected controllers.
	struct ControllerDataHolder
	{
		struct AnalogStickState
		{
			AnalogStickState() :
				normMag(0.0f), prevNormMag(0.0f),
				secsSincePrevStateSet(0.0f),
				stickAngularSpeed(0.0f), stickLinearSpeed(0.0f),
				xComp(0.0f), 
				yComp(0.0f),
				maxMag(0),
				setPrevStateTP(SteadyClock::now())
			{ }

			// Analog stick is displaced fully from center.
			inline constexpr bool MaxDisplacement() const
			{
				return normMag == 1.0f;
			}

			// Analog stick was moved.
			inline constexpr bool Moved() const
			{
				return normMag != 0.0f;
			}
			
			// Analog stick was just moved from rest this frame.
			inline constexpr bool MovedFromCenter() const
			{
				return normMag != 0.0f && prevNormMag == 0.0f;
			}

			// Analog stick was just moved to center this frame.
			inline constexpr bool MovedToCenter() const
			{
				return normMag == 0.0f && prevNormMag != 0.0f;
			} 

			// Normalized magnitude of the analog stick's displacement: [0.0, 1.0].
			float normMag;
			// Previous normalized magnitude recorded the last frame.
			float prevNormMag;
			// Seconds since the previous set of data was updated.
			float secsSincePrevStateSet;
			// Angular speed of the analog stick (radians per second).
			float stickAngularSpeed;
			// Linear speed of the analog stick (normalized magnitude change per second).
			float stickLinearSpeed;
			// X component of the analog stick's displacement.
			float xComp;
			// Y component of the analog stick's displacement.
			float yComp;
			// Maximum pre-normalized full displacement from center.
			SHORT maxMag;
			// Time point at which the previous set of data was recorded.
			SteadyClock::time_point setPrevStateTP;
		};

		struct InputState
		{
			InputState() :
				isPressed(false), 
				justPressed(false), 
				justReleased(false), 
				heldTimeSecs(0.0f), 
				pressedMag(0.0f), 
				consecPresses(0)
			{ }

			// Is this input pressed?
			bool isPressed;
			bool justPressed;
			bool justReleased;
			// Time this input has been held for.
			float heldTimeSecs;
			// Magnitude of the input's button press 
			// ([0.0, 1.0] for triggers, 0.0 or 1.0 for buttons)
			float pressedMag;
			// Number of times this input has been pressed consecutively.
			uint8_t consecPresses;
		};

		ControllerDataHolder();
		~ControllerDataHolder() = default;
		ControllerDataHolder& operator=(const ControllerDataHolder& _com) = delete;
		ControllerDataHolder& operator=(ControllerDataHolder&& _com) = delete;

		// Get the number of times the input corresponding to the given input action 
		// was pressed/moved and centered consecutively
		// on the given controller.
		inline constexpr uint8_t ConsecTaps
		(
			const int32_t& a_controllerID, const InputAction& a_index
		) const
		{
			return inputStatesList[a_controllerID][!a_index].consecPresses;
		}

		// Get cached analog stick state data for the given controller's LS/RS.
		inline const AnalogStickState& GetAnalogStickState
		(
			const int32_t& a_controllerID, const bool& a_isLS
		) const
		{
			return (a_isLS) ? lsStatesList[a_controllerID] : rsStatesList[a_controllerID];
		}

		// Returns the input action mask for the given controller.
		// Each bit represents a button/analog stick 
		// that is either pressed/moved (1) or released/centered (0).
		inline constexpr uint32_t GetInputMask(const int32_t& a_controllerID) const
		{
			return inputMasksList[a_controllerID];
		}

		// Get input state data for the given controller 
		// and input action index (must NOT be a player action).
		inline const InputState& GetInputState
		(
			const int32_t& a_controllerID, const InputAction& a_index
		) const
		{
			return inputStatesList[a_controllerID][!a_index];
		}

		// Returns the number of seconds that the input has been held/moved 
		// for on the given controller.
		inline constexpr float HeldSecs
		(
			const int32_t& a_controllerID, const InputAction& a_index
		) const
		{
			return inputStatesList[a_controllerID][!a_index].heldTimeSecs;
		}

		// Returns true if the input is pressed/moved on the given controller.
		inline constexpr bool IsPressed
		(
			const int32_t& a_controllerID, const InputAction& a_index
		) const
		{
			return inputStatesList[a_controllerID][!a_index].isPressed;
		}

		// Returns true if the input is not pressed/centered on the given controller.
		inline constexpr bool IsReleased
		(
			const int32_t& a_controllerID, const InputAction& a_index
		) const
		{
			return !inputStatesList[a_controllerID][!a_index].isPressed;
		}

		// Gets input index corresponding to the given keycode.
		// Returns invalid index if keycode is invalid or not a controller mask/dxsc.
		inline constexpr InputAction MappedKeyCodeToInputIndex(const uint32_t& a_keyCode) const
		{
			if (a_keyCode != 0xFF)
			{
				if (GAMEMASK_TO_DXSC.contains(a_keyCode))
				{
					return 
					(
						static_cast<InputAction>
						(
							GAMEMASK_TO_DXSC.at(a_keyCode) - FIRST_CTRLR_DXSC
						)
					);
				}
				else if (a_keyCode >= FIRST_CTRLR_DXSC && a_keyCode <= LAST_CTRLR_DXSC)
				{
					return static_cast<InputAction>(a_keyCode - FIRST_CTRLR_DXSC);
				}
			}

			return InputAction::kNone;
		}

		// Returns 1 or 0 if the input action corresponds to a button/analog stick 
		// and is pressed/moved or released/centered.
		// Returns a number between 1 (fully pressed) and 0 (not pressed) 
		// if the input action corresponds to a trigger.
		inline constexpr float PressedMag
		(
			const int32_t& a_controllerID, const InputAction& a_index
		) const
		{
			return inputStatesList[a_controllerID][!a_index].pressedMag;
		}

		// Sets up controller data for connected controllers and returns a list of their IDs.
		std::vector<uint32_t> SetupConnectedCoopControllers();
		
		// Update analog stick state data for the given controller's LS/RS.
		void UpdateAnalogStickState
		(
			const int32_t& a_controllerID, 
			const int32_t& a_playerID, 
			const bool& a_isLS, 
			const bool& a_isControllingMenus
		);
		
		// Update input (buttons and analog sticks) data for the given controller ID 
		// and player ID (used for player-specific deadzone settings).
		void UpdateInputStatesAndMask(const int32_t& a_controllerID, const int32_t& a_playerID);
		
		// Update each player's controller data when a co-op session is active 
		// and each active controller's data when there is no co-op session.
		void UpdatePlayerControllerStates();

		//
		// Members
		//

		// Various maps between button code conventions.

		const std::unordered_map<std::uint32_t, std::uint16_t> DXSC_TO_XIMASK =
		{
			{ DXSC_DPAD_UP, XINPUT_GAMEPAD_DPAD_UP },
			{ DXSC_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN },
			{ DXSC_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT },
			{ DXSC_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT },
			{ DXSC_START, XINPUT_GAMEPAD_START },
			{ DXSC_BACK, XINPUT_GAMEPAD_BACK },
			{ DXSC_LEFT_THUMB, XINPUT_GAMEPAD_LEFT_THUMB },
			{ DXSC_RIGHT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB },
			{ DXSC_LEFT_SHOULDER, XINPUT_GAMEPAD_LEFT_SHOULDER },
			{ DXSC_RIGHT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER },
			{ DXSC_A, XINPUT_GAMEPAD_A },
			{ DXSC_B, XINPUT_GAMEPAD_B },
			{ DXSC_X, XINPUT_GAMEPAD_X },
			{ DXSC_Y, XINPUT_GAMEPAD_Y }
		};

		const std::unordered_map<std::uint16_t, std::uint8_t> GAMEMASK_TO_DXSC = 
		{
			{ GAMEPAD_MASK_DPAD_UP, DXSC_DPAD_UP },
			{ GAMEPAD_MASK_DPAD_DOWN, DXSC_DPAD_DOWN },
			{ GAMEPAD_MASK_DPAD_LEFT, DXSC_DPAD_LEFT },
			{ GAMEPAD_MASK_DPAD_RIGHT, DXSC_DPAD_RIGHT },
			{ GAMEPAD_MASK_START, DXSC_START },
			{ GAMEPAD_MASK_BACK, DXSC_BACK },
			{ GAMEPAD_MASK_LEFT_THUMB, DXSC_LEFT_THUMB },
			{ GAMEPAD_MASK_RIGHT_THUMB, DXSC_RIGHT_THUMB },
			{ GAMEPAD_MASK_LEFT_SHOULDER, DXSC_LEFT_SHOULDER },
			{ GAMEPAD_MASK_RIGHT_SHOULDER, DXSC_RIGHT_SHOULDER },
			{ GAMEPAD_MASK_A, DXSC_A },
			{ GAMEPAD_MASK_B, DXSC_B },
			{ GAMEPAD_MASK_X, DXSC_X },
			{ GAMEPAD_MASK_Y, DXSC_Y },
			{ GAMEPAD_MASK_LT, DXSC_LT },
			{ GAMEPAD_MASK_RT, DXSC_RT }
		};

		const std::unordered_map<std::uint8_t, InputAction> GAMEMASK_TO_INPUT_ACTION =
		{
			{ GAMEPAD_MASK_DPAD_UP, InputAction::kDPadU },
			{ GAMEPAD_MASK_DPAD_DOWN, InputAction::kDPadD },
			{ GAMEPAD_MASK_DPAD_LEFT, InputAction::kDPadL },
			{ GAMEPAD_MASK_DPAD_RIGHT, InputAction::kDPadR },
			{ GAMEPAD_MASK_START, InputAction::kStart },
			{ GAMEPAD_MASK_BACK, InputAction::kBack },
			{ GAMEPAD_MASK_LEFT_THUMB, InputAction::kLThumb },
			{ GAMEPAD_MASK_RIGHT_THUMB, InputAction::kRThumb },
			{ GAMEPAD_MASK_LEFT_SHOULDER, InputAction::kLShoulder },
			{ GAMEPAD_MASK_RIGHT_SHOULDER, InputAction::kRShoulder },
			{ GAMEPAD_MASK_A, InputAction::kA },
			{ GAMEPAD_MASK_B, InputAction::kB },
			{ GAMEPAD_MASK_X, InputAction::kX },
			{ GAMEPAD_MASK_Y, InputAction::kY },
			{ GAMEPAD_MASK_LT, InputAction::kLT },
			{ GAMEPAD_MASK_RT, InputAction::kRT }
		};

		const std::unordered_map<std::uint32_t, std::uint32_t> GAMEMASK_TO_XIMASK =
		{
			{ GAMEPAD_MASK_DPAD_UP, XINPUT_GAMEPAD_DPAD_UP },
			{ GAMEPAD_MASK_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN },
			{ GAMEPAD_MASK_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT },
			{ GAMEPAD_MASK_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT },
			{ GAMEPAD_MASK_START, XINPUT_GAMEPAD_START },
			{ GAMEPAD_MASK_BACK, XINPUT_GAMEPAD_BACK },
			{ GAMEPAD_MASK_LEFT_THUMB, XINPUT_GAMEPAD_LEFT_THUMB },
			{ GAMEPAD_MASK_RIGHT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB },
			{ GAMEPAD_MASK_LEFT_SHOULDER, XINPUT_GAMEPAD_LEFT_SHOULDER },
			{ GAMEPAD_MASK_RIGHT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER },
			{ GAMEPAD_MASK_A, XINPUT_GAMEPAD_A },
			{ GAMEPAD_MASK_B, XINPUT_GAMEPAD_B },
			{ GAMEPAD_MASK_X, XINPUT_GAMEPAD_X },
			{ GAMEPAD_MASK_Y, XINPUT_GAMEPAD_Y },
			{ GAMEPAD_MASK_LT, XMASK_LT },
			{ GAMEPAD_MASK_RT, XMASK_RT },
			{ GAMEPAD_MASK_LS, XMASK_LS },
			{ GAMEPAD_MASK_RS, XMASK_RS }
		};

		const std::unordered_map<InputAction, std::uint8_t> INPUT_ACTION_TO_GAMEMASK =
		{
			{ InputAction::kDPadU, GAMEPAD_MASK_DPAD_UP },
			{ InputAction::kDPadD, GAMEPAD_MASK_DPAD_DOWN },
			{ InputAction::kDPadL, GAMEPAD_MASK_DPAD_LEFT },
			{ InputAction::kDPadR, GAMEPAD_MASK_DPAD_RIGHT },
			{ InputAction::kStart, GAMEPAD_MASK_START },
			{ InputAction::kBack, GAMEPAD_MASK_BACK },
			{ InputAction::kLThumb, GAMEPAD_MASK_LEFT_THUMB },
			{ InputAction::kRThumb, GAMEPAD_MASK_RIGHT_THUMB },
			{ InputAction::kLShoulder, GAMEPAD_MASK_LEFT_SHOULDER },
			{ InputAction::kRShoulder, GAMEPAD_MASK_RIGHT_SHOULDER },
			{ InputAction::kA, GAMEPAD_MASK_A },
			{ InputAction::kB, GAMEPAD_MASK_B },
			{ InputAction::kX, GAMEPAD_MASK_X },
			{ InputAction::kY, GAMEPAD_MASK_Y },
			{ InputAction::kLT, GAMEPAD_MASK_LT },
			{ InputAction::kRT, GAMEPAD_MASK_RT }
		};

		const std::unordered_map<std::uint32_t, std::uint32_t> XIMASK_TO_DXSC = 
		{
			{ XINPUT_GAMEPAD_DPAD_UP, DXSC_DPAD_UP },
			{ XINPUT_GAMEPAD_DPAD_DOWN, DXSC_DPAD_DOWN },
			{ XINPUT_GAMEPAD_DPAD_LEFT, DXSC_DPAD_LEFT },
			{ XINPUT_GAMEPAD_DPAD_RIGHT, DXSC_DPAD_RIGHT },
			{ XINPUT_GAMEPAD_START, DXSC_START },
			{ XINPUT_GAMEPAD_BACK, DXSC_BACK },
			{ XINPUT_GAMEPAD_LEFT_THUMB, DXSC_LEFT_THUMB },
			{ XINPUT_GAMEPAD_RIGHT_THUMB, DXSC_RIGHT_THUMB },
			{ XINPUT_GAMEPAD_LEFT_SHOULDER, DXSC_LEFT_SHOULDER },
			{ XINPUT_GAMEPAD_RIGHT_SHOULDER, DXSC_RIGHT_SHOULDER },
			{ XINPUT_GAMEPAD_A, DXSC_A },
			{ XINPUT_GAMEPAD_B, DXSC_B },
			{ XINPUT_GAMEPAD_X, DXSC_X },
			{ XINPUT_GAMEPAD_Y, DXSC_Y },
			{ XMASK_LT, DXSC_LT },
			{ XMASK_RT, DXSC_RT }

		};

		const std::unordered_map<std::uint32_t, std::uint32_t> XIMASK_TO_GAMEMASK = 
		{
			{ XINPUT_GAMEPAD_DPAD_UP, GAMEPAD_MASK_DPAD_UP },
			{ XINPUT_GAMEPAD_DPAD_DOWN, GAMEPAD_MASK_DPAD_DOWN },
			{ XINPUT_GAMEPAD_DPAD_LEFT, GAMEPAD_MASK_DPAD_LEFT },
			{ XINPUT_GAMEPAD_DPAD_RIGHT, GAMEPAD_MASK_DPAD_RIGHT },
			{ XINPUT_GAMEPAD_START, GAMEPAD_MASK_START },
			{ XINPUT_GAMEPAD_BACK, GAMEPAD_MASK_BACK },
			{ XINPUT_GAMEPAD_LEFT_THUMB, GAMEPAD_MASK_LEFT_THUMB },
			{ XINPUT_GAMEPAD_RIGHT_THUMB, GAMEPAD_MASK_RIGHT_THUMB },
			{ XINPUT_GAMEPAD_LEFT_SHOULDER, GAMEPAD_MASK_LEFT_SHOULDER },
			{ XINPUT_GAMEPAD_RIGHT_SHOULDER, GAMEPAD_MASK_RIGHT_SHOULDER },

			{ XINPUT_GAMEPAD_A, GAMEPAD_MASK_A },
			{ XINPUT_GAMEPAD_B, GAMEPAD_MASK_B },
			{ XINPUT_GAMEPAD_X, GAMEPAD_MASK_X },
			{ XINPUT_GAMEPAD_Y, GAMEPAD_MASK_Y },

			{ XMASK_LT, GAMEPAD_MASK_LT },
			{ XMASK_RT, GAMEPAD_MASK_RT },
			{ XMASK_LS, GAMEPAD_MASK_LS },
			{ XMASK_RS, GAMEPAD_MASK_RS }
		};

		// Analog stick states for the left and right sticks.
		std::array<AnalogStickState, Settings::fMaxNumControllers> lsStatesList;
		std::array<AnalogStickState, Settings::fMaxNumControllers> rsStatesList;
		// Input masks for each player.
		std::array<std::uint32_t, Settings::fMaxNumControllers> inputMasksList;
		// Time points indicating when each button was last pressed 
		// or analog stick moved for each player.
		std::array<std::vector<SteadyClock::time_point>, Settings::fMaxNumControllers> 
		firstPressTPsList;
		// Input (button/analog stick) states for each player.
		std::array<std::vector<InputState>, Settings::fMaxNumControllers> inputStatesList;
		// Time points indicating when each button was last released 
		// or analog stick centered for each player.
		std::array<std::vector<SteadyClock::time_point>, Settings::fMaxNumControllers> 
		lastReleaseTPsList;
	};
};

	
