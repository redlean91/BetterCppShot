#pragma once

#include "Window.h"

class MainWindow : public Window {
    void onOpenExplorer();
	void onOpenExplorer_change();
	void onChangeKeybinds();
public:
	MainWindow();
};