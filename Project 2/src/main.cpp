#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>

/* One linked-list node stores one complete user/model conversation turn. */
struct ConversationTurn {
    char *prompt;                   /* Dynamically allocated user input. */
    char *response;                 /* Dynamically allocated model reply. */
    int counter;                    /* Number of turns this node has existed. */
    struct ConversationTurn *next; /* Next newer turn, or nullptr at the tail. */
};

/* The test model uses this structure to return text or request a tool. */
struct ModelResult {
    int tool_requested;      /* 0 for text, 1 for a tool request. */
    const char *tool_name;   /* Lowercase tool name, or nullptr for text. */
    const char *content;     /* Response text or tool input. */
};

/*
 * The second argument is nullptr on the first call. After a tool executes,
 * the harness calls the model again and supplies the tool's result there.
 */
struct ModelResult generate_response(
    const struct ConversationTurn *history_head,
    const char *tool_result
);

/*
 * This temporary test model verifies the harness without using an API.
 * It traverses the history from the oldest node to the newest node.
 */
struct ModelResult generate_response(
    const struct ConversationTurn *history_head,
    const char *tool_result
)
{
    const struct ConversationTurn *current = history_head;
    const struct ConversationTurn *newest = nullptr;
    struct ModelResult result;

    /* A supplied tool result becomes the model's final ordinary response. */
    if (tool_result != nullptr) {
        result.tool_requested = 0;
        result.tool_name = nullptr;
        result.content = tool_result;
        return result;
    }

    /* Walk through every saved turn and retain the final, newest node. */
    while (current != nullptr) {
        newest = current;
        current = current->next;
    }

    /* A missing newest prompt is an invalid model input. */
    if (newest == nullptr || newest->prompt == nullptr) {
        result.tool_requested = 0;
        result.tool_name = nullptr;
        result.content = nullptr;
        return result;
    }

    /* "calculate " requests the calculator with the remaining expression. */
    if (strncmp(newest->prompt, "calculate ", 10) == 0) {
        result.tool_requested = 1;
        result.tool_name = "calculator";
        result.content = newest->prompt + 10;
        return result;
    }

    /* All other input is echoed as a normal test-model response. */
    result.tool_requested = 0;
    result.tool_name = nullptr;
    result.content = newest->prompt;
    return result;
}

/* Allocate and initialize a conversation node that owns prompt. */
static struct ConversationTurn *create_node(char *prompt)
{
    /* Allocate enough memory for the node structure itself. */
    ConversationTurn *new_node = new (std::nothrow) ConversationTurn;

    /* Report allocation failure to the caller by returning nullptr. */
    if (new_node == nullptr) {
        return nullptr;
    }

    /* The response is not available until after the model is called. */
    new_node->prompt = prompt;
    new_node->response = nullptr;
    new_node->counter = 0;
    new_node->next = nullptr;

    return new_node;
}

/* Append a node so history remains ordered from oldest to newest. */
static void append_node(struct ConversationTurn **head,
                        struct ConversationTurn **tail,
                        struct ConversationTurn *new_node)
{
    /* The first node becomes both the head and tail. */
    if (*head == nullptr) {
        *head = new_node;
        *tail = new_node;
    } else {
        /* Later nodes are attached after the current tail. */
        (*tail)->next = new_node;
        *tail = new_node;
    }
}

/* Allocate and store a private copy of the model's borrowed response. */
static int store_response(struct ConversationTurn *node,
                          const char *model_response)
{
    size_t response_size;

    if (model_response == nullptr) {
        return 0;
    }

    /* Include one extra byte for the terminating null character. */
    response_size = strlen(model_response) + 1;
    node->response = new (std::nothrow) char[response_size];

    if (node->response == nullptr) {
        return 0;
    }

    /* Copy the text and its null terminator into node-owned memory. */
    memcpy(node->response, model_response, response_size);
    return 1;
}

/* Parse and evaluate exactly two numbers separated by one operator. */
static int calculate(const char *expression, double *result)
{
    char *end;
    double left_operand;
    double right_operand;
    char operation;

    if (expression == nullptr || result == nullptr) {
        return 0;
    }

    /* Convert the first number and record where that number ends. */
    left_operand = strtod(expression, &end);
    if (end == expression) {
        return 0;
    }

    /* Allow spaces between the first number and operator. */
    while (isspace((unsigned char)*end)) {
        end++;
    }

    /* Save the operator and advance to the second number. */
    operation = *end;
    if (operation == '\0') {
        return 0;
    }
    end++;

    /* Allow spaces between the operator and second number. */
    while (isspace((unsigned char)*end)) {
        end++;
    }

    /* Parse the second number using a separate ending pointer. */
    {
        char *right_end;

        right_operand = strtod(end, &right_end);
        if (right_end == end) {
            return 0;
        }
        end = right_end;
    }

    /* Only trailing whitespace is valid after the second number. */
    while (isspace((unsigned char)*end)) {
        end++;
    }

    if (*end != '\0') {
        return 0;
    }

    /* Execute one of the four supported arithmetic operations. */
    switch (operation) {
        case '+':
            *result = left_operand + right_operand;
            break;
        case '-':
            *result = left_operand - right_operand;
            break;
        case '*':
            *result = left_operand * right_operand;
            break;
        case '/':
            /* Division by zero is rejected as an invalid calculation. */
            if (right_operand == 0.0) {
                return 0;
            }
            *result = left_operand / right_operand;
            break;
        default:
            return 0;
    }

    return 1;
}

/* Increase the age of every stored conversation turn. */
static void increment_counters(struct ConversationTurn *head)
{
    struct ConversationTurn *current = head;

    while (current != nullptr) {
        current->counter++;
        current = current->next;
    }
}

/* Remove the oldest node after it has existed for more than five turns. */
static void check_head_counter(struct ConversationTurn **head,
                               struct ConversationTurn **tail)
{
    struct ConversationTurn *old_head;

    if (*head == nullptr || (*head)->counter <= 5) {
        return;
    }

    /* Save the old address before advancing the head pointer. */
    old_head = *head;
    *head = old_head->next;

    /* Keep the tail valid if removal leaves an empty list. */
    if (*head == nullptr) {
        *tail = nullptr;
    }

    /* Free both strings before freeing their containing node. */
    delete[] old_head->prompt;
    delete[] old_head->response;
    delete old_head;
}

/* Release every remaining node when the program ends or encounters an error. */
static void free_history(struct ConversationTurn *head)
{
    while (head != nullptr) {
        /* Save next before freeing head because freed memory cannot be read. */
        struct ConversationTurn *next_node = head->next;

        delete[] head->prompt;
        delete[] head->response;
        delete head;
        head = next_node;
    }
}

int main(void)
{
    /* The loop continues until exit or end-of-file is received. */
    int running = 1;

    /* An empty history begins with both list endpoints set to nullptr. */
    struct ConversationTurn *head = nullptr;
    struct ConversationTurn *tail = nullptr;

    /* Print the available commands before accepting the first prompt. */
    printf("Type your prompt into the terminal and press Enter.\n");
    printf("Type \"exit\" to close the agent.\n");
    printf("Type \"calculate 2 + 3\" to use the calculator.\n");

    /* Process one user prompt during each loop iteration. */
    while (running) {
        /* Start each prompt with 64 dynamically allocated bytes. */
        size_t prompt_capacity = 64;
        size_t prompt_length = 0;
        char *prompt = new (std::nothrow) char[prompt_capacity];
        int reached_end_of_file = 0;

        /* Stop safely if the initial prompt allocation fails. */
        if (prompt == nullptr) {
            fprintf(stderr, "Unable to allocate memory for prompt.\n");
            free_history(head);
            return EXIT_FAILURE;
        }

        /* Begin with a valid empty C string before calling fgets. */
        prompt[0] = '\0';
        printf("You: ");

        /* Keep reading chunks until fgets captures a newline or EOF. */
        while (1) {
            /* Double the buffer whenever no useful space remains. */
            if (prompt_length + 1 >= prompt_capacity) {
                size_t new_capacity = prompt_capacity * 2;
                char *resized_prompt = new (std::nothrow) char[new_capacity];

                if (resized_prompt == nullptr) {
                    fprintf(stderr, "Unable to resize memory for prompt.\n");
                    delete[] prompt;
                    free_history(head);
                    return EXIT_FAILURE;
                }

                /* Preserve the text before releasing the old buffer. */
                memcpy(resized_prompt, prompt, prompt_length + 1);
                delete[] prompt;
                prompt = resized_prompt;
                prompt_capacity = new_capacity;
            }

            /* Add the next input chunk after the characters already stored. */
            if (fgets(prompt + prompt_length,
                      (int)(prompt_capacity - prompt_length),
                      stdin) == nullptr) {
                /* EOF is a normal shutdown condition, not an input error. */
                if (feof(stdin)) {
                    reached_end_of_file = 1;
                    break;
                }

                fprintf(stderr, "Unable to read input.\n");
                delete[] prompt;
                free_history(head);
                return EXIT_FAILURE;
            }

            /* Measure only the newly appended portion of the prompt. */
            prompt_length += strlen(prompt + prompt_length);

            /* Enter ends the prompt; remove the newline before processing. */
            if (prompt_length > 0 && prompt[prompt_length - 1] == '\n') {
                prompt_length--;
                prompt[prompt_length] = '\0';
                break;
            }
        }

        /* EOF with no pending text ends the main loop immediately. */
        if (reached_end_of_file && prompt_length == 0) {
            printf("\n");
            delete[] prompt;
            break;
        }

        /* "hello" bypasses the model and conversation history. */
        if (strcmp(prompt, "hello") == 0) {
            printf("Agent: Hello! How can I help you?\n");
            delete[] prompt;

            if (reached_end_of_file) {
                running = 0;
            }

            continue;
        }

        /* "exit" ends the loop without storing a conversation node. */
        if (strcmp(prompt, "exit") == 0) {
            running = 0;
        } else {
            /* Create the newest history node before sending history to the model. */
            struct ConversationTurn *new_node = create_node(prompt);
            struct ModelResult model_result;
            /* Keep tool text alive until store_response copies it. */
            char formatted_tool_result[64];

            if (new_node == nullptr) {
                fprintf(stderr, "Unable to allocate memory for conversation node.\n");
                delete[] prompt;
                free_history(head);
                return EXIT_FAILURE;
            }

            /* Append first so the model can read the current prompt at the tail. */
            append_node(&head, &tail, new_node);

            /* nullptr indicates that no tool has been executed on this first call. */
            model_result = generate_response(head, nullptr);

            /* A value of 1 means content contains input for a requested tool. */
            if (model_result.tool_requested == 1) {
                double calculation_result;

                /* Only the lowercase calculator tool is currently supported. */
                if (model_result.tool_name == nullptr ||
                    strcmp(model_result.tool_name, "calculator") != 0) {
                    fprintf(stderr, "The model requested an unknown tool.\n");
                    free_history(head);
                    return EXIT_FAILURE;
                }

                /* Parse and execute the arithmetic expression from content. */
                if (!calculate(model_result.content, &calculation_result)) {
                    fprintf(stderr, "The calculator received an invalid expression.\n");
                    free_history(head);
                    return EXIT_FAILURE;
                }

                /* Convert the numeric answer into text for the second model call. */
                snprintf(formatted_tool_result,
                         sizeof(formatted_tool_result),
                         "%.15g",
                         calculation_result);

                /* The test model turns the tool result into the final response. */
                model_result = generate_response(head, formatted_tool_result);
            }

            /* The final result must be ordinary, non-nullptr response text. */
            if (model_result.tool_requested != 0 ||
                model_result.content == nullptr) {
                fprintf(stderr, "The model returned an invalid response.\n");
                free_history(head);
                return EXIT_FAILURE;
            }

            /* Store a dynamically allocated copy in the newest history node. */
            if (!store_response(new_node, model_result.content)) {
                fprintf(stderr, "Unable to store the model response.\n");
                free_history(head);
                return EXIT_FAILURE;
            }

            /* Display the same response that is now retained in history. */
            printf("Agent: %s\n", new_node->response);

            /* Age the nodes and discard the oldest once it exceeds five turns. */
            increment_counters(head);
            check_head_counter(&head, &tail);
        }

        /* Exit prompts are never owned by a node, so free them directly. */
        if (!running) {
            delete[] prompt;
        }

        /* Process a final unterminated line once, then stop after that turn. */
        if (reached_end_of_file) {
            running = 0;
        }
    }

    /* Release the five or fewer turns that remain at shutdown. */
    free_history(head);

    return EXIT_SUCCESS;
}
