#include "Dialogue.h"
#include "Settings.h"

namespace Dialogue
{
	namespace
	{
		constexpr std::size_t PROCESS_EVENT_SLOT = 1;   // BSTEventSink::ProcessEvent
		constexpr std::size_t MAX_SWALLOWED      = 32;  // per input batch; more than that pass through

		// The game sets the compass clip's _visible every frame, so it is hidden through _alpha instead.
		constexpr auto COMPASS_PATH = "_root.HUDMovieBaseInstance.CompassShoutMeterHolder.Compass";

		// Far outside the screen, so no dialogue line is under the pointer while the cursor is hidden.
		constexpr float PARKED_POINTER = -10000.0F;

		struct State
		{
			bool                         inDialogue{ false };
			bool                         coveredByMenu{ false };     // a menu that uses the cursor opened over this conversation
			bool                         wantCursorHidden{ false };  // this conversation should hide the cursor until it is shown again
			bool                         cursorHidden{ false };      // the plugin has hidden the current cursor movie
			std::optional<std::uint32_t> swallowedButton;            // mouse button whose press showed the cursor, swallowed until released
		};

		std::mutex stateLock;
		State      state;

		std::optional<double> savedCompassAlpha;  // only used in UI tasks

		RE::GPtr<RE::GFxMovieView> GetMovie(std::string_view a_menuName)
		{
			auto* const ui   = RE::UI::GetSingleton();
			const auto  menu = ui ? ui->GetMenu(a_menuName) : RE::GPtr<RE::IMenu>{};
			return menu ? menu->uiMovie : RE::GPtr<RE::GFxMovieView>{};
		}

		bool GetCompass(RE::GFxValue& a_compass)
		{
			const auto hud = GetMovie(RE::HUDMenu::MENU_NAME);
			return hud && hud->GetVariable(&a_compass, COMPASS_PATH) && a_compass.IsDisplayObject();
		}

		void SetAlpha(RE::GFxValue& a_clip, double a_alpha)
		{
			RE::GFxValue::DisplayInfo info;
			info.SetAlpha(a_alpha);
			a_clip.SetDisplayInfo(info);
		}

		void HideCompass()
		{
			if (savedCompassAlpha)
			{
				return;
			}
			RE::GFxValue compass;
			if (!GetCompass(compass))
			{
				logger::warn("Compass clip {} not found - compass left alone.", COMPASS_PATH);
				return;
			}
			RE::GFxValue::DisplayInfo info;
			compass.GetDisplayInfo(&info);
			if (info.GetAlpha() <= 0.0)
			{
				logger::info("Compass is already transparent - left alone.");
				return;
			}
			savedCompassAlpha = info.GetAlpha();
			SetAlpha(compass, 0.0);
			logger::info("Compass hidden (alpha {} -> 0).", *savedCompassAlpha);
		}

		void RestoreCompass()
		{
			if (!savedCompassAlpha)
			{
				return;
			}
			const double alpha = *std::exchange(savedCompassAlpha, std::nullopt);
			RE::GFxValue compass;
			if (!GetCompass(compass))
			{
				return;
			}
			RE::GFxValue::DisplayInfo info;
			compass.GetDisplayInfo(&info);
			if (info.GetAlpha() != 0.0)
			{
				logger::info("Compass alpha was changed to {} by something else - left alone.", info.GetAlpha());
				return;
			}
			SetAlpha(compass, alpha);
			logger::info("Compass shown (alpha 0 -> {}).", alpha);
		}

		void ApplyCursorHide()
		{
			{
				const std::scoped_lock guard(stateLock);
				if (!state.wantCursorHidden || state.cursorHidden)
				{
					return;
				}
			}
			const auto cursor = GetMovie(RE::CursorMenu::MENU_NAME);
			if (!cursor)
			{
				return;  // tried again when the Cursor Menu opens
			}
			if (!cursor->GetVisible())
			{
				logger::info("Cursor is already hidden by the game - left alone.");
				const std::scoped_lock guard(stateLock);
				state.wantCursorHidden = false;
				return;
			}
			cursor->SetVisible(false);
			{
				const std::scoped_lock guard(stateLock);
				state.cursorHidden = true;
			}
			if (Settings::Get().blockHiddenHover)
			{
				if (const auto dialogue = GetMovie(RE::DialogueMenu::MENU_NAME))
				{
					dialogue->NotifyMouseState(PARKED_POINTER, PARKED_POINTER, 0);
				}
			}
			logger::info("Cursor hidden.");
		}

		void ShowCursor(std::string_view a_reason, bool a_byMouse)
		{
			bool wasHidden = false;
			{
				const std::scoped_lock guard(stateLock);
				wasHidden              = std::exchange(state.cursorHidden, false);
				state.wantCursorHidden = false;
			}
			if (wasHidden)
			{
				logger::info("Cursor shown ({}).", a_reason);
				SKSE::GetTaskInterface()->AddUITask([] {
					const auto cursor = GetMovie(RE::CursorMenu::MENU_NAME);
					if (!cursor)
					{
						return;
					}
					if (cursor->GetVisible())
					{
						logger::info("Cursor movie was already visible again.");
						return;
					}
					cursor->SetVisible(true);
				});
			}
			if (wasHidden && a_byMouse && Settings::Get().showCompassWithCursor)
			{
				SKSE::GetTaskInterface()->AddUITask(RestoreCompass);
			}
		}

		bool IsWheel(std::uint32_t a_idCode)
		{
			return a_idCode == RE::BSWin32MouseDevice::Key::kWheelUp || a_idCode == RE::BSWin32MouseDevice::Key::kWheelDown;
		}

		bool ShouldSwallow(RE::InputEvent& a_event)
		{
			if (a_event.GetDevice() != RE::INPUT_DEVICE::kMouse)
			{
				return false;
			}
			const auto&      settings = Settings::Get();
			std::unique_lock guard(stateLock);

			if (auto* const button = a_event.AsButtonEvent())
			{
				const auto id = button->GetIDCode();
				if (state.swallowedButton == id)
				{
					if (!button->IsPressed())
					{
						state.swallowedButton.reset();
					}
					return true;
				}
				if (!state.cursorHidden || !settings.showCursorOnClick || !button->IsDown())
				{
					return false;
				}
				// The wheel sends no release, so only real buttons are tracked.
				if (!IsWheel(id))
				{
					state.swallowedButton = id;
				}
				guard.unlock();
				ShowCursor(IsWheel(id) ? "mouse wheel"sv : "mouse click"sv, true);
				return true;
			}

			if (auto* const move = a_event.AsMouseMoveEvent())
			{
				if (!state.cursorHidden)
				{
					return false;
				}
				const auto distance = std::max(std::abs(move->mouseInputX), std::abs(move->mouseInputY));
				if (distance >= std::max(settings.moveThreshold, 1))
				{
					guard.unlock();
					ShowCursor("mouse moved"sv, true);
					return false;
				}
				return settings.blockHiddenHover;
			}
			return false;
		}

		struct InputHook
		{
			using Link = std::pair<RE::InputEvent*, RE::InputEvent*>;  // previous event, unlinked event

			// Swallowed events are unlinked for the menus only and linked back before later sinks see the chain.
			static RE::BSEventNotifyControl Thunk(RE::MenuControls* a_this, RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_source)
			{
				if (!a_event || !*a_event)
				{
					return func(a_this, a_event, a_source);
				}

				RE::InputEvent*                 head = *a_event;
				RE::InputEvent*                 prev = nullptr;
				std::array<Link, MAX_SWALLOWED> unlinked{};
				std::size_t                     count = 0;
				for (auto* event = *a_event; event; event = event->next)
				{
					if (count < unlinked.size() && ShouldSwallow(*event))
					{
						(prev ? prev->next : head) = event->next;
						unlinked.at(count++)       = { prev, event };
					}
					else
					{
						prev = event;
					}
				}
				if (count == 0)
				{
					return func(a_this, a_event, a_source);
				}

				const auto result = head ? func(a_this, &head, a_source) : RE::BSEventNotifyControl::kContinue;
				for (auto i = count; i-- > 0;)
				{
					if (const auto& [linkPrev, linkEvent] = unlinked.at(i); linkPrev)
					{
						linkPrev->next = linkEvent;
					}
				}
				return result;
			}

			static inline REL::Relocation<decltype(Thunk)> func;
		};

		bool NeedsCursor(const RE::IMenu& a_menu)
		{
			return a_menu.UsesCursor() || a_menu.UpdateUsesCursor();
		}

		void HideCursorForConversation()
		{
			if (!Settings::Get().hideCursor)
			{
				return;
			}
			auto* const devices = RE::BSInputDeviceManager::GetSingleton();
			if (devices && devices->IsGamepadEnabled())
			{
				logger::info("Gamepad is enabled - cursor left to the game.");
				return;
			}
			{
				const std::scoped_lock guard(stateLock);
				state.wantCursorHidden = true;
			}
			SKSE::GetTaskInterface()->AddUITask(ApplyCursorHide);
		}

		// Runs as a UI task, after the closed menu has left the menu stack.
		void ReturnToConversation()
		{
			auto* const ui = RE::UI::GetSingleton();
			if (!ui)
			{
				return;
			}
			const auto dialogue = ui->GetMenu(RE::DialogueMenu::MENU_NAME);
			if (!dialogue)
			{
				return;
			}
			for (const auto& menu : ui->menuStack)
			{
				if (menu && menu != dialogue && NeedsCursor(*menu))
				{
					return;  // another menu that uses the cursor is still open
				}
			}
			{
				const std::scoped_lock guard(stateLock);
				if (!state.inDialogue || !std::exchange(state.coveredByMenu, false))
				{
					return;
				}
			}
			logger::info("Back in the conversation.");
			HideCursorForConversation();
		}

		void OnDialogueOpen()
		{
			logger::info("Dialogue Menu opened.");
			{
				const std::scoped_lock guard(stateLock);
				state.inDialogue    = true;
				state.coveredByMenu = false;
			}
			HideCursorForConversation();
			if (Settings::Get().hideCompass)
			{
				SKSE::GetTaskInterface()->AddUITask(HideCompass);
			}
		}

		void OnDialogueClose()
		{
			logger::info("Dialogue Menu closed.");
			{
				const std::scoped_lock guard(stateLock);
				state.inDialogue    = false;
				state.coveredByMenu = false;
			}
			ShowCursor("Dialogue Menu closed"sv, false);
			SKSE::GetTaskInterface()->AddUITask(RestoreCompass);
		}

		void OnOtherMenuOpen(std::string_view a_menuName)
		{
			if (a_menuName == RE::CursorMenu::MENU_NAME)
			{
				{
					const std::scoped_lock guard(stateLock);
					if (!state.wantCursorHidden)
					{
						return;
					}
					// A newly opened Cursor Menu has a fresh, visible movie.
					state.cursorHidden = false;
				}
				SKSE::GetTaskInterface()->AddUITask(ApplyCursorHide);
				return;
			}

			auto* const ui   = RE::UI::GetSingleton();
			const auto  menu = ui ? ui->GetMenu(a_menuName) : RE::GPtr<RE::IMenu>{};
			if (!menu || !NeedsCursor(*menu))
			{
				return;
			}
			{
				const std::scoped_lock guard(stateLock);
				if (!state.inDialogue)
				{
					return;
				}
				state.coveredByMenu = true;
			}
			ShowCursor(a_menuName, false);
		}

		void OnOtherMenuClose(std::string_view a_menuName)
		{
			if (a_menuName == RE::CursorMenu::MENU_NAME)
			{
				return;
			}
			{
				const std::scoped_lock guard(stateLock);
				if (!state.inDialogue || !state.coveredByMenu)
				{
					return;
				}
			}
			SKSE::GetTaskInterface()->AddUITask(ReturnToConversation);
		}

		class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			static MenuSink* GetSingleton()
			{
				static MenuSink singleton;
				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* /*a_source*/) override
			{
				if (!a_event)
				{
					return RE::BSEventNotifyControl::kContinue;
				}
				const std::string_view name{ a_event->menuName };
				if (name == RE::DialogueMenu::MENU_NAME)
				{
					a_event->opening ? OnDialogueOpen() : OnDialogueClose();
				}
				else if (a_event->opening)
				{
					OnOtherMenuOpen(name);
				}
				else
				{
					OnOtherMenuClose(name);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		if (!Settings::Get().hideCursor)
		{
			return;
		}
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_MenuControls[0] };  // BSTEventSink<InputEvent*>
		InputHook::func = vtbl.write_vfunc(PROCESS_EVENT_SLOT, InputHook::Thunk);
		logger::info("Hooked MenuControls input.");
	}

	void Register()
	{
		auto* const ui = RE::UI::GetSingleton();
		if (!ui)
		{
			logger::error("UI not available - nothing will be hidden.");
			return;
		}
		ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSink::GetSingleton());
		logger::info("Listening for the Dialogue Menu.");
	}

	void Reset()
	{
		{
			const std::scoped_lock guard(stateLock);
			state.swallowedButton.reset();
			state.inDialogue    = false;
			state.coveredByMenu = false;
		}
		ShowCursor("game loaded"sv, false);
		SKSE::GetTaskInterface()->AddUITask(RestoreCompass);
	}
}
