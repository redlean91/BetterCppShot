#include "Application.h"
#include "..\Utils.h"

Application& Application::get() {
    static Application instance;
    return instance;
}

Application::Application() {
    m_saveDirectory = CppShot::getRegistry("Path", "C:\\test\\");
}

std::string Application::getSaveDirectory() const {
    return m_saveDirectory;
}