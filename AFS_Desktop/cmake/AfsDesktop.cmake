# Source list of the desktop application and afs_desktop_configure_target(),
# which attaches include paths and libraries. The GUI tests compile the same
# sources with _GUITEST (App.cpp then leaves main() to Google Test), so the
# list is kept here rather than in the CMakeLists.txt of the application.
include_guard(DIRECTORY)

get_filename_component(AFS_DESKTOP_DIR ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)

set(AFS_DESKTOP_SOURCES
    App.cpp
    AutoSave.cpp
    BackgroundService.cpp
    Canvas.cpp
    ChatDialog.cpp
    DevicesDialog.cpp
    FirewallHint.cpp
    PairDeviceDialog.cpp
    PhoneDirDialog.cpp
    PhoneTemplatesDialog.cpp
    QrCode.cpp
    HelpBook.cpp
    GridCtrl.cpp
    GridWindow.cpp
    ListCtrl.cpp
    ListWindow.cpp
    LoggingDialog.cpp
    MainWindow.cpp
    Menu.cpp
    ProgressDialog.cpp
    PreviewDialog.cpp
    SettingsDialog.cpp
    SourceCanvas.cpp
    SplitterWindow.cpp
    SshAccountDialog.cpp
    SshAccountSelectDialog.cpp
    SshAccountsDialog.cpp
    SshDirDialog.cpp
    SyncInterface.cpp
    SyncItemDialog.cpp
    TargetCanvas.cpp
    TaskBarIcon.cpp
    Toolbar.cpp
    Tree.cpp
    TreeCtrl.cpp
    TreeItemData.cpp
)
list(TRANSFORM AFS_DESKTOP_SOURCES PREPEND ${AFS_DESKTOP_DIR}/src/)
list(APPEND AFS_DESKTOP_SOURCES ${AFS_DESKTOP_DIR}/../third-party/qrcodegen/qrcodegen.cpp)  # QrCode.cpp draws the pairing code with it (MIT, see doc/THIRD-PARTY-NOTICES.md)

if(APPLE)
    list(APPEND AFS_DESKTOP_SOURCES ${AFS_DESKTOP_DIR}/src/macutil.mm)
    set_source_files_properties(${AFS_DESKTOP_DIR}/src/macutil.mm PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

function(afs_desktop_configure_target target)
    target_include_directories(${target} PRIVATE ${AFS_DESKTOP_DIR}/include ${AFS_DESKTOP_DIR}/../third-party)
    target_link_libraries(${target} PRIVATE afs_core)
    if(APPLE)
        target_link_libraries(${target} PRIVATE "-framework Cocoa")
    endif()
endfunction()
