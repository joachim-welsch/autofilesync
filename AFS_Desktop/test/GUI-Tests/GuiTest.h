#pragma once

#include <gtest/gtest.h>
#include <wx/frame.h>
#include <string>
#include <vector>

class ListCtrl;

namespace guitest {

// Snapshot of everything a GUI test is required to leave untouched.
struct ApplicationState {
    std::vector<std::string> syncItemNames;
    std::vector<std::string> sshAccountNames;
    std::string configFile;
    std::vector<std::string> sourceEntries;
    std::vector<std::string> targetEntries;
};

// Base of every GUI test. It enforces the rule from CLAUDE.md that after a GUI
// test the configuration and the source and target file systems are in the same
// state as before: TearDown() drops what the test changed in memory and then
// compares the whole state against the snapshot taken in SetUp().
class GuiTest: public ::testing::Test {
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
