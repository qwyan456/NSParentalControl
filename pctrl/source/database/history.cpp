#include "history.h"
#include "../helpers.h"
#include <string>

HistoryEntry::HistoryEntry(AccountUid uid, std::string date, u64 titleId, u16 durationInMinutes) 
: d_(std::make_shared<HistoryEntryData>()) { 
    d_->uid = uid;
    d_->date = date;
    d_->titleId = titleId;
    d_->durationInMinutes = durationInMinutes;
}

HistoryEntry::HistoryEntry(const HistoryEntry& orig)
: d_(orig.d_) {}

HistoryEntry& HistoryEntry::operator=(const HistoryEntry& orig) {    
    d_ = orig.d_;
    return *this;
}

AccountUid HistoryEntry::uid() const {
    return d_->uid;
}

std::string HistoryEntry::uidAsString() const {
    return alefbet::pctrl::helpers::accountUidToString(d_->uid);
}

std::string HistoryEntry::date() const {
    return d_->date;
}
    
u64 HistoryEntry::titleId() const {
    return d_->titleId;
}

u16 HistoryEntry::durationInMinutes() const {
    return d_->durationInMinutes;
}

History::History()
: d_(std::make_shared<HistoryData>()) {}

History::History(const History& orig)
: d_(orig.d_) {}

History& History::operator=(const History& orig) {
    d_ = orig.d_;
    return *this;
}

void History::addEntry(const HistoryEntry& entry) {
    d_->history.push_back(entry);
}

std::vector<HistoryEntry> History::entries() const {
    return d_->history;
}

std::vector<HistoryEntry> History::entries(const AccountUid& uid, const std::string& date, const u64& titleId) const {
    std::vector<HistoryEntry> entries;

    for(const auto& _entry: d_->history) {
        if(_entry.uid().uid[0] == uid.uid[0] && _entry.uid().uid[1] == uid.uid[1] && _entry.date() == date) {
            if(titleId > 0 && titleId == _entry.titleId()) {
                entries.push_back(_entry);
            } else {
                entries.push_back(_entry);
            }            
        }
    }

    return entries;
}

bool HistoryEntry::isValid() const {
    return d_->titleId != 0 && !d_->date.empty();
}

void HistoryEntry::setDurationInMinutes(u16 duration) {
    d_->durationInMinutes = duration;
}