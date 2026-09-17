#include <string>
#include "conversation.h"

struct StopReason {
    enum class Kind { Sentinel, TurnLimit, UserExit, ClientError } kind;
    std::string detail;
};

class TokenSink {
public:
    virtual ~TokenSink() = default;
    virtual void on_chunk(std::string_view chunk) = 0;
    virtual void on_complete() = 0;
};

class ModelClient {
public:
    virtual ~ModelClient() = default;

    virtual void generate(const Conversation& conv, TokenSink& sink) = 0;
    Message generate(const Conversation& conv); 
};