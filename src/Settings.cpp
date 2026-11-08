#include "Settings.h"

namespace
{
	constexpr auto INI_PATH           = "Data/SKSE/Plugins/CleanDialogueHUD.ini";
	constexpr long MAX_MOVE_THRESHOLD = 1000;

	struct BoolOption
	{
		const char* section;
		const char* key;
		bool Settings::* member;
	};

	constexpr auto BOOL_OPTIONS = std::to_array<BoolOption>({
		{ "Cursor", "HideCursor", &Settings::hideCursor },
		{ "Cursor", "ShowCursorOnClick", &Settings::showCursorOnClick },
		{ "Cursor", "BlockHiddenHover", &Settings::blockHiddenHover },
		{ "Compass", "HideCompass", &Settings::hideCompass },
		{ "Compass", "ShowCompassWithCursor", &Settings::showCompassWithCursor },
	});

	bool EqualsNoCase(std::string_view a, std::string_view b)
	{
		return std::ranges::equal(a, b, [](char x, char y) { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
	}

	bool IsKnownKey(std::string_view a_section, std::string_view a_key)
	{
		if (EqualsNoCase(a_section, "General"))
		{
			return EqualsNoCase(a_key, "EnableLogging");
		}
		if (EqualsNoCase(a_section, "Cursor") && EqualsNoCase(a_key, "MoveThreshold"))
		{
			return true;
		}
		return std::ranges::any_of(BOOL_OPTIONS, [&](const BoolOption& a_opt) { return EqualsNoCase(a_section, a_opt.section) && EqualsNoCase(a_key, a_opt.key); });
	}
}

bool Settings::ReadEnableLogging()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	// A missing INI is logged so the warning about it can be read.
	return ini.LoadFile(INI_PATH) < 0 || ini.GetBoolValue("General", "EnableLogging", false);
}

void Settings::Load()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	if (const auto rc = ini.LoadFile(INI_PATH); rc < 0)
	{
		logger::warn("Could not read {} (error {}); using defaults.", INI_PATH, static_cast<int>(rc));
		return;
	}

	enableLogging = ini.GetBoolValue("General", "EnableLogging", enableLogging);
	logger::info("  [General] EnableLogging = {}", enableLogging);

	for (const auto& opt : BOOL_OPTIONS)
	{
		this->*opt.member = ini.GetBoolValue(opt.section, opt.key, this->*opt.member);
		logger::info("  [{}] {} = {}", opt.section, opt.key, this->*opt.member);
	}

	const auto threshold = ini.GetLongValue("Cursor", "MoveThreshold", moveThreshold);
	if (threshold < 0)
	{
		logger::warn("  [Cursor] MoveThreshold = {} is negative - using 0.", threshold);
	}
	moveThreshold = static_cast<std::int32_t>(std::clamp(threshold, 0L, MAX_MOVE_THRESHOLD));
	logger::info("  [Cursor] MoveThreshold = {}", moveThreshold);

	// Typos in option names would otherwise be silently ignored.
	CSimpleIniA::TNamesDepend sections;
	ini.GetAllSections(sections);
	for (const auto& section : sections)
	{
		CSimpleIniA::TNamesDepend keys;
		ini.GetAllKeys(section.pItem, keys);
		for (const auto& key : keys)
		{
			if (!IsKnownKey(section.pItem, key.pItem))
			{
				logger::warn("  Unknown option [{}] {} in INI - ignored (check spelling).", section.pItem, key.pItem);
			}
		}
	}
}
