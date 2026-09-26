// tests/p2/test_p2.cpp
//
// YOUR test suite goes here. At least 12 assert-based test cases — see
// spec §5 for the required categories and the sample test for the
// expected level of rigor.
//
// This file is a stub so the project builds out of the box; replace the
// body of main() with your own tests.

#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"
#include <stdexcept>
#include <cassert>
#include <utility>
#include <memory>
#include <fstream>

int main()
{
    // TODO: write your tests here.
    int test = 1;
    // Test 1
    if (test == 1)
    {
        Conversation conv1;
        assert(conv1.size() == 0);
        assert(conv1.begin() == conv1.end());

        bool reject = false;
        try
        {
            conv1.at(0);
        }
        catch (const std::out_of_range &)
        {
            reject = true;
        }
        assert(reject);
        return 0;
    }
    // Test 2
    if (test == 2)
    {
        Conversation conv2;

        conv2.append(Message(Role::System, "What's good gang."));

        for (int i = 0; i < 5; i++)
        {
            conv2.append(Message(Role::User, "Banana"));
            conv2.append(Message(Role::Assistant, "Potassium"));

            assert(conv2.at(0).role() == Role::System);
            assert(conv2.at(0).content() == "What's good gang.");
        }
        assert(conv2.size() == 11);
        return 0;
    }
    // Test 3
    if (test == 3)
    {
        Conversation conv3;

        conv3.append(Message(Role::System, "HI MR.LLM"));
        conv3.append(Message(Role::User, "my name is NOT mr.llm, my name is John Rod"));

        Conversation conv3copy = conv3;

        assert(conv3copy.begin() != conv3.begin());
        assert(conv3copy.size() == conv3.size());
        return 0;
    }
    // Test 4
    if (test == 4)
    {
        Conversation conv4;

        conv4.append(Message(Role::User, "I love you 3000"));
        const Message *oldAddy = conv4.begin();

        Conversation theNewConv4(std::move(conv4));

        assert(theNewConv4.begin() == oldAddy);
        assert(conv4.begin() == nullptr);
        return 0;
    }
    // Test 5
    if (test == 5)
    {
        Conversation conv5;
        for (int i = 0; i < 6; i++)
        {
            const Message *oldAddy = conv5.begin();
            conv5.append(Message(Role::User, "Hi"));
            bool growOnNext = (i == 0 || i == 1 || i == 2 || i == 4);
            assert((conv5.begin() != oldAddy) == growOnNext);
            assert(conv5.size() == static_cast<std::size_t>(i + 1));
        }
        return 0;
    }
    // Test 6
    if (test == 6)
    {
        SentinelScanner gonnaScanIt("<|end_conversation|>");
        auto result = gonnaScanIt.feed("I don't think this says the sentinel bruv");
        assert(result.safe_text == "I don't think this says the sentinel bruv");
        assert(result.sentinel_found == false);
        auto lastBit = gonnaScanIt.flush();
        assert(lastBit.safe_text.empty());
        assert(lastBit.sentinel_found == false);
        return 0;
    }
    // Test 7
    if (test == 7)
    {
        SentinelScanner gonnaScanIt("<|end_conversation|>");
        auto chunk1 = gonnaScanIt.feed("Bye <|end_conver");
        assert(chunk1.safe_text == "Bye ");
        assert(chunk1.sentinel_found == false);
        auto chunk2 = gonnaScanIt.feed("sation|>");
        assert(chunk2.safe_text.empty());
        assert(chunk2.sentinel_found == true);
        return 0;
    }
    // Test 8
    if (test == 8)
    {
        SentinelScanner gonnaScanIt("<|end_conversation|>");
        auto partial = gonnaScanIt.feed("<|end_world|>");
        assert(partial.safe_text == "<|end_world|>");
        assert(partial.sentinel_found == false);
        return 0;
    }
    // Test 9
    if (test == 9)
    {
        std::string sentinel = "<|end_conversation|>";
        SentinelScanner gonnaScanIt(sentinel);
        std::size_t pending = 0;
        for (int i = 0; i < 100; i++)
        {
            for (char c : std::string("<|end_conversation|Q"))
            {
                std::string chunk;
                chunk = chunk + c;
                auto result = gonnaScanIt.feed(chunk);
                pending = pending + 1;
                assert(result.safe_text.size() <= pending);
                pending = pending - result.safe_text.size();
                assert(result.sentinel_found == false);
                assert(pending < sentinel.size());
            }
        }
        return 0;
    }
    // Test 10 or 11
    if (test == 10 || test == 11)
    {
        class Input : public InputSource
        {
        public:
            std::string read_line() override
            {
                return "Hello";
            }
            bool is_eof() const override
            {
                return false;
            }
        };
        class Output : public OutputSink
        {
        public:
            void write(std::string_view) override {}
        };
        Input input;
        Output output;
        HarnessConfig config;
        // Test 10
        if (test == 10)
        {
            config.max_turns = 1;
            Harness harness(std::make_unique<ScriptedModelClient>("scripts/greeting.script"), config);
            auto result = harness.run(input, output);
            assert(result.kind == StopReason::Kind::TurnLimit);
            assert(harness.conversation().size() == 2);
            return 0;
        }
        // Test 11
        else
        {
            config.max_turns = 10;
            Harness harness(std::make_unique<ScriptedModelClient>("scripts/greeting.script"), config);
            auto result = harness.run(input, output);
            assert(result.kind == StopReason::Kind::Sentinel);
            assert(harness.conversation().size() == 6);
            assert(harness.conversation().at(5).content() == "Goodbye!<|end_conversation|>");
            return 0;
        }
    }
    // Test 12
    if (test == 12)
    {
        Conversation conv12;
        conv12.append(Message(Role::Assistant, "General Kenobi"));
        std::ofstream file("test12_transcript.txt");
        file << "role: assistant\n"
             << conv12.at(0).content() << "\n";
        file.close();
        assert(!file.fail());
        ReplayModelClient replay("test12_transcript.txt");
        Message reply = replay.generate(conv12);
        assert(reply.role() == conv12.at(0).role());
        assert(reply.content() == conv12.at(0).content());
    }
}
