#pragma once

namespace Dialogue
{
	// Hooks menu input; call at plugin load.
	void Install();

	// Listens for menus opening and closing; call once the UI exists (kDataLoaded).
	void Register();

	// Puts back anything still hidden and forgets the conversation (load or new game).
	void Reset();
}
