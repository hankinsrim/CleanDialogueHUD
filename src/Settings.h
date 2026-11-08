#pragma once

// Values from Data\SKSE\Plugins\CleanDialogueHUD.ini.
struct Settings
{
	// [General]
	bool enableLogging{ false };

	// [Cursor]
	bool         hideCursor{ true };
	std::int32_t moveThreshold{ 2 };  // raw mouse units in one event that count as a real move
	bool         showCursorOnClick{ true };
	bool         blockHiddenHover{ true };

	// [Compass]
	bool hideCompass{ true };
	bool showCompassWithCursor{ false };

	[[nodiscard]] static Settings& Get() noexcept
	{
		static Settings singleton;
		return singleton;
	}

	// Read before SKSE::Init, which creates the log file.
	[[nodiscard]] static bool ReadEnableLogging();

	void Load();

	[[nodiscard]] bool HidesAnything() const noexcept { return hideCursor || hideCompass; }
};
