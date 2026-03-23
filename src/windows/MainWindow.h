#pragma once

#include "Window.h"

class MainWindow : public Window {
    void onOpenExplorer();
	void onOpenSettings();
	void onOpenAbout();
public:
	MainWindow();
	void onOpenExplorer_change();
	void onChangeKeybinds();
};