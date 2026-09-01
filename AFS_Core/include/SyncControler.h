#pragma once
#include <optional>
#include <atomic>
#include <memory>
#include "Global.h"
#include "SyncMessage.h"

class ProgressDialog;
class Synchronize;

enum class SyncCtrlState { Waiting, SyncActive, SyncFinished };

class SyncControler {
	using SyncCtrlPtr = std::unique_ptr<SyncControler>;

	static SyncCtrlPtr  instance_;
	std::atomic<ProgressDialog*> progressDialog_ = nullptr;
	Synchronize*		synchronize_ = nullptr;
	bool				displayErrormessages_ = false;
	std::atomic<SyncCtrlState>	state_ = SyncCtrlState::Waiting;
	std::size_t			countFiles_ = 0;
	
protected:
	void setSyncFinished();
	SyncControler() = default;
public:
	~SyncControler();
	static SyncControler& instance();
	void registerProgressDialog(ProgressDialog*);
	std::optional<ProgressDialog*> progressDialog();
	bool startSync(SyncType syncType);
	void requestStop();
	void stopSync();
	bool syncFinished() const;
	void resetState();
	void destroySynchronize();
	void displayErrormessages(bool);
	bool displayErrormessages() const;
	SyncMessage getMessage() const;
	void countFiles(std::size_t);
	std::size_t countFiles() const;
	friend class Synchronize;
};

//##############################################################
inline
void SyncControler::countFiles(std::size_t countFiles)
{ 
	countFiles_ = countFiles;
}

inline
std::size_t SyncControler::countFiles() const
{
	return countFiles_;
}

inline
void SyncControler::resetState() {
	state_ = SyncCtrlState::Waiting;
}