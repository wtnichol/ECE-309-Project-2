#include "core/conversation.h"
#include <stdexcept>
#include <memory>

Message::Message()
    : role_(Role::User), content_("") {}

Message::Message(Role incomingRole, std::string incomingContent)
    : role_(incomingRole), content_(incomingContent) {}

Role Message::role() const noexcept
{
    return role_;
}

const std::string &Message::content() const noexcept
{
    return content_;
}

Conversation::Conversation() {}

std::size_t Conversation::size() const noexcept
{
    return size_;
}

const Message &Conversation::at(std::size_t i) const
{
    if (i >= size_)
    {
        throw std::out_of_range("Index out of range");
    }
    return data_[i];
}

const Message *Conversation::begin() const noexcept
{
    return data_;
}

const Message *Conversation::end() const noexcept
{
    return data_ + size_;
}

void Conversation::append(Message m)
{
    if (size_ == capacity_)
    {
        std::size_t newCapacity;
        if (capacity_ == 0)
        {
            newCapacity = 1;
        }
        else
        {
            newCapacity = capacity_ * 2;
        }
        auto newData = std::make_unique<Message[]>(newCapacity);
        for (std::size_t i = 0; i < size_; i++)
        {
            newData[i] = data_[i];
        }
        delete[] data_;
        data_ = newData.release();
        capacity_ = newCapacity;
    }
    data_[size_] = m;
    size_ = size_ + 1;
}

Conversation::~Conversation()
{
    delete[] data_;
}

Conversation::Conversation(const Conversation &other)
{
    size_ = other.size_;
    capacity_ = other.capacity_;

    if (other.capacity_ > 0)
    {
        auto newData = std::make_unique<Message[]>(capacity_);

        for (std::size_t i = 0; i < size_; i++)
        {
            newData[i] = other.data_[i];
        }
        data_ = newData.release();
    }
}

Conversation &Conversation::operator=(const Conversation &other)
{
    if (this == &other)
    {
        return *this;
    }

    std::unique_ptr<Message[]> newData;
    if (other.capacity_ > 0)
    {
        newData = std::make_unique<Message[]>(other.capacity_);

        for (std::size_t i = 0; i < other.size_; i++)
        {
            newData[i] = other.data_[i];
        }
    }
    delete[] data_;
    data_ = newData.release();
    size_ = other.size_;
    capacity_ = other.capacity_;
    return *this;
}

Conversation::Conversation(Conversation &&other) noexcept
{
    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation &Conversation::operator=(Conversation &&other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    delete[] data_;

    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    return *this;
}
