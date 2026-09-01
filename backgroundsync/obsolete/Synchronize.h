#ifndef SYNCHRONIZE_H
#define SYNCHRONIZE_H

#include <string>
#include "config.h"

class ProgressDialog;

class Synchronize
{
    const Config&           config_;
    std::size_t             copyCount_ = 0;
    static ProgressDialog*  progressDialog_;
    static bool             stop_;
public:
    Synchronize(const Config&);
    static void setProgressDialog(ProgressDialog*);
    static void setProgressMessage(const std::string&);
    static void stop();
    static bool stopped();
    void operator()();
    std::size_t copyCount() const { return copyCount_; }
};

#endif // SYNCHRONIZE_H
