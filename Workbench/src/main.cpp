// main.cpp
#include <wx/app.h>

#include "MainWindow.hpp"

class MycoWorkbenchApp : public wxApp {
public:
  bool OnInit() override {
    auto* window = new MainWindow();
    window->Show(true);
    return true;
  }
};

wxIMPLEMENT_APP(MycoWorkbenchApp);
