# ECE 309: Project 2 — The Conversation Loop
**Weight:** 15% of project grade | **Duration:** Weeks 2–4

## 1. Project Overview & Learning Goals

An LLM harness is the program that sits between a language model and the outside world. A raw language model can only produce text. The harness is what turns that text into an agent: it maintains conversation history, decides when a session is over, handles token streaming, and manages user input loops.

In Project 1, you will build the core execution loop (`miniharness`) from scratch in C++. 

```text
                 ┌──────────────────────────────────────────────┐
                 │                  Harness                     │
                 │  ┌────────────────────────────────────────┐  │
   user input ──►│  │            run loop  (P1)              │  │
                 │  └───┬───────────────────────┬────────────┘  │
                 │      │                       │               │
                 │      ▼                       ▼               │
                 │  ┌────────────┐      ┌────────────────────┐  │
                 │  │Conversation│      │  ScriptedClient    │  │
                 │  │  (Custom)  │      │  ReplayClient      │  │
                 │  └─────┬──────┘      └─────────┬──────────┘  │
                 └────────┼───────────────────────┼─────────────┘
                          │                       │
                          ▼                       ▼
                     [Memory / Heap]       [TokenStream / Sink]
```

**Learning Goals:**
* Design an abstract base class and program against the interface.
* Manage object lifetime with `std::unique_ptr` and destructors. 
* Implement a sequence container (growable array) and reason about its amortized cost.
* Solve a real streaming problem: detect a multi-character sentinel in a character stream that arrives in arbitrary chunks.

---

## 2. Program Behaviors & Requirements

Your executable, `miniharness`, starts a conversation, alternates turns between a user and a model, and stops when the model emits the end-of-conversation sentinel `<|end_conversation|>` or when a turn limit is reached.

**Example CLI Interaction:**
```text
$ ./miniharness --script scripts/greeting.script
you> hello
assistant> Hi! What can I do for you today?
you> nothing, bye
assistant> Goodbye.
[conversation ended: stop sentinel after 2 turns]
```

**Required Behaviors:**
1. **Roles:** Every message carries a role: `System`, `User`, or `Assistant`. A system message, if present, is always first and is never evicted. 
2. **Two Model Clients:** 
   * `ScriptedModelClient` reads a `.script` file and returns the next scripted reply sequentially.
   * `ReplayModelClient` reads a previously recorded transcript and replays assistant turns verbatim. This is used by the test harness.
3. **Streaming & Sentinels:** The model interface must support delivering a reply in chunks. Chunk boundaries are arbitrary and *may split the sentinel* (e.g., `"Goodbye.<|end_"` followed by `"conversation|>"`). Your loop must terminate correctly and must not print the sentinel to the user.
4. **Turn Limit:** Passing `--max-turns N` (default 20) stops the loop with a distinct exit reason.
5. **Transcript Output:** Passing `--save transcript.txt` writes the full conversation to disk, such that feeding it back through `ReplayModelClient` reproduces the session exactly.
6. **Clean Shutdown:** Pressing Ctrl-D (EOF) on standard input ends the conversation gracefully and still writes the transcript.

---

## 3. Architecture & Class Design

You must implement the following classes. You are programming against these specific interfaces to ensure your codebase is ready for Project 2.

### 3.1 The `Message` Class (`core/message.h`)
Messages must carry a role and string content. Because your conversation history will allocate an array of these, you must provide a default constructor.

~~~cpp
enum class Role { System, User, Assistant };

class Message {
public:
    Message(); // Required to initialize empty array slots
    Message(Role role, std::string content);

    Role               role()    const noexcept;
    const std::string& content() const noexcept;

private:
    Role        role_;
    std::string content_;
};
~~~

### 3.2 The `Conversation` Container (`core/conversation.h`)
You must implement your own growable array. **The use of `std::vector` is strictly forbidden.** 

This class is the *only* place in your entire codebase where raw `new` and `delete` are permitted. 

~~~cpp
class Conversation {
public:
    // You must implement the Rule of Five:
    ~Conversation();
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    void append(Message m);

    std::size_t    size() const noexcept;
    const Message& at(std::size_t i) const;
    const Message* begin() const noexcept;
    const Message* end()   const noexcept;

private:
    Message*    data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};
~~~
* **Memory Management:** You must obey the Rule of Five. Your copy semantics must perform deep copies. Your move semantics must steal the pointer and zero out the source object. 
* **Amortized Cost:** You must document your chosen growth factor and prove the amortized $O(1)$ cost of the `append` operation in your Design Log.

### 3.3 The Model Interface (`model/model_client.h`)
~~~cpp
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
~~~
* **C++ Name Hiding:** This uses the Non-Virtual Interface (NVI) idiom. When you subclass `ModelClient` (e.g., for `ScriptedModelClient`) and override the virtual `generate` function, C++ will hide the non-virtual version. You must use `using ModelClient::generate;` in your derived classes to prevent compiler errors.

### 3.4 The `SentinelScanner` (`core/sentinel_scanner.h`)
This class consumes chunks, emits text guaranteed not to be part of the sentinel, and reports when the sentinel is found.

~~~cpp
class SentinelScanner {
public:
    explicit SentinelScanner(std::string sentinel);

    struct Out { std::string safe_text; bool sentinel_found; };
    Out feed(std::string_view chunk);
    Out flush();       

private:
    std::string sentinel_;
    std::string pending_;   
};
~~~
* **Bounded Memory:** You must prove in your log that `pending_` never exceeds `sentinel_.size() - 1` characters. A naive approach of concatenating all chunks into one string creates an $O(N^2)$ memory leak and will fail the autograder stress tests.
* **Stretch Goal:** Replace the naive scanner approach with the **Knuth-Morris-Pratt (KMP)** algorithm and measure the performance difference on adversarial input (e.g., streaming `<|end_<|end_<|end_...`).

### 3.5 The `Harness` (`harness/harness.h`)
~~~cpp
class Harness {
public:
    Harness(std::unique_ptr<ModelClient> model, HarnessConfig cfg);
    StopReason run(InputSource& in, OutputSink& out);

private:
    std::unique_ptr<ModelClient> model_;
    Conversation                 conv_;
    HarnessConfig                cfg_;
};
~~~
* **Logic Requirement:** Inside `run()`, you must append the user's input to the `Conversation`. When the model finishes streaming, you must gather that streamed text and append it as an Assistant message. If you do not save the assistant's reply, your transcripts will be incomplete.

---

## 4. Common Pitfalls

Review these common mistakes before submitting, as they are the most frequent causes of failed test cases:
1. **Ownership Issues:** Storing `Message*` in the conversation and letting the caller keep ownership. Ownership must be unambiguous. 
2. **Sentinel Leaks:** Printing the reply to the terminal *before* checking for the sentinel, resulting in `<|end_conversation|>` leaking to the user.
3. **Shallow Copies:** A `Conversation` copy constructor that copies the pointer instead of the buffer. This causes a double-free at scope exit which AddressSanitizer will immediately flag.

---

## 5. Deliverables & Testing

Your repository must be built with CMake and compile cleanly under AddressSanitizer (`-fsanitize=address`). 

**Folder Structure:**
* `src/` (Source files)
* `include/` (Headers)
* `tests/` (Test suite)
* `docs/` (Design log)

**Repository Structure**
```text
├── CMakeLists.txt
├── include/
│   ├── core/
│   │   ├── conversation.h
│   │   ├── message.h
│   │   └── sentinel_scanner.h
│   ├── harness/
│   │   └── harness.h
│   └── model/
│       ├── model_client.h
│       ├── scripted_client.h
│       └── replay_client.h
├── src/
│   ├── conversation.cpp
│   ├── sentinel_scanner.cpp
│   ├── model_client.cpp
│   ├── scripted_client.cpp
│   ├── replay_client.cpp
│   ├── harness.cpp
│   └── main.cpp
├── tests/
│   └── p1/
│       └── test_p1.cpp
└── docs/
    └── design-log-p1.md
```

**Test Suite Requirements (`tests/p1/test_p1.cpp`):**
You must write at least 12 assert-based test cases covering:
1. **Empty Conversation Bounds:** Handle empty conversations without out-of-bounds access.
2. **System Message Ordering:** Ensure system messages remain pinned at the front.
3. **Rule of Five (Copy):** Assert that copy constructors allocate entirely different pointer addresses.
4. **Rule of Five (Move):** Assert that move constructors successfully steal the data pointer and zero the source.
5. **Scanner (Clean Text):** Verify the scanner processes strings with no sentinel correctly.
6. **Scanner (Split Sentinel):** Prove the scanner catches the sentinel when split across *every possible boundary* (programmatically loop over all split points).
7. **Scanner (False Alarms):** Ensure the scanner does not trigger on partial matches (e.g., `<|end_world|>`).
8. **Harness (Turn Limit):** Assert the loop stops cleanly with the correct `TurnLimit` reason.
9. **Harness (EOF):** Assert the loop handles `Ctrl-D` mid-conversation.
10. **Harness (Sentinel Halt):** Assert the loop halts exactly when the sentinel is emitted.
11. **Clean Destruction:** Ensure the harness destructs gracefully without throwing exceptions or memory leaks.
12. **Transcript Round-Trip:** Save a mock conversation to a `.txt` file, load it via `ReplayModelClient`, and assert it plays back identically.

**Design Log (`docs/design-log-p1.md`):**
A 500–800 word Markdown document defending your design. It must cover:
* Your growth factor choice and the proof of amortized $O(1)$ insertions.
* Evidence of how your code handles the Rule of Five safely.
* The mathematical argument proving your pending-buffer never exceeds the sentinel length.
* One thing you would design differently in hindsight.

**Prepare Final Submission Files:** Create the following two files to submit for grading:
* **github.txt:** A simple text file containing the direct URL to your GitHub repository.
* **github.zip:** A compressed ZIP file containing your entire repository as a backup.

---

## 6. Grading Rubric

| Criterion | Points | Evaluation |
|---|---|---|
| **Loop Semantics** | 25 | Automated tests checking turn limits, EOF, and stopping. |
| **ModelClient & Interfaces** | 15 | Code inspection ensuring no downcasting in the loop. |
| **Rule of Five & Memory Safety** | 20 | Automated tests compiled with AddressSanitizer (must show 0 leaks). |
| **Sentinel Bounded Memory** | 20 | Stress test feeding 4MB streams one byte at a time. |
| **Test Suite Quality** | 10 | Manual review of your 12 test cases. |
| **Design Log** | 10 | Evaluation of your amortized math and buffer proofs. |

## 7. Appendix A — Transcript and Script Format

The `.script` files (input) and `transcript.txt` files (output) are line-oriented, with one message per block. Blocks are separated by a line containing exactly `---`.

* **Content Escaping:** A line containing exactly `---` is strictly reserved for message boundaries. A message's text content cannot contain a bare `---` line.
* **System Messages:** If a script or transcript begins with a block tagged `role: system`, that becomes the initial system message.

### Standard Transcript Format
Used for `--save` outputs and `ReplayModelClient` inputs.
~~~text
role: system
Be concise.
---
role: user
hello
---
role: assistant
Hi! What can I do for you today?
---
role: user
goodbye
---
role: assistant
Goodbye.<|end_conversation|>
~~~

### Script Format (`ScriptedModelClient`)
A `.script` file is similar to a transcript but can include directives to test your harness's streaming logic. **Directives must appear *before* the `role:` declaration in any block, in any order.**

~~~text
match: /file|todo/i
chunk: 5
role: assistant
This will stream 5 characters at a time.<|end_conversation|>
~~~

* **`chunk: N`** — Forces the `ScriptedModelClient` to emit the reply in chunks of `N` characters. This is how you test that your `SentinelScanner` correctly handles sentinels split across chunks. *If omitted, the default behavior is to emit the entire message as a single chunk.*
* **Block Exhaustion:** If the `ScriptedModelClient` is asked to generate a reply but has exhausted all of its available scripted blocks, it must trigger a loop termination and yield a `ClientError` StopReason.
* **`match: /pattern/i`** — *(Optional Stretch Goal)* Allows the scripted client to select a reply by matching a regex pattern against the most recent user message. Assume the default `std::regex` flavor (ECMAScript) is used. **Semantics:** Search proceeds top-to-bottom through the unconsumed blocks. The first block whose pattern matches wins. A block without a `match` directive acts as an unconditional catch-all default. If you do not implement branching, your client can simply ignore `match` directives and read the blocks sequentially.