#include "conversation.h"
#include "model_client.h"
#include <memory>

class Harness {
public:
    Harness(std::unique_ptr<ModelClient> model, HarnessConfig cfg);
    StopReason run(InputSource& in, OutputSink& out);

private:
    std::unique_ptr<ModelClient> model_;
    Conversation                 conv_;
    HarnessConfig                cfg_;
};