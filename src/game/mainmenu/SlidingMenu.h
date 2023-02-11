#pragma once

#include <vector>
#include <string>
#include <map>
#include <functional>

#include "MenuObject.h"

#include "ui/slidingmenu/MenuBlock.h"

using namespace std;

namespace game {
namespace mainmenu {

CLASS( SlidingMenu, MenuObject )
	
	typedef function<void()> choice_handler_t;
	typedef vector<pair<string, choice_handler_t>> choice_handlers_t;
	
	SlidingMenu( MainMenu *mainmenu, const string& title, const choice_handlers_t& choices );
	
	void Show();
	void Hide();
	void OnItemClick( const string& choice );
	const string GetChoice() const;
	void SetChoice( const string& choice );
	
	void Close();
	bool MaybeClose();

private:
	const choice_handlers_t m_choices = {};
	string m_choice = "";

	MenuBlock* m_menu_block = nullptr;
	
};
	
}
}

