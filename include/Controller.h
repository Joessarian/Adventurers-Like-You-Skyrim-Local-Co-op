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

// [Controller]
#define GAME_INPUT_CODE_DPAD_UP 0x0001
#define GAME_INPUT_CODE_DPAD_DOWN 0x0002
#define GAME_INPUT_CODE_DPAD_LEFT 0x0004
#define GAME_INPUT_CODE_DPAD_RIGHT 0x0008
#define GAME_INPUT_CODE_START 0x0010
#define GAME_INPUT_CODE_BACK 0x0020
#define GAME_INPUT_CODE_LEFT_THUMB 0x0040
#define GAME_INPUT_CODE_RIGHT_THUMB 0x0080
#define GAME_INPUT_CODE_LEFT_SHOULDER 0x0100
#define GAME_INPUT_CODE_RIGHT_SHOULDER 0x0200
#define GAME_INPUT_CODE_A 0x1000
#define GAME_INPUT_CODE_B 0x2000
#define GAME_INPUT_CODE_X 0x4000
#define GAME_INPUT_CODE_Y 0x8000
#define GAME_INPUT_CODE_LT 0x0009
#define GAME_INPUT_CODE_RT 0x000A
#define GAME_INPUT_CODE_LS 0x000B
#define GAME_INPUT_CODE_RS 0x000C

// [Keyboard]
#define GAME_INPUT_ESC 0x01
#define GAME_INPUT_1 0x02  
#define GAME_INPUT_2 0x03  
#define GAME_INPUT_3 0x04  
#define GAME_INPUT_4 0x05 
#define GAME_INPUT_5 0x06
#define GAME_INPUT_6 0x07 
#define GAME_INPUT_7 0x08  
#define GAME_INPUT_8 0x09   
#define GAME_INPUT_9 0x0A  
#define GAME_INPUT_0 0x0B 
#define GAME_INPUT_HYPHEN 0x0C  
#define GAME_INPUT_EQUAL 0x0D  
#define GAME_INPUT_BACKSPACE 0x0E  
#define GAME_INPUT_TAB 0x0F  
#define GAME_INPUT_Q 0x10
#define GAME_INPUT_W 0x11
#define GAME_INPUT_E 0x12
#define GAME_INPUT_R 0x13
#define GAME_INPUT_T 0x14
#define GAME_INPUT_Y 0x15
#define GAME_INPUT_U 0x16
#define GAME_INPUT_I 0x17
#define GAME_INPUT_O 0x18
#define GAME_INPUT_P 0x19
#define GAME_INPUT_LBRACKET 0x1A  
#define GAME_INPUT_RBRACKET 0x1B  
#define GAME_INPUT_ENTER 0x1C  
#define GAME_INPUT_LCTRL 0x1D  
#define GAME_INPUT_A 0x1E
#define GAME_INPUT_S 0x1F
#define GAME_INPUT_D 0x20
#define GAME_INPUT_F 0x21
#define GAME_INPUT_G 0x22
#define GAME_INPUT_H 0x23
#define GAME_INPUT_J 0x24
#define GAME_INPUT_K 0x25
#define GAME_INPUT_L 0x26
#define GAME_INPUT_SEMICOLON 0x27
#define GAME_INPUT_APOSTROPHE 0x28
#define GAME_INPUT_TILDE 0x29
#define GAME_INPUT_LSHIFT 0x2A
#define GAME_INPUT_BCK_SLASH 0x2B
#define GAME_INPUT_Z 0x2C
#define GAME_INPUT_X 0x2D
#define GAME_INPUT_C 0x2E
#define GAME_INPUT_V 0x2F
#define GAME_INPUT_B 0x30
#define GAME_INPUT_N 0x31
#define GAME_INPUT_M 0x32
#define GAME_INPUT_COMMA 0x33  
#define GAME_INPUT_PERIOD 0x34  
#define GAME_INPUT_FWD_SLASH   0x35 
#define GAME_INPUT_RSHIFT 0x36  
#define GAME_INPUT_NUM_MULT 0x37  
#define GAME_INPUT_LALT 0x38
#define GAME_INPUT_SPACE 0x39  
#define GAME_INPUT_CAPS_LOCK 0x3A  
#define GAME_INPUT_F1 0x3B  
#define GAME_INPUT_F2 0x3C  
#define GAME_INPUT_F3 0x3D  
#define GAME_INPUT_F4 0x3E  
#define GAME_INPUT_F5 0x3F  
#define GAME_INPUT_F6 0x40  
#define GAME_INPUT_F7 0x41  
#define GAME_INPUT_F8 0x42  
#define GAME_INPUT_F9 0x43  
#define GAME_INPUT_F10 0x44
#define GAME_INPUT_NUM_LOCK 0x45  
#define GAME_INPUT_SCROLL_LOCK 0x46  
#define GAME_INPUT_NUM_7 0x47  
#define GAME_INPUT_NUM_8 0x48  
#define GAME_INPUT_NUM_9 0x49  
#define GAME_INPUT_NUM_MINUS 0x4A  
#define GAME_INPUT_NUM_4 0x4B  
#define GAME_INPUT_NUM_5 0x4C  
#define GAME_INPUT_NUM_6 0x4D  
#define GAME_INPUT_NUM_PLUS 0x4E  
#define GAME_INPUT_NUM_1 0x4F  
#define GAME_INPUT_NUM_2 0x50  
#define GAME_INPUT_NUM_3 0x51  
#define GAME_INPUT_NUM_0 0x52  
#define GAME_INPUT_NUM_DEC 0x53  
#define GAME_INPUT_F11 0x57  
#define GAME_INPUT_F12 0x58  
#define GAME_INPUT_NUM_EQUAL 0x8D,
#define GAME_INPUT_NUM_ENTER 0x9C  
#define GAME_INPUT_RCTRL 0x9D  
#define GAME_INPUT_NUM_COMMA 0xB3
#define GAME_INPUT_NUM_DIVIDE 0xB5  
#define GAME_INPUT_SYSREQ_PRTSCR 0xB7  
#define GAME_INPUT_RALT 0xB8  
#define GAME_INPUT_PAUSE 0xC5  
#define GAME_INPUT_HOME 0xC7  
#define GAME_INPUT_UP_ARROW 0xC8 
#define GAME_INPUT_PGUP 0xC9  
#define GAME_INPUT_LEFT_ARROW 0xCB  
#define GAME_INPUT_RIGHT_ARROW 0xCD   
#define GAME_INPUT_END 0xCF  
#define GAME_INPUT_DOWN_ARROW 0xD0  
#define GAME_INPUT_PGDOWN 0xD1  
#define GAME_INPUT_INSERT 0xD2  
#define GAME_INPUT_DELETE 0xD3 
#define GAME_INPUT_LWIN 0xDB
#define GAME_INPUT_RWIN 0xDC

// [Mouse]
#define GAME_INPUT_CODE_MOUSE1 0x0
#define GAME_INPUT_CODE_MOUSE2 0x1
#define GAME_INPUT_CODE_MOUSE3 0x2
#define GAME_INPUT_CODE_MOUSE4 0x3
#define GAME_INPUT_CODE_MOUSE5 0x4
#define GAME_INPUT_CODE_MOUSE6 0x5
#define GAME_INPUT_CODE_MOUSE7 0x6
#define GAME_INPUT_CODE_MOUSE8 0x7
#define GAME_INPUT_CODE_MOUSE_WHEEL_UP 0x8
#define GAME_INPUT_CODE_MOUSE_WHEEL_DOWN 0x9
#define GAME_INPUT_CODE_MOUSE_MOVE 0xA

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
				normMag(0.0f), 
				prevNormMag(0.0f),
				stickAngularSpeed(0.0f), 
				stickLinearSpeed(0.0f),
				prevXComp(0.0f),
				prevYComp(0.0f),
				xComp(0.0f), 
				yComp(0.0f),
				maxMag(0),
				packetNumber(0)
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

			// Analog stick is moving away from the center.
			inline bool MovingAwayFromCenter() const
			{
				auto deltaPos = RE::NiPoint2(xComp - prevXComp, yComp - prevYComp);
				if (deltaPos.Length() == 0.0f)
				{
					return false;
				}

				deltaPos.Unitize();
				return 
				(
					acosf(deltaPos.Dot(RE::NiPoint2(xComp, yComp))) >= 0.0f
				);	
			}

			// Analog stick is moving towards the center.
			inline bool MovingTowardsCenter() const
			{
				auto deltaPos = RE::NiPoint2(xComp - prevXComp, yComp - prevYComp);
				if (deltaPos.Length() == 0.0f)
				{
					return false;
				}

				deltaPos.Unitize();
				return 
				(
					acosf(deltaPos.Dot(RE::NiPoint2(xComp, yComp))) < 0.0f
				);	
			}

			// Normalized magnitude of the analog stick's displacement: [0.0, 1.0].
			float normMag;
			// Previous normalized magnitude recorded the last frame.
			float prevNormMag;
			// Angular speed of the analog stick (radians per second).
			float stickAngularSpeed;
			// Linear speed of the analog stick (normalized magnitude change per second).
			float stickLinearSpeed;
			// Previous X component of the analog stick's displacement.
			float prevXComp;
			// Previous Y component of the analog stick's displacement.
			float prevYComp;
			// X component of the analog stick's displacement.
			float xComp;
			// Y component of the analog stick's displacement.
			float yComp;
			// Maximum pre-normalized full displacement from center.
			SHORT maxMag;
			// Last recorded controller packet ID (snapshot of controller state).
			DWORD packetNumber;
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
			// Was the input just pressed or released?
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
				const auto iter = GAMEMASK_TO_DXSC.find(a_keyCode); 
				if (iter != GAMEMASK_TO_DXSC.end())
				{
					return 
					(
						static_cast<InputAction>
						(
							iter->second - FIRST_CTRLR_DXSC
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
		
		// Get and return a list of all co-op usable input devices' IDs.
		// NOTE:
		// P1's DID is always first.
		std::vector<uint32_t> SetupConnectedInputDevices();
		
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

		const std::unordered_map<std::uint16_t, std::uint16_t> DXSC_TO_XIMASK =
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

		const std::unordered_map<std::uint16_t, std::uint16_t> GAMEMASK_TO_DXSC = 
		{
			{ GAME_INPUT_CODE_DPAD_UP, DXSC_DPAD_UP },
			{ GAME_INPUT_CODE_DPAD_DOWN, DXSC_DPAD_DOWN },
			{ GAME_INPUT_CODE_DPAD_LEFT, DXSC_DPAD_LEFT },
			{ GAME_INPUT_CODE_DPAD_RIGHT, DXSC_DPAD_RIGHT },
			{ GAME_INPUT_CODE_START, DXSC_START },
			{ GAME_INPUT_CODE_BACK, DXSC_BACK },
			{ GAME_INPUT_CODE_LEFT_THUMB, DXSC_LEFT_THUMB },
			{ GAME_INPUT_CODE_RIGHT_THUMB, DXSC_RIGHT_THUMB },
			{ GAME_INPUT_CODE_LEFT_SHOULDER, DXSC_LEFT_SHOULDER },
			{ GAME_INPUT_CODE_RIGHT_SHOULDER, DXSC_RIGHT_SHOULDER },
			{ GAME_INPUT_CODE_A, DXSC_A },
			{ GAME_INPUT_CODE_B, DXSC_B },
			{ GAME_INPUT_CODE_X, DXSC_X },
			{ GAME_INPUT_CODE_Y, DXSC_Y },
			{ GAME_INPUT_CODE_LT, DXSC_LT },
			{ GAME_INPUT_CODE_RT, DXSC_RT }
		};

		const std::unordered_map<std::uint16_t, InputAction> GAMEMASK_TO_INPUT_ACTION =
		{
			{ GAME_INPUT_CODE_DPAD_UP, InputAction::kDPadU },
			{ GAME_INPUT_CODE_DPAD_DOWN, InputAction::kDPadD },
			{ GAME_INPUT_CODE_DPAD_LEFT, InputAction::kDPadL },
			{ GAME_INPUT_CODE_DPAD_RIGHT, InputAction::kDPadR },
			{ GAME_INPUT_CODE_START, InputAction::kStart },
			{ GAME_INPUT_CODE_BACK, InputAction::kBack },
			{ GAME_INPUT_CODE_LEFT_THUMB, InputAction::kLThumb },
			{ GAME_INPUT_CODE_RIGHT_THUMB, InputAction::kRThumb },
			{ GAME_INPUT_CODE_LEFT_SHOULDER, InputAction::kLShoulder },
			{ GAME_INPUT_CODE_RIGHT_SHOULDER, InputAction::kRShoulder },
			{ GAME_INPUT_CODE_A, InputAction::kA },
			{ GAME_INPUT_CODE_B, InputAction::kB },
			{ GAME_INPUT_CODE_X, InputAction::kX },
			{ GAME_INPUT_CODE_Y, InputAction::kY },
			{ GAME_INPUT_CODE_LT, InputAction::kLT },
			{ GAME_INPUT_CODE_RT, InputAction::kRT }
		};

		const std::unordered_map<std::uint16_t, std::uint32_t> GAMEMASK_TO_XIMASK =
		{
			{ GAME_INPUT_CODE_DPAD_UP, XINPUT_GAMEPAD_DPAD_UP },
			{ GAME_INPUT_CODE_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN },
			{ GAME_INPUT_CODE_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT },
			{ GAME_INPUT_CODE_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT },
			{ GAME_INPUT_CODE_START, XINPUT_GAMEPAD_START },
			{ GAME_INPUT_CODE_BACK, XINPUT_GAMEPAD_BACK },
			{ GAME_INPUT_CODE_LEFT_THUMB, XINPUT_GAMEPAD_LEFT_THUMB },
			{ GAME_INPUT_CODE_RIGHT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB },
			{ GAME_INPUT_CODE_LEFT_SHOULDER, XINPUT_GAMEPAD_LEFT_SHOULDER },
			{ GAME_INPUT_CODE_RIGHT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER },
			{ GAME_INPUT_CODE_A, XINPUT_GAMEPAD_A },
			{ GAME_INPUT_CODE_B, XINPUT_GAMEPAD_B },
			{ GAME_INPUT_CODE_X, XINPUT_GAMEPAD_X },
			{ GAME_INPUT_CODE_Y, XINPUT_GAMEPAD_Y },
			{ GAME_INPUT_CODE_LT, XMASK_LT },
			{ GAME_INPUT_CODE_RT, XMASK_RT },
			{ GAME_INPUT_CODE_LS, XMASK_LS },
			{ GAME_INPUT_CODE_RS, XMASK_RS }
		};

		const std::unordered_map<InputAction, std::uint16_t> INPUT_ACTION_TO_GAMEMASK =
		{
			{ InputAction::kDPadU, GAME_INPUT_CODE_DPAD_UP },
			{ InputAction::kDPadD, GAME_INPUT_CODE_DPAD_DOWN },
			{ InputAction::kDPadL, GAME_INPUT_CODE_DPAD_LEFT },
			{ InputAction::kDPadR, GAME_INPUT_CODE_DPAD_RIGHT },
			{ InputAction::kStart, GAME_INPUT_CODE_START },
			{ InputAction::kBack, GAME_INPUT_CODE_BACK },
			{ InputAction::kLThumb, GAME_INPUT_CODE_LEFT_THUMB },
			{ InputAction::kRThumb, GAME_INPUT_CODE_RIGHT_THUMB },
			{ InputAction::kLShoulder, GAME_INPUT_CODE_LEFT_SHOULDER },
			{ InputAction::kRShoulder, GAME_INPUT_CODE_RIGHT_SHOULDER },
			{ InputAction::kA, GAME_INPUT_CODE_A },
			{ InputAction::kB, GAME_INPUT_CODE_B },
			{ InputAction::kX, GAME_INPUT_CODE_X },
			{ InputAction::kY, GAME_INPUT_CODE_Y },
			{ InputAction::kLT, GAME_INPUT_CODE_LT },
			{ InputAction::kRT, GAME_INPUT_CODE_RT }
		};

		const std::unordered_map<std::uint32_t, std::uint16_t> XIMASK_TO_DXSC = 
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

		const std::unordered_map<std::uint32_t, std::uint16_t> XIMASK_TO_GAMEMASK = 
		{
			{ XINPUT_GAMEPAD_DPAD_UP, GAME_INPUT_CODE_DPAD_UP },
			{ XINPUT_GAMEPAD_DPAD_DOWN, GAME_INPUT_CODE_DPAD_DOWN },
			{ XINPUT_GAMEPAD_DPAD_LEFT, GAME_INPUT_CODE_DPAD_LEFT },
			{ XINPUT_GAMEPAD_DPAD_RIGHT, GAME_INPUT_CODE_DPAD_RIGHT },
			{ XINPUT_GAMEPAD_START, GAME_INPUT_CODE_START },
			{ XINPUT_GAMEPAD_BACK, GAME_INPUT_CODE_BACK },
			{ XINPUT_GAMEPAD_LEFT_THUMB, GAME_INPUT_CODE_LEFT_THUMB },
			{ XINPUT_GAMEPAD_RIGHT_THUMB, GAME_INPUT_CODE_RIGHT_THUMB },
			{ XINPUT_GAMEPAD_LEFT_SHOULDER, GAME_INPUT_CODE_LEFT_SHOULDER },
			{ XINPUT_GAMEPAD_RIGHT_SHOULDER, GAME_INPUT_CODE_RIGHT_SHOULDER },

			{ XINPUT_GAMEPAD_A, GAME_INPUT_CODE_A },
			{ XINPUT_GAMEPAD_B, GAME_INPUT_CODE_B },
			{ XINPUT_GAMEPAD_X, GAME_INPUT_CODE_X },
			{ XINPUT_GAMEPAD_Y, GAME_INPUT_CODE_Y },

			{ XMASK_LT, GAME_INPUT_CODE_LT },
			{ XMASK_RT, GAME_INPUT_CODE_RT },
			{ XMASK_LS, GAME_INPUT_CODE_LS },
			{ XMASK_RS, GAME_INPUT_CODE_RS }
		};

		// Number of XInput controllers plugged in.
		uint32_t activeControllerCount;
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

	
