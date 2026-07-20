#include "agenc_instruction.h"
#include "sol/transaction_summary.h"
#include "system_instruction.h"
#include "util.h"

#include <string.h>

#define AGENC_DISCRIMINATOR_SIZE          8
#define AGENC_HASH_SIZE                   32
#define AGENC_DESCRIPTION_SIZE            64
#define AGENC_RESULT_DATA_SIZE            64
#define AGENC_ARTIFACT_RESULT_PREFIX      "artifact:sha256:"
#define AGENC_ARTIFACT_RESULT_PREFIX_SIZE 16
#define AGENC_ARTIFACT_RESULT_B64_SIZE    43

#define AGENC_REGISTER_AGENT_ACCOUNTS            4
#define AGENC_CREATE_TASK_ACCOUNTS               13
#define AGENC_CONFIGURE_TASK_VALIDATION_ACCOUNTS 7
#define AGENC_SET_TASK_JOB_SPEC_ACCOUNTS         9
#define AGENC_CLAIM_TASK_WITH_JOB_SPEC_ACCOUNTS  10
#define AGENC_SUBMIT_TASK_RESULT_ACCOUNTS        8
#define AGENC_ACCEPT_TASK_RESULT_ACCOUNTS        21
#define AGENC_REJECT_TASK_RESULT_ACCOUNTS        11
#define AGENC_CANCEL_TASK_BASE_ACCOUNTS          15
#define AGENC_CANCEL_TASK_WORKER_ACCOUNT_STRIDE  3
#define AGENC_EXPIRE_CLAIM_ACCOUNTS              14

const Pubkey agenc_artifact_program_id = {{PROGRAM_ID_AGENC_ARTIFACT}};
const Pubkey agenc_mainnet_preset_program_id = {{PROGRAM_ID_AGENC_MAINNET_PRESET}};

static const uint8_t DISCRIMINATOR_REGISTER_AGENT[AGENC_DISCRIMINATOR_SIZE] =
    {0x87, 0x9d, 0x42, 0xc3, 0x02, 0x71, 0xaf, 0x1e};
static const uint8_t DISCRIMINATOR_CREATE_TASK[AGENC_DISCRIMINATOR_SIZE] =
    {0xc2, 0x50, 0x06, 0xb4, 0xe8, 0x7f, 0x30, 0xab};
static const uint8_t DISCRIMINATOR_CONFIGURE_TASK_VALIDATION[AGENC_DISCRIMINATOR_SIZE] =
    {0x0b, 0x4f, 0x13, 0xbc, 0x0d, 0x20, 0xf4, 0x5a};
static const uint8_t DISCRIMINATOR_SET_TASK_JOB_SPEC[AGENC_DISCRIMINATOR_SIZE] =
    {0x86, 0x66, 0x66, 0x56, 0x1f, 0xa4, 0xca, 0xc1};
static const uint8_t DISCRIMINATOR_CLAIM_TASK_WITH_JOB_SPEC[AGENC_DISCRIMINATOR_SIZE] =
    {0xe6, 0x28, 0x6b, 0x6d, 0xd0, 0xe4, 0xaf, 0x1f};
static const uint8_t DISCRIMINATOR_SUBMIT_TASK_RESULT[AGENC_DISCRIMINATOR_SIZE] =
    {0x27, 0x6c, 0x4a, 0x04, 0x42, 0x7d, 0x9d, 0x07};
static const uint8_t DISCRIMINATOR_ACCEPT_TASK_RESULT[AGENC_DISCRIMINATOR_SIZE] =
    {0x59, 0xe6, 0x33, 0x19, 0x00, 0xdb, 0x05, 0x89};
static const uint8_t DISCRIMINATOR_REJECT_TASK_RESULT[AGENC_DISCRIMINATOR_SIZE] =
    {0x90, 0x07, 0x3a, 0xe8, 0x9d, 0xa7, 0x55, 0xd6};
static const uint8_t DISCRIMINATOR_CANCEL_TASK[AGENC_DISCRIMINATOR_SIZE] =
    {0x45, 0xe4, 0x86, 0xbb, 0x86, 0x69, 0xee, 0x30};
static const uint8_t DISCRIMINATOR_EXPIRE_CLAIM[AGENC_DISCRIMINATOR_SIZE] =
    {0xb0, 0x4e, 0xf1, 0x1d, 0x9f, 0x51, 0x1a, 0x06};

bool is_agenc_program_id(const Pubkey *program_id) {
    if (pubkeys_equal(program_id, &agenc_artifact_program_id)) {
        return true;
    }

    if (pubkeys_equal(program_id, &agenc_mainnet_preset_program_id)) {
        return true;
    }

    return false;
}

static int parse_u16_le(Parser *parser, uint16_t *value) {
    uint8_t lower;
    uint8_t upper;
    BAIL_IF(parse_u8(parser, &lower));
    BAIL_IF(parse_u8(parser, &upper));
    *value = lower + ((uint16_t) upper << 8);
    return 0;
}

static int parse_fixed_bytes(Parser *parser, const uint8_t **value, size_t length) {
    BAIL_IF(parser->buffer_length < length);
    *value = parser->buffer;
    parser->buffer += length;
    parser->buffer_length -= length;
    return 0;
}

static int parse_hash_ref(Parser *parser, const Hash **hash) {
    const uint8_t *bytes;
    BAIL_IF(parse_fixed_bytes(parser, &bytes, AGENC_HASH_SIZE));
    *hash = (const Hash *) bytes;
    return 0;
}

static int parse_optional_hash_ref(Parser *parser, const Hash **hash) {
    enum Option option;
    BAIL_IF(parse_option(parser, &option));
    if (option == OptionNone) {
        *hash = NULL;
        return 0;
    }
    return parse_hash_ref(parser, hash);
}

static int parse_optional_pubkey_ref(Parser *parser, const Pubkey **pubkey) {
    enum Option option;
    BAIL_IF(parse_option(parser, &option));
    if (option == OptionNone) {
        *pubkey = NULL;
        return 0;
    }
    return parse_pubkey(parser, pubkey);
}

static int parse_optional_result_data(Parser *parser,
                                      const uint8_t **result_data,
                                      bool *has_result_data) {
    enum Option option;
    BAIL_IF(parse_option(parser, &option));
    if (option == OptionNone) {
        *result_data = NULL;
        *has_result_data = false;
        return 0;
    }
    *has_result_data = true;
    return parse_fixed_bytes(parser, result_data, AGENC_RESULT_DATA_SIZE);
}

static int base64url_value(uint8_t character) {
    if (character >= 'A' && character <= 'Z') {
        return character - 'A';
    }
    if (character >= 'a' && character <= 'z') {
        return character - 'a' + 26;
    }
    if (character >= '0' && character <= '9') {
        return character - '0' + 52;
    }
    if (character == '-') {
        return 62;
    }
    if (character == '_') {
        return 63;
    }
    return -1;
}

static bool decode_artifact_result_hash(const uint8_t *result_data, Hash *artifact_hash) {
    if (memcmp(result_data, AGENC_ARTIFACT_RESULT_PREFIX, AGENC_ARTIFACT_RESULT_PREFIX_SIZE) != 0) {
        return false;
    }

    size_t text_length = 0;
    while (text_length < AGENC_RESULT_DATA_SIZE && result_data[text_length] != 0) {
        ++text_length;
    }
    if (text_length != AGENC_ARTIFACT_RESULT_PREFIX_SIZE + AGENC_ARTIFACT_RESULT_B64_SIZE) {
        return false;
    }

    uint32_t accumulator = 0;
    uint8_t bits = 0;
    size_t output_length = 0;
    uint8_t *output = (uint8_t *) artifact_hash;
    for (size_t i = AGENC_ARTIFACT_RESULT_PREFIX_SIZE; i < text_length; ++i) {
        int value = base64url_value(result_data[i]);
        if (value < 0) {
            return false;
        }
        accumulator = (accumulator << 6) | (uint32_t) value;
        bits += 6;
        while (bits >= 8) {
            bits -= 8;
            if (output_length >= AGENC_HASH_SIZE) {
                return false;
            }
            output[output_length++] = (uint8_t) ((accumulator >> bits) & 0xff);
            accumulator &= (1u << bits) - 1u;
        }
    }

    return output_length == AGENC_HASH_SIZE && accumulator == 0;
}

static int parse_borsh_string(Parser *parser, SizedString *string) {
    uint32_t length;
    BAIL_IF(parse_u32(parser, &length));
    BAIL_IF(parser->buffer_length < length);
    string->length = length;
    string->string = (const char *) parser->buffer;
    parser->buffer += length;
    parser->buffer_length -= length;
    return 0;
}

static int parse_optional_borsh_string(Parser *parser, SizedString *string, bool *has_string) {
    enum Option option;
    BAIL_IF(parse_option(parser, &option));
    if (option == OptionNone) {
        explicit_bzero(string, sizeof(*string));
        *has_string = false;
        return 0;
    }
    *has_string = true;
    return parse_borsh_string(parser, string);
}

static int account_at(const Instruction *instruction,
                      const MessageHeader *header,
                      uint8_t account_index,
                      const Pubkey **account) {
    BAIL_IF(account_index >= instruction->accounts_length);
    uint8_t pubkey_index = instruction->accounts[account_index];
    BAIL_IF(pubkey_index >= header->pubkeys_header.pubkeys_length);
    *account = &header->pubkeys[pubkey_index];
    return 0;
}

static int validate_system_program(const Pubkey *pubkey) {
    BAIL_IF(!pubkeys_equal(pubkey, &system_program_id));
    return 0;
}

static int parse_register_agent(Parser *parser,
                                const Instruction *instruction,
                                const MessageHeader *header,
                                AgencRegisterAgentInfo *info) {
    BAIL_IF(instruction->accounts_length != AGENC_REGISTER_AGENT_ACCOUNTS);
    BAIL_IF(account_at(instruction, header, 0, &info->agent));
    BAIL_IF(account_at(instruction, header, 1, &info->protocol_config));
    BAIL_IF(account_at(instruction, header, 2, &info->authority));
    BAIL_IF(account_at(instruction, header, 3, &info->system_program));
    BAIL_IF(validate_system_program(info->system_program));

    BAIL_IF(parse_hash_ref(parser, &info->agent_id));
    BAIL_IF(parse_u64(parser, &info->capabilities));
    BAIL_IF(parse_borsh_string(parser, &info->endpoint));
    BAIL_IF(parse_optional_borsh_string(parser, &info->metadata_uri, &info->has_metadata_uri));
    BAIL_IF(parse_u64(parser, &info->stake_amount));
    return 0;
}

static int parse_create_task(Parser *parser,
                             const Instruction *instruction,
                             const MessageHeader *header,
                             AgencCreateTaskInfo *info) {
    BAIL_IF(instruction->accounts_length != AGENC_CREATE_TASK_ACCOUNTS);
    BAIL_IF(account_at(instruction, header, 0, &info->task));
    BAIL_IF(account_at(instruction, header, 1, &info->escrow));
    BAIL_IF(account_at(instruction, header, 2, &info->protocol_config));
    BAIL_IF(account_at(instruction, header, 3, &info->creator_agent));
    BAIL_IF(account_at(instruction, header, 4, &info->authority_rate_limit));
    BAIL_IF(account_at(instruction, header, 5, &info->authority));
    BAIL_IF(account_at(instruction, header, 6, &info->creator));
    BAIL_IF(account_at(instruction, header, 7, &info->system_program));
    BAIL_IF(validate_system_program(info->system_program));

    BAIL_IF(parse_hash_ref(parser, &info->task_id));
    BAIL_IF(parse_u64(parser, &info->required_capabilities));
    BAIL_IF(parse_fixed_bytes(parser, &info->description, AGENC_DESCRIPTION_SIZE));
    // The moderation gate stores a sha256 content commitment in `description`:
    // the 32-byte digest in bytes 0..31, the remaining 32 bytes zero. Refuse to
    // clear-sign anything that does not match that layout (e.g. raw prose) so the
    // device only ever attests to a moderated content commitment.
    for (size_t i = AGENC_HASH_SIZE; i < AGENC_DESCRIPTION_SIZE; i++) {
        BAIL_IF(info->description[i] != 0);
    }
    info->description_commitment = (const Hash *) info->description;
    BAIL_IF(parse_u64(parser, &info->reward_amount));
    BAIL_IF(parse_u8(parser, &info->max_workers));
    BAIL_IF(parse_i64(parser, &info->deadline));
    BAIL_IF(parse_u8(parser, &info->task_type));
    BAIL_IF(parse_optional_hash_ref(parser, &info->constraint_hash));
    BAIL_IF(parse_u16_le(parser, &info->min_reputation));
    BAIL_IF(parse_optional_pubkey_ref(parser, &info->reward_mint));
    return 0;
}

static int parse_configure_task_validation(Parser *parser,
                                           const Instruction *instruction,
                                           const MessageHeader *header,
                                           AgencConfigureTaskValidationInfo *info) {
    BAIL_IF(instruction->accounts_length != AGENC_CONFIGURE_TASK_VALIDATION_ACCOUNTS);
    BAIL_IF(account_at(instruction, header, 0, &info->task));
    BAIL_IF(account_at(instruction, header, 1, &info->task_validation_config));
    BAIL_IF(account_at(instruction, header, 2, &info->task_attestor_config));
    BAIL_IF(account_at(instruction, header, 3, &info->protocol_config));
    BAIL_IF(account_at(instruction, header, 4, &info->hire_record));
    BAIL_IF(account_at(instruction, header, 5, &info->creator));
    BAIL_IF(account_at(instruction, header, 6, &info->system_program));
    BAIL_IF(validate_system_program(info->system_program));

    BAIL_IF(parse_u8(parser, &info->mode));
    BAIL_IF(parse_i64(parser, &info->review_window_secs));
    BAIL_IF(parse_u8(parser, &info->validator_quorum));
    BAIL_IF(parse_optional_pubkey_ref(parser, &info->attestor));
    return 0;
}

static int parse_set_task_job_spec(Parser *parser,
                                   const Instruction *instruction,
                                   const MessageHeader *header,
                                   AgencSetTaskJobSpecInfo *info) {
    BAIL_IF(instruction->accounts_length != AGENC_SET_TASK_JOB_SPEC_ACCOUNTS);
    BAIL_IF(account_at(instruction, header, 0, &info->protocol_config));
    BAIL_IF(account_at(instruction, header, 1, &info->task));
    BAIL_IF(account_at(instruction, header, 2, &info->moderation_config));
    BAIL_IF(account_at(instruction, header, 3, &info->task_moderation));
    BAIL_IF(account_at(instruction, header, 4, &info->moderation_attestor));
    BAIL_IF(account_at(instruction, header, 5, &info->moderation_block));
    BAIL_IF(account_at(instruction, header, 6, &info->task_job_spec));
    BAIL_IF(account_at(instruction, header, 7, &info->creator));
    BAIL_IF(account_at(instruction, header, 8, &info->system_program));
    BAIL_IF(validate_system_program(info->system_program));

    BAIL_IF(parse_hash_ref(parser, &info->job_spec_hash));
    BAIL_IF(parse_borsh_string(parser, &info->job_spec_uri));
    return 0;
}

static int parse_claim_task_with_job_spec(const Instruction *instruction,
                                          const MessageHeader *header,
                                          AgencClaimTaskWithJobSpecInfo *info) {
    BAIL_IF(instruction->accounts_length != AGENC_CLAIM_TASK_WITH_JOB_SPEC_ACCOUNTS);
    BAIL_IF(account_at(instruction, header, 0, &info->task));
    BAIL_IF(account_at(instruction, header, 1, &info->task_job_spec));
    BAIL_IF(account_at(instruction, header, 2, &info->hire_record));
    BAIL_IF(account_at(instruction, header, 3, &info->legacy_listing));
    BAIL_IF(account_at(instruction, header, 4, &info->moderation_block));
    BAIL_IF(account_at(instruction, header, 5, &info->claim));
    BAIL_IF(account_at(instruction, header, 6, &info->protocol_config));
    BAIL_IF(account_at(instruction, header, 7, &info->worker));
    BAIL_IF(account_at(instruction, header, 8, &info->authority));
    BAIL_IF(account_at(instruction, header, 9, &info->system_program));
    BAIL_IF(validate_system_program(info->system_program));
    return 0;
}

static int parse_submit_task_result(Parser *parser,
                                    const Instruction *instruction,
                                    const MessageHeader *header,
                                    AgencSubmitTaskResultInfo *info) {
    BAIL_IF(instruction->accounts_length != AGENC_SUBMIT_TASK_RESULT_ACCOUNTS);
    BAIL_IF(account_at(instruction, header, 0, &info->task));
    BAIL_IF(account_at(instruction, header, 1, &info->claim));
    BAIL_IF(account_at(instruction, header, 2, &info->task_validation_config));
    BAIL_IF(account_at(instruction, header, 3, &info->task_submission));
    BAIL_IF(account_at(instruction, header, 4, &info->protocol_config));
    BAIL_IF(account_at(instruction, header, 5, &info->worker));
    BAIL_IF(account_at(instruction, header, 6, &info->authority));
    BAIL_IF(account_at(instruction, header, 7, &info->system_program));
    BAIL_IF(validate_system_program(info->system_program));

    BAIL_IF(parse_hash_ref(parser, &info->proof_hash));
    BAIL_IF(parse_optional_result_data(parser, &info->result_data, &info->has_result_data));
    if (info->has_result_data) {
        info->has_artifact_hash = decode_artifact_result_hash(info->result_data,
                                                              &info->artifact_hash);
    }
    return 0;
}

static int parse_accept_task_result(const Instruction *instruction,
                                    const MessageHeader *header,
                                    AgencAcceptTaskResultInfo *info) {
    BAIL_IF(instruction->accounts_length != AGENC_ACCEPT_TASK_RESULT_ACCOUNTS);
    BAIL_IF(account_at(instruction, header, 0, &info->task));
    BAIL_IF(account_at(instruction, header, 1, &info->claim));
    BAIL_IF(account_at(instruction, header, 2, &info->escrow));
    BAIL_IF(account_at(instruction, header, 3, &info->task_validation_config));
    BAIL_IF(account_at(instruction, header, 4, &info->task_submission));
    BAIL_IF(account_at(instruction, header, 5, &info->worker));
    BAIL_IF(account_at(instruction, header, 6, &info->protocol_config));
    BAIL_IF(account_at(instruction, header, 7, &info->treasury));
    BAIL_IF(account_at(instruction, header, 8, &info->creator));
    BAIL_IF(account_at(instruction, header, 9, &info->worker_authority));
    BAIL_IF(account_at(instruction, header, 10, &info->hire_record));
    BAIL_IF(account_at(instruction, header, 11, &info->operator));
    BAIL_IF(account_at(instruction, header, 12, &info->referrer));
    BAIL_IF(account_at(instruction, header, 13, &info->creator_completion_bond));
    BAIL_IF(account_at(instruction, header, 14, &info->worker_completion_bond));
    BAIL_IF(account_at(instruction, header, 20, &info->system_program));
    BAIL_IF(validate_system_program(info->system_program));
    return 0;
}

static int parse_reject_task_result(Parser *parser,
                                    const Instruction *instruction,
                                    const MessageHeader *header,
                                    AgencRejectTaskResultInfo *info) {
    BAIL_IF(instruction->accounts_length != AGENC_REJECT_TASK_RESULT_ACCOUNTS);
    BAIL_IF(account_at(instruction, header, 0, &info->task));
    BAIL_IF(account_at(instruction, header, 1, &info->claim));
    BAIL_IF(account_at(instruction, header, 2, &info->task_validation_config));
    BAIL_IF(account_at(instruction, header, 3, &info->task_submission));
    BAIL_IF(account_at(instruction, header, 4, &info->worker));
    BAIL_IF(account_at(instruction, header, 5, &info->protocol_config));
    BAIL_IF(account_at(instruction, header, 6, &info->creator));
    BAIL_IF(account_at(instruction, header, 7, &info->worker_authority));
    BAIL_IF(account_at(instruction, header, 8, &info->agent_stats));
    BAIL_IF(account_at(instruction, header, 9, &info->system_program));
    BAIL_IF(account_at(instruction, header, 10, &info->worker_completion_bond));
    BAIL_IF(validate_system_program(info->system_program));

    BAIL_IF(parse_hash_ref(parser, &info->rejection_hash));
    return 0;
}

static int parse_cancel_task(const Instruction *instruction,
                             const MessageHeader *header,
                             AgencCancelTaskInfo *info) {
    BAIL_IF(instruction->accounts_length < AGENC_CANCEL_TASK_BASE_ACCOUNTS);
    // Clear-sign only the independent, non-bid one-shot shape (zero or one
    // worker triple). Larger suffixes are ambiguous without reading Task state:
    // a BidExclusive suffix can otherwise be mislabeled as worker claims.
    BAIL_IF(instruction->accounts_length >
            AGENC_CANCEL_TASK_BASE_ACCOUNTS + AGENC_CANCEL_TASK_WORKER_ACCOUNT_STRIDE);
    BAIL_IF(((instruction->accounts_length - AGENC_CANCEL_TASK_BASE_ACCOUNTS) %
             AGENC_CANCEL_TASK_WORKER_ACCOUNT_STRIDE) != 0);
    BAIL_IF(account_at(instruction, header, 0, &info->task));
    BAIL_IF(account_at(instruction, header, 1, &info->escrow));
    BAIL_IF(account_at(instruction, header, 2, &info->authority));
    BAIL_IF(account_at(instruction, header, 3, &info->protocol_config));
    BAIL_IF(account_at(instruction, header, 4, &info->system_program));
    BAIL_IF(validate_system_program(info->system_program));
    info->worker_account_count = (instruction->accounts_length - AGENC_CANCEL_TASK_BASE_ACCOUNTS) /
                                 AGENC_CANCEL_TASK_WORKER_ACCOUNT_STRIDE;
    return 0;
}

static int parse_expire_claim(const Instruction *instruction,
                              const MessageHeader *header,
                              AgencExpireClaimInfo *info) {
    // Parent/bid remaining-account suffixes require task-state context and are
    // deliberately left to blind signing instead of being mislabeled.
    BAIL_IF(instruction->accounts_length != AGENC_EXPIRE_CLAIM_ACCOUNTS);
    BAIL_IF(account_at(instruction, header, 0, &info->authority));
    BAIL_IF(account_at(instruction, header, 1, &info->task));
    BAIL_IF(account_at(instruction, header, 2, &info->escrow));
    BAIL_IF(account_at(instruction, header, 3, &info->claim));
    BAIL_IF(account_at(instruction, header, 4, &info->worker));
    BAIL_IF(account_at(instruction, header, 5, &info->protocol_config));

    BAIL_IF(account_at(instruction, header, 6, &info->task_validation_config));
    BAIL_IF(account_at(instruction, header, 7, &info->task_submission));
    BAIL_IF(account_at(instruction, header, 8, &info->rent_recipient));
    BAIL_IF(account_at(instruction, header, 9, &info->worker_completion_bond));
    BAIL_IF(account_at(instruction, header, 10, &info->bond_creator));
    BAIL_IF(account_at(instruction, header, 11, &info->agent_stats));
    BAIL_IF(account_at(instruction, header, 12, &info->treasury));
    BAIL_IF(account_at(instruction, header, 13, &info->system_program));
    BAIL_IF(validate_system_program(info->system_program));
    return 0;
}

int parse_agenc_instructions(const Instruction *instruction,
                             const MessageHeader *header,
                             AgencInfo *info) {
    const Pubkey *program_id = &header->pubkeys[instruction->program_id_index];
    BAIL_IF(!is_agenc_program_id(program_id));

    explicit_bzero(info, sizeof(*info));
    info->program_id = program_id;

    Parser parser = {instruction->data, instruction->data_length};
    const uint8_t *discriminator;
    BAIL_IF(parse_fixed_bytes(&parser, &discriminator, AGENC_DISCRIMINATOR_SIZE));

    if (memcmp(discriminator, DISCRIMINATOR_REGISTER_AGENT, AGENC_DISCRIMINATOR_SIZE) == 0) {
        info->kind = AgencInstructionRegisterAgent;
        BAIL_IF(parse_register_agent(&parser, instruction, header, &info->register_agent));
    } else if (memcmp(discriminator, DISCRIMINATOR_CREATE_TASK, AGENC_DISCRIMINATOR_SIZE) == 0) {
        info->kind = AgencInstructionCreateTask;
        BAIL_IF(parse_create_task(&parser, instruction, header, &info->create_task));
    } else if (memcmp(discriminator,
                      DISCRIMINATOR_CONFIGURE_TASK_VALIDATION,
                      AGENC_DISCRIMINATOR_SIZE) == 0) {
        info->kind = AgencInstructionConfigureTaskValidation;
        BAIL_IF(parse_configure_task_validation(&parser,
                                                instruction,
                                                header,
                                                &info->configure_task_validation));
    } else if (memcmp(discriminator, DISCRIMINATOR_SET_TASK_JOB_SPEC, AGENC_DISCRIMINATOR_SIZE) ==
               0) {
        info->kind = AgencInstructionSetTaskJobSpec;
        BAIL_IF(parse_set_task_job_spec(&parser, instruction, header, &info->set_task_job_spec));
    } else if (memcmp(discriminator,
                      DISCRIMINATOR_CLAIM_TASK_WITH_JOB_SPEC,
                      AGENC_DISCRIMINATOR_SIZE) == 0) {
        info->kind = AgencInstructionClaimTaskWithJobSpec;
        BAIL_IF(
            parse_claim_task_with_job_spec(instruction, header, &info->claim_task_with_job_spec));
    } else if (memcmp(discriminator, DISCRIMINATOR_SUBMIT_TASK_RESULT, AGENC_DISCRIMINATOR_SIZE) ==
               0) {
        info->kind = AgencInstructionSubmitTaskResult;
        BAIL_IF(parse_submit_task_result(&parser, instruction, header, &info->submit_task_result));
    } else if (memcmp(discriminator, DISCRIMINATOR_ACCEPT_TASK_RESULT, AGENC_DISCRIMINATOR_SIZE) ==
               0) {
        info->kind = AgencInstructionAcceptTaskResult;
        BAIL_IF(parse_accept_task_result(instruction, header, &info->accept_task_result));
    } else if (memcmp(discriminator, DISCRIMINATOR_REJECT_TASK_RESULT, AGENC_DISCRIMINATOR_SIZE) ==
               0) {
        info->kind = AgencInstructionRejectTaskResult;
        BAIL_IF(parse_reject_task_result(&parser, instruction, header, &info->reject_task_result));
    } else if (memcmp(discriminator, DISCRIMINATOR_CANCEL_TASK, AGENC_DISCRIMINATOR_SIZE) == 0) {
        info->kind = AgencInstructionCancelTask;
        BAIL_IF(parse_cancel_task(instruction, header, &info->cancel_task));
    } else if (memcmp(discriminator, DISCRIMINATOR_EXPIRE_CLAIM, AGENC_DISCRIMINATOR_SIZE) == 0) {
        info->kind = AgencInstructionExpireClaim;
        BAIL_IF(parse_expire_claim(instruction, header, &info->expire_claim));
    } else {
        return 1;
    }

    BAIL_IF(!parser_is_empty(&parser));
    return 0;
}

static int set_primary_action(const char *action) {
    SummaryItem *item = transaction_summary_primary_item();
    BAIL_IF(item == NULL);
    summary_item_set_string(item, "AgenC action", action);
    transaction_summary_set_transaction_type(TRANSACTION_TYPE_AGENC_ACTION);
    return 0;
}

static int set_general_pubkey(const char *title, const Pubkey *pubkey) {
    SummaryItem *item = transaction_summary_general_item();
    BAIL_IF(item == NULL);
    summary_item_set_pubkey(item, title, pubkey);
    return 0;
}

static int set_general_hash(const char *title, const Hash *hash) {
    SummaryItem *item = transaction_summary_general_item();
    BAIL_IF(item == NULL);
    summary_item_set_hash(item, title, hash);
    return 0;
}

static int set_general_amount(const char *title, uint64_t lamports) {
    SummaryItem *item = transaction_summary_general_item();
    BAIL_IF(item == NULL);
    summary_item_set_amount(item, title, lamports);
    return 0;
}

static int set_general_u64(const char *title, uint64_t value) {
    SummaryItem *item = transaction_summary_general_item();
    BAIL_IF(item == NULL);
    summary_item_set_u64(item, title, value);
    return 0;
}

static int set_general_i64(const char *title, int64_t value) {
    SummaryItem *item = transaction_summary_general_item();
    BAIL_IF(item == NULL);
    summary_item_set_i64(item, title, value);
    return 0;
}

static int set_general_timestamp(const char *title, int64_t value) {
    SummaryItem *item = transaction_summary_general_item();
    BAIL_IF(item == NULL);
    summary_item_set_timestamp(item, title, value);
    return 0;
}

static int set_general_string(const char *title, const char *value) {
    SummaryItem *item = transaction_summary_general_item();
    BAIL_IF(item == NULL);
    summary_item_set_string(item, title, value);
    return 0;
}

static int set_general_sized_string(const char *title, const SizedString *value) {
    SummaryItem *item = transaction_summary_general_item();
    BAIL_IF(item == NULL);
    summary_item_set_sized_string(item, title, value);
    return 0;
}

static int print_create_task_fields(const AgencInfo *agenc_info) {
    const AgencCreateTaskInfo *info = &agenc_info->create_task;

    if (info->reward_mint != NULL) {
        // SPL-token reward. The on-chain amount is denominated in the token's base
        // units and the device cannot resolve the mint's decimals offline, so show
        // the raw amount AND the mint pubkey so the signer verifies both explicitly
        // rather than seeing a misleading SOL-formatted value.
        BAIL_IF(set_general_u64("Reward (token base units)", info->reward_amount));
        BAIL_IF(set_general_pubkey("Token mint", info->reward_mint));
    } else {
        BAIL_IF(set_general_amount("Reward", info->reward_amount));
    }
    BAIL_IF(set_general_pubkey("Task", info->task));
    BAIL_IF(set_general_pubkey("Creator", info->creator));
    BAIL_IF(set_general_hash("Content hash", info->description_commitment));
    BAIL_IF(set_general_timestamp("Deadline", info->deadline));
    BAIL_IF(set_general_u64("Max workers", info->max_workers));
    // task_type 0 is the standard task; surface any non-default type explicitly.
    if (info->task_type != 0) {
        BAIL_IF(set_general_u64("Task type", info->task_type));
    }
    BAIL_IF(set_general_u64("Min reputation", info->min_reputation));
    BAIL_IF(set_general_pubkey("Program", agenc_info->program_id));
    return 0;
}

// Standalone create_task display (no paired review-config instruction). The kit
// normally pairs create_task + configure_task_validation (rendered by
// print_agenc_create_task_with_review_info), but a lone create_task is a valid
// instruction; decoding it here is strictly safer than forcing blind signing.
static int print_agenc_create_task_info(const AgencInfo *agenc_info,
                                        const PrintConfig *print_config) {
    UNUSED(print_config);
    BAIL_IF(set_primary_action("Create task"));
    BAIL_IF(print_create_task_fields(agenc_info));
    return 0;
}

static int print_agenc_register_agent_info(const AgencInfo *agenc_info,
                                           const PrintConfig *print_config) {
    UNUSED(print_config);
    const AgencRegisterAgentInfo *info = &agenc_info->register_agent;
    BAIL_IF(set_primary_action("Register agent"));
    BAIL_IF(set_general_amount("Stake", info->stake_amount));
    BAIL_IF(set_general_pubkey("Agent", info->agent));
    BAIL_IF(set_general_pubkey("Authority", info->authority));
    BAIL_IF(set_general_u64("Capabilities", info->capabilities));
    BAIL_IF(set_general_sized_string("Endpoint", &info->endpoint));
    if (info->has_metadata_uri) {
        BAIL_IF(set_general_sized_string("Metadata URI", &info->metadata_uri));
    }
    BAIL_IF(set_general_pubkey("Program", agenc_info->program_id));
    return 0;
}

int print_agenc_create_task_with_review_info(const AgencInfo *create_info,
                                             const AgencInfo *configure_info,
                                             const PrintConfig *print_config) {
    UNUSED(print_config);
    BAIL_IF(create_info->kind != AgencInstructionCreateTask);
    BAIL_IF(configure_info->kind != AgencInstructionConfigureTaskValidation);

    const AgencCreateTaskInfo *create_task = &create_info->create_task;
    const AgencConfigureTaskValidationInfo *configure = &configure_info->configure_task_validation;

    BAIL_IF(!pubkeys_equal(create_task->task, configure->task));
    BAIL_IF(!pubkeys_equal(create_task->creator, configure->creator));
    BAIL_IF(configure->mode != 1);
    BAIL_IF(configure->validator_quorum != 0);
    BAIL_IF(configure->attestor != NULL);

    BAIL_IF(set_primary_action("Create task"));
    BAIL_IF(print_create_task_fields(create_info));
    BAIL_IF(set_general_i64("Review window", configure->review_window_secs));
    return 0;
}

static int print_agenc_configure_task_validation_info(const AgencInfo *agenc_info,
                                                      const PrintConfig *print_config) {
    UNUSED(print_config);
    const AgencConfigureTaskValidationInfo *info = &agenc_info->configure_task_validation;
    BAIL_IF(set_primary_action("Configure review"));
    BAIL_IF(set_general_pubkey("Task", info->task));
    BAIL_IF(set_general_u64("Mode", info->mode));
    BAIL_IF(set_general_i64("Review window", info->review_window_secs));
    BAIL_IF(set_general_u64("Validator quorum", info->validator_quorum));
    if (info->attestor != NULL) {
        BAIL_IF(set_general_pubkey("Attestor", info->attestor));
    }
    BAIL_IF(set_general_pubkey("Program", agenc_info->program_id));
    return 0;
}

static int print_agenc_set_task_job_spec_info(const AgencInfo *agenc_info,
                                              const PrintConfig *print_config) {
    UNUSED(print_config);
    const AgencSetTaskJobSpecInfo *info = &agenc_info->set_task_job_spec;
    BAIL_IF(set_primary_action("Attach job spec"));
    BAIL_IF(set_general_hash("Job spec hash", info->job_spec_hash));
    BAIL_IF(set_general_pubkey("Task", info->task));
    BAIL_IF(set_general_pubkey("Creator", info->creator));
    BAIL_IF(set_general_sized_string("Job spec URI", &info->job_spec_uri));
    BAIL_IF(set_general_pubkey("Program", agenc_info->program_id));
    return 0;
}

static int print_agenc_claim_task_with_job_spec_info(const AgencInfo *agenc_info,
                                                     const PrintConfig *print_config) {
    UNUSED(print_config);
    const AgencClaimTaskWithJobSpecInfo *info = &agenc_info->claim_task_with_job_spec;
    BAIL_IF(set_primary_action("Claim task"));
    BAIL_IF(set_general_pubkey("Task", info->task));
    BAIL_IF(set_general_pubkey("Worker", info->worker));
    BAIL_IF(set_general_pubkey("Authority", info->authority));
    BAIL_IF(set_general_pubkey("Claim", info->claim));
    BAIL_IF(set_general_pubkey("Program", agenc_info->program_id));
    return 0;
}

static int print_agenc_submit_task_result_info(const AgencInfo *agenc_info,
                                               const PrintConfig *print_config) {
    UNUSED(print_config);
    const AgencSubmitTaskResultInfo *info = &agenc_info->submit_task_result;
    BAIL_IF(set_primary_action("Submit result"));
    BAIL_IF(set_general_hash("Proof hash", info->proof_hash));
    BAIL_IF(set_general_pubkey("Task", info->task));
    BAIL_IF(set_general_pubkey("Worker", info->worker));
    BAIL_IF(set_general_pubkey("Submission", info->task_submission));
    if (info->has_artifact_hash) {
        BAIL_IF(set_general_hash("Artifact SHA-256", &info->artifact_hash));
    } else {
        BAIL_IF(set_general_string("Result data", info->has_result_data ? "included" : "none"));
    }
    BAIL_IF(set_general_pubkey("Program", agenc_info->program_id));
    return 0;
}

static int print_agenc_accept_task_result_info(const AgencInfo *agenc_info,
                                               const PrintConfig *print_config) {
    UNUSED(print_config);
    const AgencAcceptTaskResultInfo *info = &agenc_info->accept_task_result;
    BAIL_IF(set_primary_action("Accept result"));
    BAIL_IF(set_general_string("Reward", "not in tx"));
    BAIL_IF(set_general_pubkey("Task", info->task));
    BAIL_IF(set_general_pubkey("Worker", info->worker));
    BAIL_IF(set_general_pubkey("Worker auth", info->worker_authority));
    BAIL_IF(set_general_pubkey("Creator", info->creator));
    BAIL_IF(set_general_pubkey("Treasury", info->treasury));
    BAIL_IF(set_general_pubkey("Program", agenc_info->program_id));
    return 0;
}

static int print_agenc_reject_task_result_info(const AgencInfo *agenc_info,
                                               const PrintConfig *print_config) {
    UNUSED(print_config);
    const AgencRejectTaskResultInfo *info = &agenc_info->reject_task_result;
    BAIL_IF(set_primary_action("Reject result"));
    BAIL_IF(set_general_hash("Rejection hash", info->rejection_hash));
    BAIL_IF(set_general_pubkey("Task", info->task));
    BAIL_IF(set_general_pubkey("Worker", info->worker));
    BAIL_IF(set_general_pubkey("Worker auth", info->worker_authority));
    BAIL_IF(set_general_pubkey("Creator", info->creator));
    BAIL_IF(set_general_pubkey("Submission", info->task_submission));
    BAIL_IF(set_general_pubkey("Program", agenc_info->program_id));
    return 0;
}

static int print_agenc_cancel_task_info(const AgencInfo *agenc_info,
                                        const PrintConfig *print_config) {
    UNUSED(print_config);
    const AgencCancelTaskInfo *info = &agenc_info->cancel_task;
    BAIL_IF(set_primary_action("Cancel task"));
    BAIL_IF(set_general_string("Refund", "not in tx"));
    BAIL_IF(set_general_pubkey("Task", info->task));
    BAIL_IF(set_general_pubkey("Authority", info->authority));
    BAIL_IF(set_general_pubkey("Escrow", info->escrow));
    BAIL_IF(set_general_u64("Worker claims", info->worker_account_count));
    BAIL_IF(set_general_pubkey("Program", agenc_info->program_id));
    return 0;
}

static int print_agenc_expire_claim_info(const AgencInfo *agenc_info,
                                         const PrintConfig *print_config) {
    UNUSED(print_config);
    const AgencExpireClaimInfo *info = &agenc_info->expire_claim;
    BAIL_IF(set_primary_action("Expire claim"));
    BAIL_IF(set_general_pubkey("Task", info->task));
    BAIL_IF(set_general_pubkey("Claim", info->claim));
    BAIL_IF(set_general_pubkey("Worker", info->worker));
    BAIL_IF(set_general_pubkey("Authority", info->authority));
    BAIL_IF(set_general_pubkey("Rent recipient", info->rent_recipient));
    if (info->task_validation_config != NULL) {
        BAIL_IF(set_general_pubkey("Validation", info->task_validation_config));
    }
    if (info->task_submission != NULL) {
        BAIL_IF(set_general_pubkey("Submission", info->task_submission));
    }
    BAIL_IF(set_general_pubkey("Program", agenc_info->program_id));
    return 0;
}

int print_agenc_info(const AgencInfo *info, const PrintConfig *print_config) {
    switch (info->kind) {
        case AgencInstructionRegisterAgent:
            return print_agenc_register_agent_info(info, print_config);
        case AgencInstructionCreateTask:
            // Paired create_task + configure_task_validation is rendered upstream by
            // print_agenc_create_task_with_review_info (with the review window). A lone
            // create_task still decodes here instead of falling back to blind signing.
            return print_agenc_create_task_info(info, print_config);
        case AgencInstructionConfigureTaskValidation:
            return print_agenc_configure_task_validation_info(info, print_config);
        case AgencInstructionSetTaskJobSpec:
            return print_agenc_set_task_job_spec_info(info, print_config);
        case AgencInstructionClaimTaskWithJobSpec:
            return print_agenc_claim_task_with_job_spec_info(info, print_config);
        case AgencInstructionSubmitTaskResult:
            return print_agenc_submit_task_result_info(info, print_config);
        case AgencInstructionAcceptTaskResult:
            return print_agenc_accept_task_result_info(info, print_config);
        case AgencInstructionRejectTaskResult:
            return print_agenc_reject_task_result_info(info, print_config);
        case AgencInstructionCancelTask:
            return print_agenc_cancel_task_info(info, print_config);
        case AgencInstructionExpireClaim:
            return print_agenc_expire_claim_info(info, print_config);
        case AgencInstructionUnknown:
        default:
            return -1;
    }
}
