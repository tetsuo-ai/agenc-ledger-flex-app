#include "agenc_instruction.h"
#include "common_byte_strings.h"
#include "compute_budget_instruction.h"
#include "sol/message.h"
#include "sol/print_config.h"
#include "sol/transaction_summary.h"
#include "system_instruction.h"
#include "test_utils.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define AGENC_PROGRAM_INDEX 11
#define SYSTEM_INDEX        10
#define COMPUTE_BUDGET_INDEX 9
#define MESSAGE_PROGRAM_INDEX 8
#define MESSAGE_SYSTEM_INDEX  7

typedef struct TestMessageBuilder {
    uint8_t data[800];
    size_t length;
} TestMessageBuilder;

static void init_pubkeys(Pubkey pubkeys[12]) {
    for (size_t i = 0; i < 12; i++) {
        memset(pubkeys[i].data, (int) (i + 1), PUBKEY_SIZE);
    }
    memcpy(&pubkeys[COMPUTE_BUDGET_INDEX], &compute_budget_program_id, PUBKEY_SIZE);
    memcpy(&pubkeys[SYSTEM_INDEX], &system_program_id, PUBKEY_SIZE);
    memcpy(&pubkeys[AGENC_PROGRAM_INDEX], &agenc_mainnet_preset_program_id, PUBKEY_SIZE);
}

static MessageHeader test_header(Pubkey pubkeys[12], size_t instruction_count) {
    MessageHeader header = {
        false,
        0,
        {2, 0, 1, 12},
        pubkeys,
        NULL,
        instruction_count,
    };
    return header;
}

static void assert_display(size_t index, const char *title, const char *text);
static void assert_display_title(size_t index, const char *title);
static void assert_summary_count(size_t expected_count);

static void builder_append(TestMessageBuilder *builder, const void *data, size_t length) {
    assert(builder->length + length <= sizeof(builder->data));
    if (length > 0) {
        memcpy(&builder->data[builder->length], data, length);
    }
    builder->length += length;
}

static void builder_append_u8(TestMessageBuilder *builder, uint8_t value) {
    builder_append(builder, &value, sizeof(value));
}

static void builder_append_shortvec(TestMessageBuilder *builder, size_t value) {
    assert(value < 0x4000);
    if (value < 0x80) {
        builder_append_u8(builder, (uint8_t) value);
        return;
    }
    builder_append_u8(builder, (uint8_t) ((value & 0x7f) | 0x80));
    builder_append_u8(builder, (uint8_t) (value >> 7));
}

static void builder_append_instruction(TestMessageBuilder *builder,
                                       uint8_t program_index,
                                       const uint8_t *accounts,
                                       size_t accounts_length,
                                       const uint8_t *data,
                                       size_t data_length) {
    builder_append_u8(builder, program_index);
    builder_append_shortvec(builder, accounts_length);
    builder_append(builder, accounts, accounts_length);
    builder_append_shortvec(builder, data_length);
    builder_append(builder, data, data_length);
}

static void builder_append_message_header(TestMessageBuilder *builder,
                                          Pubkey pubkeys[12],
                                          uint8_t readonly_unsigned_accounts,
                                          size_t instruction_count) {
    Hash blockhash = {{BYTES32_BS58_8}};
    builder_append_u8(builder, 2);  // required signatures
    builder_append_u8(builder, 0);  // readonly signed accounts
    builder_append_u8(builder, readonly_unsigned_accounts);
    builder_append_shortvec(builder, 12);
    builder_append(builder, pubkeys, PUBKEY_SIZE * 12);
    builder_append(builder, &blockhash, sizeof(blockhash));
    builder_append_shortvec(builder, instruction_count);
}

static void build_single_agenc_message(TestMessageBuilder *builder,
                                       const uint8_t *accounts,
                                       size_t accounts_length,
                                       const uint8_t *data,
                                       size_t data_length) {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    builder_append_message_header(builder, pubkeys, 2, 1);
    builder_append_instruction(builder,
                               AGENC_PROGRAM_INDEX,
                               accounts,
                               accounts_length,
                               data,
                               data_length);
}

static void build_create_review_message(TestMessageBuilder *builder,
                                        const uint8_t *create_data,
                                        size_t create_data_length,
                                        bool include_compute_budget) {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    uint8_t compute_unit_limit_data[] = {
        ComputeBudgetChangeUnitLimit,
        0x40,
        0x0d,
        0x03,
        0x00,
    };
    uint8_t create_accounts[] = {0, 1, 2, 3, 4, 6, 6, SYSTEM_INDEX};
    uint8_t configure_accounts[] = {0, 4, 5, 2, 6, SYSTEM_INDEX};
    uint8_t configure_data[] = {
        0x0b, 0x4f, 0x13, 0xbc, 0x0d, 0x20, 0xf4, 0x5a,  // discriminator
        0x01,                                            // mode
        0x10, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // review_window_secs
        0x00,                                            // validator_quorum
        0x00,                                            // attestor none
    };

    builder_append_message_header(builder,
                                  pubkeys,
                                  include_compute_budget ? 3 : 2,
                                  include_compute_budget ? 3 : 2);
    if (include_compute_budget) {
        builder_append_instruction(builder,
                                   COMPUTE_BUDGET_INDEX,
                                   NULL,
                                   0,
                                   compute_unit_limit_data,
                                   ARRAY_LEN(compute_unit_limit_data));
    }
    builder_append_instruction(builder,
                               AGENC_PROGRAM_INDEX,
                               create_accounts,
                               ARRAY_LEN(create_accounts),
                               create_data,
                               create_data_length);
    builder_append_instruction(builder,
                               AGENC_PROGRAM_INDEX,
                               configure_accounts,
                               ARRAY_LEN(configure_accounts),
                               configure_data,
                               ARRAY_LEN(configure_data));
}

static int process_builder_message(TestMessageBuilder *builder, bool add_fee_payer) {
    PrintConfig print_config = {0};
    print_config.expert_mode = true;
    Parser parser = {builder->data, builder->length};
    BAIL_IF(parse_message_header(&parser, &print_config.header));

    transaction_summary_reset();
    BAIL_IF(process_message_body(parser.buffer, parser.buffer_length, &print_config));
    if (add_fee_payer) {
        BAIL_IF(transaction_summary_set_fee_payer_pubkey(&print_config.header.pubkeys[0]));
    }
    return 0;
}

static void assert_process_agenc_message(TestMessageBuilder *builder,
                                         size_t expected_count,
                                         const char *action) {
    assert(process_builder_message(builder, true) == 0);
    assert_summary_count(expected_count);

    transaction_type_t transaction_type;
    transaction_summary_get_transaction_type(&transaction_type);
    assert(transaction_type == TRANSACTION_TYPE_AGENC_ACTION);

    assert_display(0, "AgenC action", action);
    assert_display_title(expected_count - 1, "Fee payer");
}

static void assert_reject_message(TestMessageBuilder *builder) {
    assert(process_builder_message(builder, false) != 0);
}

static void assert_display(size_t index, const char *title, const char *text) {
    assert(transaction_summary_display_item(index, DisplayFlagLongPubkeys) == 0);
    assert_string_equal(G_transaction_summary_title, title);
    assert_string_equal(G_transaction_summary_text, text);
}

static void assert_display_title(size_t index, const char *title) {
    assert(transaction_summary_display_item(index, DisplayFlagLongPubkeys) == 0);
    assert_string_equal(G_transaction_summary_title, title);
}

static void assert_summary_count(size_t expected_count) {
    enum SummaryItemKind kinds[MAX_TRANSACTION_SUMMARY_ITEMS];
    size_t num_kinds = 0;
    assert(transaction_summary_finalize(kinds, &num_kinds) == 0);
    assert(num_kinds == expected_count);
}

void test_is_agenc_program_id() {
    assert(is_agenc_program_id(&agenc_artifact_program_id));
    assert(is_agenc_program_id(&agenc_mainnet_preset_program_id));

    Pubkey other = {{BYTES32_BS58_8}};
    assert(!is_agenc_program_id(&other));
}

void test_parse_register_agent() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 1);

    uint8_t accounts[] = {0, 1, 6, SYSTEM_INDEX};
    uint8_t data[] = {
        0x87, 0x9d, 0x42, 0xc3, 0x02, 0x71, 0xaf, 0x1e,  // discriminator
        BYTES32_BS58_2,                                   // agent_id
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // capabilities
        0x13, 0x00, 0x00, 0x00,                          // endpoint length
        'h',  't',  't',  'p',  's',  ':',  '/',  '/',
        'a',  'g',  'e',  'n',  't',  '.',  'l',  'o',
        'c',  'a',  'l',                                  // endpoint
        0x00,                                             // metadata_uri none
        0x80, 0x96, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00,  // stake_amount
    };
    Instruction instruction = {
        AGENC_PROGRAM_INDEX,
        accounts,
        ARRAY_LEN(accounts),
        data,
        ARRAY_LEN(data),
    };

    AgencInfo info;
    assert(parse_agenc_instructions(&instruction, &header, &info) == 0);
    assert(info.kind == AgencInstructionRegisterAgent);
    assert(info.register_agent.agent == &pubkeys[0]);
    assert(info.register_agent.protocol_config == &pubkeys[1]);
    assert(info.register_agent.authority == &pubkeys[6]);
    assert(info.register_agent.capabilities == 1);
    assert(info.register_agent.endpoint.length == 19);
    assert(info.register_agent.has_metadata_uri == false);
    assert(info.register_agent.stake_amount == 10000000);

    transaction_summary_reset();
    assert(print_agenc_info(&info, NULL) == 0);
    assert_summary_count(7);
    assert_display(0, "AgenC action", "Register agent");
    assert_display(1, "Stake", "0.01 SOL");
    assert_display(4, "Capabilities", "1");
    assert_display(5, "Endpoint", "https://agent.local");
}

void test_process_register_agent_message() {
    uint8_t accounts[] = {0, 1, 6, SYSTEM_INDEX};
    uint8_t data[] = {
        0x87, 0x9d, 0x42, 0xc3, 0x02, 0x71, 0xaf, 0x1e,  // discriminator
        BYTES32_BS58_2,                                   // agent_id
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // capabilities
        0x13, 0x00, 0x00, 0x00,                          // endpoint length
        'h',  't',  't',  'p',  's',  ':',  '/',  '/',
        'a',  'g',  'e',  'n',  't',  '.',  'l',  'o',
        'c',  'a',  'l',                                  // endpoint
        0x00,                                             // metadata_uri none
        0x80, 0x96, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00,  // stake_amount
    };

    TestMessageBuilder builder = {0};
    build_single_agenc_message(&builder, accounts, ARRAY_LEN(accounts), data, ARRAY_LEN(data));
    assert_process_agenc_message(&builder, 8, "Register agent");
}

void test_parse_create_task_with_review_and_print() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 2);

    uint8_t create_accounts[] = {0, 1, 2, 3, 4, 6, 6, SYSTEM_INDEX};
    uint8_t create_data[] = {
        0xc2, 0x50, 0x06, 0xb4, 0xe8, 0x7f, 0x30, 0xab,  // discriminator
        BYTES32_BS58_2,                                   // task_id
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // required_capabilities
        't',  'e',  's',  't',  0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,                              // description
        0x00, 0xca, 0x9a, 0x3b, 0x00, 0x00, 0x00, 0x00,  // reward_amount
        0x01,                                            // max_workers
        0x00, 0xf1, 0x53, 0x65, 0x00, 0x00, 0x00, 0x00,  // deadline
        0x00,                                            // task_type
        0x00,                                            // constraint_hash none
        0x05, 0x00,                                      // min_reputation
        0x00,                                            // reward_mint none
    };
    Instruction create_ix = {
        AGENC_PROGRAM_INDEX,
        create_accounts,
        ARRAY_LEN(create_accounts),
        create_data,
        ARRAY_LEN(create_data),
    };

    AgencInfo create_info;
    assert(parse_agenc_instructions(&create_ix, &header, &create_info) == 0);
    assert(create_info.kind == AgencInstructionCreateTask);
    assert(create_info.create_task.reward_amount == 1000000000);
    assert(create_info.create_task.max_workers == 1);
    assert(create_info.create_task.deadline == 1700000000);
    assert(create_info.create_task.min_reputation == 5);
    assert(create_info.create_task.reward_mint == NULL);
    assert(create_info.create_task.creator == &pubkeys[6]);
    transaction_summary_reset();
    // A lone create_task now decodes (renders standalone) instead of forcing blind signing.
    assert(print_agenc_info(&create_info, NULL) == 0);

    uint8_t configure_accounts[] = {0, 4, 5, 2, 6, SYSTEM_INDEX};
    uint8_t configure_data[] = {
        0x0b, 0x4f, 0x13, 0xbc, 0x0d, 0x20, 0xf4, 0x5a,  // discriminator
        0x01,                                            // mode
        0x10, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // review_window_secs
        0x00,                                            // validator_quorum
        0x00,                                            // attestor none
    };
    Instruction configure_ix = {
        AGENC_PROGRAM_INDEX,
        configure_accounts,
        ARRAY_LEN(configure_accounts),
        configure_data,
        ARRAY_LEN(configure_data),
    };

    AgencInfo configure_info;
    assert(parse_agenc_instructions(&configure_ix, &header, &configure_info) == 0);
    assert(configure_info.kind == AgencInstructionConfigureTaskValidation);
    assert(configure_info.configure_task_validation.review_window_secs == 3600);

    transaction_summary_reset();
    assert(print_agenc_create_task_with_review_info(&create_info, &configure_info, NULL) == 0);
    assert_summary_count(10);
    assert_display(0, "AgenC action", "Create task");
    assert_display(1, "Reward", "1 SOL");
    assert_display_title(4, "Content hash");
    assert_display(6, "Max workers", "1");
    assert_display(9, "Review window", "3600");
}

void test_process_create_task_with_review_message() {
    Pubkey pubkeys[9];
    for (size_t i = 0; i < 7; i++) {
        memset(pubkeys[i].data, (int) (i + 1), PUBKEY_SIZE);
    }
    memcpy(&pubkeys[MESSAGE_SYSTEM_INDEX], &system_program_id, PUBKEY_SIZE);
    memcpy(&pubkeys[MESSAGE_PROGRAM_INDEX], &agenc_mainnet_preset_program_id, PUBKEY_SIZE);

    Hash blockhash = {{BYTES32_BS58_8}};
    uint8_t create_accounts[] = {0, 1, 2, 3, 4, 6, 6, MESSAGE_SYSTEM_INDEX};
    uint8_t create_data[] = {
        0xc2, 0x50, 0x06, 0xb4, 0xe8, 0x7f, 0x30, 0xab,  // discriminator
        BYTES32_BS58_2,                                   // task_id
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // required_capabilities
        't',  'e',  's',  't',  0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,                              // description
        0x00, 0xca, 0x9a, 0x3b, 0x00, 0x00, 0x00, 0x00,  // reward_amount
        0x01,                                            // max_workers
        0x00, 0xf1, 0x53, 0x65, 0x00, 0x00, 0x00, 0x00,  // deadline
        0x00,                                            // task_type
        0x00,                                            // constraint_hash none
        0x05, 0x00,                                      // min_reputation
        0x00,                                            // reward_mint none
    };
    uint8_t configure_accounts[] = {0, 4, 5, 2, 6, MESSAGE_SYSTEM_INDEX};
    uint8_t configure_data[] = {
        0x0b, 0x4f, 0x13, 0xbc, 0x0d, 0x20, 0xf4, 0x5a,  // discriminator
        0x01,                                            // mode
        0x10, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // review_window_secs
        0x00,                                            // validator_quorum
        0x00,                                            // attestor none
    };

    TestMessageBuilder builder = {0};
    builder_append_u8(&builder, 2);  // required signatures
    builder_append_u8(&builder, 0);  // readonly signed accounts
    builder_append_u8(&builder, 2);  // readonly unsigned accounts: system + AgenC program
    builder_append_shortvec(&builder, ARRAY_LEN(pubkeys));
    builder_append(&builder, pubkeys, sizeof(pubkeys));
    builder_append(&builder, &blockhash, sizeof(blockhash));
    builder_append_shortvec(&builder, 2);
    builder_append_instruction(&builder,
                               MESSAGE_PROGRAM_INDEX,
                               create_accounts,
                               ARRAY_LEN(create_accounts),
                               create_data,
                               ARRAY_LEN(create_data));
    builder_append_instruction(&builder,
                               MESSAGE_PROGRAM_INDEX,
                               configure_accounts,
                               ARRAY_LEN(configure_accounts),
                               configure_data,
                               ARRAY_LEN(configure_data));

    PrintConfig print_config = {0};
    print_config.expert_mode = true;
    Parser parser = {builder.data, builder.length};
    assert(parse_message_header(&parser, &print_config.header) == 0);

    transaction_summary_reset();
    assert(process_message_body(parser.buffer, parser.buffer_length, &print_config) == 0);
    assert(transaction_summary_set_fee_payer_pubkey(&print_config.header.pubkeys[0]) == 0);
    assert_summary_count(11);

    transaction_type_t transaction_type;
    transaction_summary_get_transaction_type(&transaction_type);
    assert(transaction_type == TRANSACTION_TYPE_AGENC_ACTION);

    assert_display(0, "AgenC action", "Create task");
    assert_display(1, "Reward", "1 SOL");
    assert_display(6, "Max workers", "1");
    assert_display(9, "Review window", "3600");
    assert_display_title(10, "Fee payer");
}

void test_parse_set_task_job_spec() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 1);

    uint8_t accounts[] = {2, 0, 3, 4, 5, 6, SYSTEM_INDEX};
    uint8_t data[] = {
        0x86, 0x66, 0x66, 0x56, 0x1f, 0xa4, 0xca, 0xc1,  // discriminator
        BYTES32_BS58_3,                                   // job_spec_hash
        0x04, 0x00, 0x00, 0x00,                          // Borsh string length
        'i',  'p',  'f',  's',
    };
    Instruction instruction = {
        AGENC_PROGRAM_INDEX,
        accounts,
        ARRAY_LEN(accounts),
        data,
        ARRAY_LEN(data),
    };

    AgencInfo info;
    assert(parse_agenc_instructions(&instruction, &header, &info) == 0);
    assert(info.kind == AgencInstructionSetTaskJobSpec);
    assert(info.set_task_job_spec.job_spec_uri.length == 4);
    assert(strncmp(info.set_task_job_spec.job_spec_uri.string, "ipfs", 4) == 0);

    transaction_summary_reset();
    assert(print_agenc_info(&info, NULL) == 0);
    assert_summary_count(6);
    assert_display(0, "AgenC action", "Attach job spec");
    assert_display(4, "Job spec URI", "ipfs");
}

void test_parse_claim_task() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 1);

    uint8_t accounts[] = {0, 1, 2, 3, 4, 5, SYSTEM_INDEX};
    uint8_t data[] = {0xe6, 0x28, 0x6b, 0x6d, 0xd0, 0xe4, 0xaf, 0x1f};
    Instruction instruction = {
        AGENC_PROGRAM_INDEX,
        accounts,
        ARRAY_LEN(accounts),
        data,
        ARRAY_LEN(data),
    };

    AgencInfo info;
    assert(parse_agenc_instructions(&instruction, &header, &info) == 0);
    assert(info.kind == AgencInstructionClaimTaskWithJobSpec);
    assert(info.claim_task_with_job_spec.task == &pubkeys[0]);
    assert(info.claim_task_with_job_spec.claim == &pubkeys[2]);
    assert(info.claim_task_with_job_spec.worker == &pubkeys[4]);
    assert(info.claim_task_with_job_spec.authority == &pubkeys[5]);

    transaction_summary_reset();
    assert(print_agenc_info(&info, NULL) == 0);
    assert_summary_count(6);
    assert_display(0, "AgenC action", "Claim task");
}

void test_parse_submit_task_result() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 1);

    uint8_t accounts[] = {0, 1, 2, 3, 4, 5, 6, SYSTEM_INDEX};
    uint8_t data[] = {
        0x27, 0x6c, 0x4a, 0x04, 0x42, 0x7d, 0x9d, 0x07,  // discriminator
        BYTES32_BS58_4,                                   // proof_hash
        0x01,                                             // result_data some
        BYTES32_BS58_5,
        BYTES32_BS58_6,
    };
    Instruction instruction = {
        AGENC_PROGRAM_INDEX,
        accounts,
        ARRAY_LEN(accounts),
        data,
        ARRAY_LEN(data),
    };

    AgencInfo info;
    assert(parse_agenc_instructions(&instruction, &header, &info) == 0);
    assert(info.kind == AgencInstructionSubmitTaskResult);
    assert(info.submit_task_result.has_result_data);
    assert(!info.submit_task_result.has_artifact_hash);
    assert(info.submit_task_result.task_submission == &pubkeys[3]);

    transaction_summary_reset();
    assert(print_agenc_info(&info, NULL) == 0);
    assert_summary_count(7);
    assert_display(0, "AgenC action", "Submit result");
    assert_display(5, "Result data", "included");
}

void test_parse_submit_artifact_result_hash() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 1);

    uint8_t accounts[] = {0, 1, 2, 3, 4, 5, 6, SYSTEM_INDEX};
    uint8_t expected_artifact_hash[HASH_SIZE] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    };
    uint8_t data[] = {
        0x27, 0x6c, 0x4a, 0x04, 0x42, 0x7d, 0x9d, 0x07,  // discriminator
        BYTES32_BS58_4,                                   // proof_hash
        0x01,                                             // result_data some
        'a',  'r',  't',  'i',  'f',  'a',  'c',  't',
        ':',  's',  'h',  'a',  '2',  '5',  '6',  ':',
        'A',  'A',  'E',  'C',  'A',  'w',  'Q',  'F',
        'B',  'g',  'c',  'I',  'C',  'Q',  'o',  'L',
        'D',  'A',  '0',  'O',  'D',  'x',  'A',  'R',
        'E',  'h',  'M',  'U',  'F',  'R',  'Y',  'X',
        'G',  'B',  'k',  'a',  'G',  'x',  'w',  'd',
        'H',  'h',  '8',  0,    0,    0,    0,    0,
    };
    Instruction instruction = {
        AGENC_PROGRAM_INDEX,
        accounts,
        ARRAY_LEN(accounts),
        data,
        ARRAY_LEN(data),
    };

    AgencInfo info;
    assert(parse_agenc_instructions(&instruction, &header, &info) == 0);
    assert(info.kind == AgencInstructionSubmitTaskResult);
    assert(info.submit_task_result.has_result_data);
    assert(info.submit_task_result.has_artifact_hash);
    assert(memcmp(&info.submit_task_result.artifact_hash, expected_artifact_hash, HASH_SIZE) == 0);

    transaction_summary_reset();
    assert(print_agenc_info(&info, NULL) == 0);
    assert_summary_count(7);
    assert_display(0, "AgenC action", "Submit result");
    assert_display_title(5, "Artifact SHA-256");
}

void test_parse_accept_task_result_marks_reward_unknown() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 1);

    uint8_t accounts[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, SYSTEM_INDEX};
    uint8_t data[] = {0x59, 0xe6, 0x33, 0x19, 0x00, 0xdb, 0x05, 0x89};
    Instruction instruction = {
        AGENC_PROGRAM_INDEX,
        accounts,
        ARRAY_LEN(accounts),
        data,
        ARRAY_LEN(data),
    };

    AgencInfo info;
    assert(parse_agenc_instructions(&instruction, &header, &info) == 0);
    assert(info.kind == AgencInstructionAcceptTaskResult);
    assert(info.accept_task_result.worker_authority == &pubkeys[9]);

    transaction_summary_reset();
    assert(print_agenc_info(&info, NULL) == 0);
    assert_summary_count(8);
    assert_display(0, "AgenC action", "Accept result");
    assert_display(1, "Reward", "not in tx");
}

void test_parse_reject_task_result() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 1);

    uint8_t accounts[] = {0, 1, 2, 3, 4, 5, 6, 7};
    uint8_t data[] = {
        0x90, 0x07, 0x3a, 0xe8, 0x9d, 0xa7, 0x55, 0xd6,  // discriminator
        BYTES32_BS58_6,                                   // rejection_hash
    };
    Instruction instruction = {
        AGENC_PROGRAM_INDEX,
        accounts,
        ARRAY_LEN(accounts),
        data,
        ARRAY_LEN(data),
    };

    AgencInfo info;
    assert(parse_agenc_instructions(&instruction, &header, &info) == 0);
    assert(info.kind == AgencInstructionRejectTaskResult);
    assert(memcmp(info.reject_task_result.rejection_hash, &data[8], HASH_SIZE) == 0);

    transaction_summary_reset();
    assert(print_agenc_info(&info, NULL) == 0);
    assert_summary_count(8);
    assert_display(0, "AgenC action", "Reject result");
}

void test_parse_cancel_task_with_remaining_worker_accounts() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 1);

    uint8_t accounts[] = {0, 1, 2, 3, SYSTEM_INDEX, 4, 5, 6, 7, 8, 9};
    uint8_t data[] = {0x45, 0xe4, 0x86, 0xbb, 0x86, 0x69, 0xee, 0x30};
    Instruction instruction = {
        AGENC_PROGRAM_INDEX,
        accounts,
        ARRAY_LEN(accounts),
        data,
        ARRAY_LEN(data),
    };

    AgencInfo info;
    assert(parse_agenc_instructions(&instruction, &header, &info) == 0);
    assert(info.kind == AgencInstructionCancelTask);
    assert(info.cancel_task.worker_account_count == 2);

    transaction_summary_reset();
    assert(print_agenc_info(&info, NULL) == 0);
    assert_summary_count(7);
    assert_display(0, "AgenC action", "Cancel task");
    assert_display(1, "Refund", "not in tx");
    assert_display(5, "Worker claims", "2");
}

void test_parse_expire_claim_with_submission() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 1);

    uint8_t accounts[] = {6, 0, 1, 2, 3, 4, 5, 7, 8, SYSTEM_INDEX};
    uint8_t data[] = {0xb0, 0x4e, 0xf1, 0x1d, 0x9f, 0x51, 0x1a, 0x06};
    Instruction instruction = {
        AGENC_PROGRAM_INDEX,
        accounts,
        ARRAY_LEN(accounts),
        data,
        ARRAY_LEN(data),
    };

    AgencInfo info;
    assert(parse_agenc_instructions(&instruction, &header, &info) == 0);
    assert(info.kind == AgencInstructionExpireClaim);
    assert(info.expire_claim.authority == &pubkeys[6]);
    assert(info.expire_claim.task == &pubkeys[0]);
    assert(info.expire_claim.claim == &pubkeys[2]);
    assert(info.expire_claim.worker == &pubkeys[3]);
    assert(info.expire_claim.task_validation_config == &pubkeys[5]);
    assert(info.expire_claim.task_submission == &pubkeys[7]);
    assert(info.expire_claim.rent_recipient == &pubkeys[8]);

    transaction_summary_reset();
    assert(print_agenc_info(&info, NULL) == 0);
    assert_summary_count(9);
    assert_display(0, "AgenC action", "Expire claim");
    assert_display_title(6, "Validation");
    assert_display_title(7, "Submission");
}

void test_process_set_task_job_spec_message() {
    uint8_t accounts[] = {2, 0, 3, 4, 5, 6, SYSTEM_INDEX};
    uint8_t data[] = {
        0x86, 0x66, 0x66, 0x56, 0x1f, 0xa4, 0xca, 0xc1,
        BYTES32_BS58_3,
        0x04, 0x00, 0x00, 0x00,
        'i',  'p',  'f',  's',
    };

    TestMessageBuilder builder = {0};
    build_single_agenc_message(&builder, accounts, ARRAY_LEN(accounts), data, ARRAY_LEN(data));
    assert_process_agenc_message(&builder, 7, "Attach job spec");
    assert_display(4, "Job spec URI", "ipfs");
}

void test_process_claim_task_message() {
    uint8_t accounts[] = {0, 1, 2, 3, 4, 5, SYSTEM_INDEX};
    uint8_t data[] = {0xe6, 0x28, 0x6b, 0x6d, 0xd0, 0xe4, 0xaf, 0x1f};

    TestMessageBuilder builder = {0};
    build_single_agenc_message(&builder, accounts, ARRAY_LEN(accounts), data, ARRAY_LEN(data));
    assert_process_agenc_message(&builder, 7, "Claim task");
}

void test_process_submit_task_result_message() {
    uint8_t accounts[] = {0, 1, 2, 3, 4, 5, 6, SYSTEM_INDEX};
    uint8_t data[] = {
        0x27, 0x6c, 0x4a, 0x04, 0x42, 0x7d, 0x9d, 0x07,
        BYTES32_BS58_4,
        0x01,
        BYTES32_BS58_5,
        BYTES32_BS58_6,
    };

    TestMessageBuilder builder = {0};
    build_single_agenc_message(&builder, accounts, ARRAY_LEN(accounts), data, ARRAY_LEN(data));
    assert_process_agenc_message(&builder, 8, "Submit result");
    assert_display(5, "Result data", "included");
}

void test_process_accept_task_result_message() {
    uint8_t accounts[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, SYSTEM_INDEX};
    uint8_t data[] = {0x59, 0xe6, 0x33, 0x19, 0x00, 0xdb, 0x05, 0x89};

    TestMessageBuilder builder = {0};
    build_single_agenc_message(&builder, accounts, ARRAY_LEN(accounts), data, ARRAY_LEN(data));
    assert_process_agenc_message(&builder, 9, "Accept result");
    assert_display(1, "Reward", "not in tx");
}

void test_process_reject_task_result_message() {
    uint8_t accounts[] = {0, 1, 2, 3, 4, 5, 6, 7};
    uint8_t data[] = {
        0x90, 0x07, 0x3a, 0xe8, 0x9d, 0xa7, 0x55, 0xd6,
        BYTES32_BS58_6,
    };

    TestMessageBuilder builder = {0};
    build_single_agenc_message(&builder, accounts, ARRAY_LEN(accounts), data, ARRAY_LEN(data));
    assert_process_agenc_message(&builder, 9, "Reject result");
}

void test_process_cancel_task_message() {
    uint8_t accounts[] = {0, 1, 2, 3, SYSTEM_INDEX, 4, 5, 6, 7, 8, 9};
    uint8_t data[] = {0x45, 0xe4, 0x86, 0xbb, 0x86, 0x69, 0xee, 0x30};

    TestMessageBuilder builder = {0};
    build_single_agenc_message(&builder, accounts, ARRAY_LEN(accounts), data, ARRAY_LEN(data));
    assert_process_agenc_message(&builder, 8, "Cancel task");
    assert_display(1, "Refund", "not in tx");
    assert_display(5, "Worker claims", "2");
}

void test_process_expire_claim_message() {
    uint8_t accounts[] = {6, 0, 1, 2, 3, 4, 8, SYSTEM_INDEX};
    uint8_t data[] = {0xb0, 0x4e, 0xf1, 0x1d, 0x9f, 0x51, 0x1a, 0x06};

    TestMessageBuilder builder = {0};
    build_single_agenc_message(&builder, accounts, ARRAY_LEN(accounts), data, ARRAY_LEN(data));
    assert_process_agenc_message(&builder, 8, "Expire claim");
}

void test_process_compute_budget_create_task_with_review_message() {
    uint8_t create_data[] = {
        0xc2, 0x50, 0x06, 0xb4, 0xe8, 0x7f, 0x30, 0xab,
        BYTES32_BS58_2,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        't',  'e',  's',  't',  0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,
        0x00, 0xca, 0x9a, 0x3b, 0x00, 0x00, 0x00, 0x00,
        0x01,
        0x00, 0xf1, 0x53, 0x65, 0x00, 0x00, 0x00, 0x00,
        0x00,
        0x00,
        0x05, 0x00,
        0x00,
    };

    TestMessageBuilder builder = {0};
    build_create_review_message(&builder, create_data, ARRAY_LEN(create_data), true);
    assert_process_agenc_message(&builder, 12, "Create task");
    assert_display(1, "Reward", "1 SOL");
    assert_display(6, "Max workers", "1");
    assert_display(9, "Review window", "3600");
    assert_display_title(10, "Max fees");
}

void test_process_lone_create_task_message() {
    uint8_t accounts[] = {0, 1, 2, 3, 4, 6, 6, SYSTEM_INDEX};
    uint8_t data[] = {
        0xc2, 0x50, 0x06, 0xb4, 0xe8, 0x7f, 0x30, 0xab,
        BYTES32_BS58_2,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        't',  'e',  's',  't',  0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,
        0x00, 0xca, 0x9a, 0x3b, 0x00, 0x00, 0x00, 0x00,
        0x01,
        0x00, 0xf1, 0x53, 0x65, 0x00, 0x00, 0x00, 0x00,
        0x00,
        0x00,
        0x05, 0x00,
        0x00,
    };

    TestMessageBuilder builder = {0};
    build_single_agenc_message(&builder, accounts, ARRAY_LEN(accounts), data, ARRAY_LEN(data));
    // A lone create_task (no paired review config) now decodes standalone instead of blind signing.
    assert_process_agenc_message(&builder, 10, "Create task");
    assert_display(6, "Max workers", "1");
}

void test_process_multi_worker_create_task_with_review_message() {
    uint8_t create_data[] = {
        0xc2, 0x50, 0x06, 0xb4, 0xe8, 0x7f, 0x30, 0xab,
        BYTES32_BS58_2,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        't',  'e',  's',  't',  0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,
        0x00, 0xca, 0x9a, 0x3b, 0x00, 0x00, 0x00, 0x00,
        0x02,
        0x00, 0xf1, 0x53, 0x65, 0x00, 0x00, 0x00, 0x00,
        0x00,
        0x00,
        0x05, 0x00,
        0x00,
    };

    TestMessageBuilder builder = {0};
    build_create_review_message(&builder, create_data, ARRAY_LEN(create_data), false);
    // Multi-worker tasks now clear-sign, showing the worker count explicitly.
    assert_process_agenc_message(&builder, 11, "Create task");
    assert_display(6, "Max workers", "2");
}

void test_process_custom_task_type_create_task_with_review_message() {
    uint8_t create_data[] = {
        0xc2, 0x50, 0x06, 0xb4, 0xe8, 0x7f, 0x30, 0xab,
        BYTES32_BS58_2,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        't',  'e',  's',  't',  0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,
        0x00, 0xca, 0x9a, 0x3b, 0x00, 0x00, 0x00, 0x00,
        0x01,                                            // max_workers
        0x00, 0xf1, 0x53, 0x65, 0x00, 0x00, 0x00, 0x00,
        0x02,                                            // task_type (non-default)
        0x00,
        0x05, 0x00,
        0x00,
    };

    TestMessageBuilder builder = {0};
    build_create_review_message(&builder, create_data, ARRAY_LEN(create_data), false);
    // A non-default task_type now clear-signs, showing the type explicitly.
    assert_process_agenc_message(&builder, 12, "Create task");
    assert_display(6, "Max workers", "1");
    assert_display(7, "Task type", "2");
}

void test_reject_create_task_non_commitment_description() {
    // A description whose tail bytes (32..63) are non-zero is not a sha256
    // content commitment, so it must not be clear-signed.
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 2);

    uint8_t accounts[] = {0, 1, 2, 3, 4, 6, 6, SYSTEM_INDEX};
    uint8_t data[] = {
        0xc2, 0x50, 0x06, 0xb4, 0xe8, 0x7f, 0x30, 0xab,  // discriminator
        BYTES32_BS58_2,                                   // task_id
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // required_capabilities
        // 64-byte description with a non-zero byte in the commitment tail
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,     0,    0,    0,    0,
        0,    0,    0,    0xaa,                            // non-zero tail byte
        0x00, 0xca, 0x9a, 0x3b, 0x00, 0x00, 0x00, 0x00,  // reward_amount
        0x01,                                            // max_workers
        0x00, 0xf1, 0x53, 0x65, 0x00, 0x00, 0x00, 0x00,  // deadline
        0x00,                                            // task_type
        0x00,                                            // constraint_hash none
        0x05, 0x00,                                      // min_reputation
        0x00,                                            // reward_mint none
    };
    Instruction instruction = {
        AGENC_PROGRAM_INDEX,
        accounts,
        ARRAY_LEN(accounts),
        data,
        ARRAY_LEN(data),
    };

    AgencInfo info;
    assert(parse_agenc_instructions(&instruction, &header, &info) != 0);
}

void test_reject_unknown_agenc_discriminator_message() {
    uint8_t accounts[] = {0, 1, 2, 3, 4, 5, SYSTEM_INDEX};
    uint8_t data[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    TestMessageBuilder builder = {0};
    build_single_agenc_message(&builder, accounts, ARRAY_LEN(accounts), data, ARRAY_LEN(data));
    assert_reject_message(&builder);
}

void test_reject_unknown_program_id_message() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    memset(&pubkeys[AGENC_PROGRAM_INDEX], 0xaa, PUBKEY_SIZE);

    uint8_t accounts[] = {0, 1, 2, 3, 4, 5, SYSTEM_INDEX};
    uint8_t data[] = {0xe6, 0x28, 0x6b, 0x6d, 0xd0, 0xe4, 0xaf, 0x1f};

    TestMessageBuilder builder = {0};
    builder_append_message_header(&builder, pubkeys, 2, 1);
    builder_append_instruction(&builder,
                               AGENC_PROGRAM_INDEX,
                               accounts,
                               ARRAY_LEN(accounts),
                               data,
                               ARRAY_LEN(data));
    assert_reject_message(&builder);
}

void test_reject_malformed_cancel_task_message() {
    uint8_t accounts[] = {0, 1, 2, 3, SYSTEM_INDEX, 4};
    uint8_t data[] = {0x45, 0xe4, 0x86, 0xbb, 0x86, 0x69, 0xee, 0x30};

    TestMessageBuilder builder = {0};
    build_single_agenc_message(&builder, accounts, ARRAY_LEN(accounts), data, ARRAY_LEN(data));
    assert_reject_message(&builder);
}

void test_reject_malformed_cancel_worker_accounts() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 1);

    uint8_t accounts[] = {0, 1, 2, 3, SYSTEM_INDEX, 4};
    uint8_t data[] = {0x45, 0xe4, 0x86, 0xbb, 0x86, 0x69, 0xee, 0x30};
    Instruction instruction = {
        AGENC_PROGRAM_INDEX,
        accounts,
        ARRAY_LEN(accounts),
        data,
        ARRAY_LEN(data),
    };

    AgencInfo info;
    assert(parse_agenc_instructions(&instruction, &header, &info) != 0);
}

void test_reject_malformed_expire_claim_accounts() {
    Pubkey pubkeys[12];
    init_pubkeys(pubkeys);
    MessageHeader header = test_header(pubkeys, 1);

    uint8_t accounts[] = {6, 0, 1, 2, 3, 4, SYSTEM_INDEX};
    uint8_t data[] = {0xb0, 0x4e, 0xf1, 0x1d, 0x9f, 0x51, 0x1a, 0x06};
    Instruction instruction = {
        AGENC_PROGRAM_INDEX,
        accounts,
        ARRAY_LEN(accounts),
        data,
        ARRAY_LEN(data),
    };

    AgencInfo info;
    assert(parse_agenc_instructions(&instruction, &header, &info) != 0);
}

int main() {
    RUN_TEST(test_is_agenc_program_id);
    RUN_TEST(test_parse_register_agent);
    RUN_TEST(test_process_register_agent_message);
    RUN_TEST(test_parse_create_task_with_review_and_print);
    RUN_TEST(test_process_create_task_with_review_message);
    RUN_TEST(test_parse_set_task_job_spec);
    RUN_TEST(test_process_set_task_job_spec_message);
    RUN_TEST(test_parse_claim_task);
    RUN_TEST(test_process_claim_task_message);
    RUN_TEST(test_parse_submit_task_result);
    RUN_TEST(test_process_submit_task_result_message);
    RUN_TEST(test_parse_accept_task_result_marks_reward_unknown);
    RUN_TEST(test_process_accept_task_result_message);
    RUN_TEST(test_parse_reject_task_result);
    RUN_TEST(test_process_reject_task_result_message);
    RUN_TEST(test_parse_cancel_task_with_remaining_worker_accounts);
    RUN_TEST(test_process_cancel_task_message);
    RUN_TEST(test_parse_expire_claim_with_submission);
    RUN_TEST(test_process_expire_claim_message);
    RUN_TEST(test_process_compute_budget_create_task_with_review_message);
    RUN_TEST(test_process_lone_create_task_message);
    RUN_TEST(test_process_multi_worker_create_task_with_review_message);
    RUN_TEST(test_process_custom_task_type_create_task_with_review_message);
    RUN_TEST(test_reject_create_task_non_commitment_description);
    RUN_TEST(test_reject_unknown_agenc_discriminator_message);
    RUN_TEST(test_reject_unknown_program_id_message);
    RUN_TEST(test_reject_malformed_cancel_task_message);
    RUN_TEST(test_reject_malformed_cancel_worker_accounts);
    RUN_TEST(test_reject_malformed_expire_claim_accounts);

    printf("passed\n");
    return 0;
}
