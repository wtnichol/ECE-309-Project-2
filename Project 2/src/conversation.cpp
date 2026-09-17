#include "conversation.h"

Message::Message()
: role_(Role::User), content_(""){}  

Message::Message(Role incomingRole, std::string incomingContent)
    : role_(incomingRole), content_(incomingContent) {}

Role Message::role() const noexcept { 
    return role_;
}

const std::string& Message::content() const noexcept {
    return content_;
}

Conversation::Conversation() {}

std::size_t Conversation::size() const noexcept {
    return size_;
}