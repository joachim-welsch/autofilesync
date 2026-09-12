#pragma once

#include <gtest/gtest.h>
#include <wx/frame.h>
#include <string>
#include <vector>

class ListCtrl;

namespace guitest {

struct ApplicationState {  // snapshot of everything a GUI test is required to leave untouched
    std::vector<std::string> syncItemNames;
    std::vector<std::string> sshAccountNames;
    std::string configFile;
    std::vector<std::string> sourceEntries;
    std::vector<std::string> targetEntries;
};

class GuiTest: public ::testing::Test {  // [GuiTest.md#GuiTest]
protected:
    void SetUp() override;
    void TearDown() override;
    wxFrame& mainWindow() const;
    ListCtrl* syncItemList() const;
    bool listShows(const std::wstring& syncItemName) const;
private:
    ApplicationState before_;
};

}
