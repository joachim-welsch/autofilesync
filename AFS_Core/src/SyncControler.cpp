#include "pch.h"
#include "SyncControler.h"
#include "Synchronize.h"
#include "Logging.h"

namespace AFS_Core {

std::unique_ptr<SyncControler> SyncControler::instance_ = nullptr;

SyncControler& SyncControler::instance() 
{
	if (instance_ == nullptr) {
		instance_ = SyncCtrlPtr(new SyncControler());
	}
	return *instance_.get();
}

SyncControler::~SyncControler()
{}

void SyncControler::register_progress_dialog(ProgressDialog* pdlg)
{
	log_debug(L"SyncControler::register_progress_dialog()");
	progress_dialog_ = pdlg;
}

std::optional<ProgressDialog*> SyncControler::progress_dialog()
{
	log_debug(L"SyncControler::progress_dialog()");
	ProgressDialog* pdlg = progress_dialog_.load();
	if (pdlg != nullptr)
		return std::optional<ProgressDialog*>(pdlg);
	else
		return std::nullopt;
}

bool SyncControler::start_sync(SyncType sync_type)
{
	log_debug(L"SyncControler::start_sync()");
	if (synchronize_ != nullptr)
		return false;
	state_ = SyncCtrlState::SyncActive;
    synchronize_ = Synchronize::create(sync_type, this);
	return true;
}

bool SyncControler::sync_finished() const
{
	return state_ == SyncCtrlState::SyncFinished;
}

void SyncControler::set_sync_finished() // called by Workerthread
{
	// GUI updates (ProgressDialog) must happen on the GUI thread; onTimer()
	// polls sync_finished() there and closes/deletes the dialog itself.
	log_debug(L"SyncControler::set_sync_finished()");
	state_ = SyncCtrlState::SyncFinished;
}

void SyncControler::destroy_synchronize()
{ 
	Synchronize::destroy();
	synchronize_ = nullptr;
}

void SyncControler::request_stop()
{
	// Non-blocking half of stop_sync(): signals the worker thread to stop and
	// returns immediately, letting the caller wait however suits it (e.g.
	// ProgressDialog::OnCancel() needs to keep pumping the GUI event queue
	// while waiting, which this class has no business knowing about).
	log_debug(L"SyncControler::request_stop()");
	if (state_ != SyncCtrlState::SyncActive)
		return;
	Synchronize::stop();
}

void SyncControler::stop_sync()
{
	log_debug(L"SyncControler::stop_sync()");
	if (state_ != SyncCtrlState::SyncActive)
		return;
	Synchronize::stop();
	while (!sync_finished()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void SyncControler::display_errormessages(bool yes_no)
{
	display_errormessages_ = yes_no;
}

bool SyncControler::display_errormessages() const
{
	return display_errormessages_;
}

SyncMessage SyncControler::get_message() const
{
	if (synchronize_ != nullptr)
		return synchronize_->get_message();
	else
		return SyncMessage();
}

} // namespace AFS_Core
