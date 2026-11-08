#include "Dialogue.h"
#include "Settings.h"

namespace
{
	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		switch (a_message->type)
		{
		case SKSE::MessagingInterface::kDataLoaded:
			Dialogue::Register();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Dialogue::Reset();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::InitInfo info;
	info.log = Settings::ReadEnableLogging();
	SKSE::Init(a_skse, info);
	if (!info.log)
	{
		spdlog::set_level(spdlog::level::off);
	}

	const auto plugin = SKSE::GetPluginVersion();
	logger::info("{} v{}.{}.{} loading on Skyrim {}", SKSE::GetPluginName(), plugin.major(), plugin.minor(), plugin.patch(), REL::Module::get().version().string("."sv));

	logger::info("Settings:");
	auto& settings = Settings::Get();
	settings.Load();
	if (!settings.HidesAnything())
	{
		logger::info("HideCursor and HideCompass are both false - nothing to do.");
		return true;
	}

	Dialogue::Install();
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
