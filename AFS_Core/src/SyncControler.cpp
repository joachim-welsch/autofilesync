#include "pch.h"
#include "SyncControler.h"
#include "Synchronize.h"
#include "Logging.h"

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

void SyncControler::registerProgressDialog(ProgressDialog* pdlg)
{
	LOG_DEBUG(L"SyncControler::registerProgressDialog()");
	progressDialog_ = pdlg;
}

std::optional<ProgressDialog*> SyncControler::progressDialog()
{
	LOG_DEBUG(L"SyncControler::progressDialog()");
	ProgressDialog* pdlg = progressDialog_.load();
	if (pdlg != nullptr)
		return std::optional<ProgressDialog*>(pdlg);
	else
		return std::nullopt;
}

bool SyncControler::startSync(SyncType syncType)
{
	LOG_DEBUG(L"SyncControler::startSync()");
	if (synchronize_ != nullptr)
		return false;
	state_ = SyncCtrlState::SyncActive;
    synchronize_ = Synchronize::create(syncType, this);
	return true;
}

bool SyncControler::syncFinished() const
{
	return state_ == SyncCtrlState::SyncFinished;
}

void SyncControler::setSyncFinished() // called by Workerthread
{
	// GUI updates (ProgressDialog) must happen on the GUI thread; onTimer()
	// polls syncFinished() there and closes/deletes the dialog itself.
	LOG_DEBUG(L"SyncControler::setSyncFinished()");
	state_ = SyncCtrlState::SyncFinished;
}

void SyncControler::destroySynchronize()
{ 
	Synchronize::destroy();
	synchronize_ = nullptr;
}

void SyncControler::requestStop()
{
	// Non-blocking half of stopSync(): signals the worker thread to stop and
	// returns immediately, letting the caller wait however suits it (e.g.
	// ProgressDialog::OnCancel() needs to keep pumping the GUI event queue
	// while waiting, which this class has no business knowing about).
	LOG_DEBUG(L"SyncControler::requestStop()");
	if (state_ != SyncCtrlState::SyncActive)
		return;
	Synchronize::stop();
}

void SyncControler::stopSync()
{
	LOG_DEBUG(L"SyncControler::stopSync()");
	if (state_ != SyncCtrlState::SyncActive)
		return;
	Synchronize::stop();
	while (!syncFinished()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void SyncControler::displayErrormessages(bool YesNo)
{
	displayErrormessages_ = YesNo;
}

bool SyncControler::displayErrormessages() const
{
	return displayErrormessages_;
}

SyncMessage SyncControler::getMessage() const {
	if (synchronize_ != nullptr)
		return synchronize_->getMessage();
	else
		return SyncMessage();
}