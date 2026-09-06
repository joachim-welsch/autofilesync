#include "GuiTest.h"
#include "EventPump.h"
#include "UserActions.h"
#include "ChatDialog.h"
#include "id.h"
#include "utilwx.h"
#include "Logging.h"
#include "util.h"
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace AFS_Core;

using utilwx::tul;
using util::wstring_to_string;

namespace guitest {
namespace {
const wxString logQuestion = "Gibt es Warnings im Log?";

// Writes realistic entries into the sandbox's own log file, repeated until
// the file is at least as large as ChatDialog's excerpt cap
// (maxLogExcerptChars in ChatDialog.cpp). Two bugs only showed up right at
// that boundary, not with a handful of lines: an empty log short-circuits
// LlamaClient::analyzeLog() before it ever builds a prompt, and a short
// excerpt tokenizes to well under LlamaClient's n_batch limit either way -
// only a real, near-the-cap excerpt exercises the path that used to crash
// the whole process (GGML_ASSERT(n_tokens_all <= n_batch)) and then, once
// that was guarded against by raising n_batch instead of capping the prompt,
// hung for over an hour in a single oversized decode() call instead.
void writeSampleLogEntries(const std::string& path)
{
    std::ofstream out(path, std::ios::app);
    constexpr std::size_t minBytes = 4000; // comfortably above maxLogExcerptChars
    std::size_t written = 0;
    for (int i = 0; written < minBytes; ++i) {
        std::ostringstream line;
        line << "2026-08-12 16:35:" << std::setw(2) << std::setfill('0') << (i % 60)
             << ".415 Warning\tZielverzeichnis fast voll (Eintrag " << i
             << "): pi_Spoccia:/home/pi/mirror\n";
        out << line.str();
        written += line.str().size();
    }
}

// Restores the log file exactly as it was found, even if a fatal ASSERT_*
// exits the test early - GUI tests must leave every piece of state the way
// they found it, and the log is not part of the ApplicationState
// GuiTest::TearDown() already checks. A real crash of the process obviously
// skips this too, same as it skips every other destructor.
class ScopedSampleLogEntries {
    std::string path_;
    std::string originalContent_;
public:
    explicit ScopedSampleLogEntries(std::string path)
        : path_(std::move(path))
    {
        std::ostringstream content;
        content << std::ifstream(path_).rdbuf();
        originalContent_ = content.str();
        writeSampleLogEntries(path_);
    }
    ~ScopedSampleLogEntries()
    {
        std::ofstream(path_, std::ios::trunc) << originalContent_;
    }
};
}

// Regression test for the crash the removed "Log analysieren" button used to
// cause: a log question is now asked as an ordinary chat message (see
// ChatDialog::onSend()), so this drives the dialog exactly like a user would
// and waits for the assistant's answer instead of assuming a fixed delay.
TEST_F(GuiTest, LogFrageWirdImChatBeantwortet)
{
    const ScopedSampleLogEntries sampleLog(wstring_to_string(Logging::instance().getLogFilePathName()));

    activateMenuItem(mainWindow(), tul(id::Chat));

    // ChatDialog is a separate top-level window, not a child of mainWindow(),
    // so its controls need the global search (no parent) rather than
    // UserActions::find(). A single pump() is not enough to rely on: the
    // window manager still has to map and position the dialog before a
    // simulated click at its screen position lands on the right spot.
    wxWindow* input = nullptr;
    ASSERT_TRUE(waitUntil([&input] {
        input = wxWindow::FindWindowById(ID_CHAT_INPUT);
        return input != nullptr && input->IsShownOnScreen();
    })) << "Der Chat-Dialog wurde nicht geöffnet.";
    wxWindow* sendButton = wxWindow::FindWindowById(ID_CHAT_SEND);
    wxWindow* transcript = wxWindow::FindWindowById(ID_CHAT_TRANSCRIPT);
    ASSERT_NE(sendButton, nullptr);
    ASSERT_NE(transcript, nullptr);

    ASSERT_TRUE(typeInto(input, logQuestion));
    ASSERT_TRUE(clickWhenEnabled(sendButton));

    // The assistant answers on a worker thread; waiting for the button to
    // re-enable avoids guessing how long a real model needs to reply. Capped
    // at 4 minutes: a real model on real hardware is well past done by then,
    // so a wait this long only ever fires on a genuine hang (regression test
    // for exactly that - see LlamaClient::ensureLoaded()/send() for the two
    // bugs this covered: the batch-size crash, then the batch-size hang).
    ASSERT_TRUE(waitUntil([sendButton] { return sendButton->IsEnabled(); }, 240000))
        << "Der Assistent hat nicht geantwortet.";

    const wxString transcriptText = valueOf(transcript);
    EXPECT_NE(transcriptText.Find(logQuestion), wxNOT_FOUND) << "Die Frage fehlt im Verlauf.";
    EXPECT_TRUE(transcriptText.Contains("Assistent:") || transcriptText.Contains("Fehler:"))
        << "Es kam keine Antwort im Verlauf an.";

    // Hidden, not closed, mirroring how the dialog's own close handler leaves
    // it - the conversation stays alive for a later test that opens the chat.
    wxDialog* chatDialog = dynamic_cast<wxDialog*>(input->GetParent());
    if (chatDialog != nullptr)
        chatDialog->Hide();
}

}
