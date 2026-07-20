#pragma once

#include "sol/parser.h"
#include "sol/print_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Current verified AgenC mainnet program id:
// HJsZ53Zb27b8QMRbQpuDngE44AdwCGxvEZr61Zmxw1xK
#define PROGRAM_ID_AGENC_ARTIFACT \
    242, 79, 6, 99, 64, 38, 134, 189, 153, 254, 99, 205, 191, 250, 117, 81, 77, \
        182, 210, 40, 135, 198, 34, 170, 239, 78, 246, 219, 237, 68, 245, 232

// Current kit mainnet preset program id. Kept as a separate symbol so tests can
// catch future drift between generated artifacts and kit preset wiring.
// HJsZ53Zb27b8QMRbQpuDngE44AdwCGxvEZr61Zmxw1xK
#define PROGRAM_ID_AGENC_MAINNET_PRESET \
    242, 79, 6, 99, 64, 38, 134, 189, 153, 254, 99, 205, 191, 250, 117, 81, 77, \
        182, 210, 40, 135, 198, 34, 170, 239, 78, 246, 219, 237, 68, 245, 232

extern const Pubkey agenc_artifact_program_id;
extern const Pubkey agenc_mainnet_preset_program_id;

typedef enum AgencInstructionKind {
    AgencInstructionUnknown = 0,
    AgencInstructionRegisterAgent,
    AgencInstructionCreateTask,
    AgencInstructionConfigureTaskValidation,
    AgencInstructionSetTaskJobSpec,
    AgencInstructionClaimTaskWithJobSpec,
    AgencInstructionSubmitTaskResult,
    AgencInstructionAcceptTaskResult,
    AgencInstructionRejectTaskResult,
    AgencInstructionCancelTask,
    AgencInstructionExpireClaim,
} AgencInstructionKind;

typedef struct AgencRegisterAgentInfo {
    const Pubkey *agent;
    const Pubkey *protocol_config;
    const Pubkey *authority;
    const Pubkey *system_program;

    const Hash *agent_id;
    uint64_t capabilities;
    SizedString endpoint;
    bool has_metadata_uri;
    SizedString metadata_uri;
    uint64_t stake_amount;
} AgencRegisterAgentInfo;

typedef struct AgencCreateTaskInfo {
    const Pubkey *task;
    const Pubkey *escrow;
    const Pubkey *protocol_config;
    const Pubkey *creator_agent;
    const Pubkey *authority_rate_limit;
    const Pubkey *authority;
    const Pubkey *creator;
    const Pubkey *system_program;

    const Hash *task_id;
    uint64_t required_capabilities;
    // On-chain `description` is a fixed [u8; 64] field. Under the moderation
    // gate the kit fills it with a sha256 content commitment: the 32-byte
    // digest in bytes 0..31 and a zero tail in bytes 32..63 (never prose).
    // `description` points at the full 64-byte field; `description_commitment`
    // aliases the first 32 bytes for hash display.
    const uint8_t *description;
    const Hash *description_commitment;
    uint64_t reward_amount;
    uint8_t max_workers;
    int64_t deadline;
    uint8_t task_type;
    const Hash *constraint_hash;
    uint16_t min_reputation;
    const Pubkey *reward_mint;
} AgencCreateTaskInfo;

typedef struct AgencConfigureTaskValidationInfo {
    const Pubkey *task;
    const Pubkey *task_validation_config;
    const Pubkey *task_attestor_config;
    const Pubkey *protocol_config;
    const Pubkey *hire_record;
    const Pubkey *creator;
    const Pubkey *system_program;

    uint8_t mode;
    int64_t review_window_secs;
    uint8_t validator_quorum;
    const Pubkey *attestor;
} AgencConfigureTaskValidationInfo;

typedef struct AgencSetTaskJobSpecInfo {
    const Pubkey *protocol_config;
    const Pubkey *task;
    const Pubkey *moderation_config;
    const Pubkey *task_moderation;
    const Pubkey *moderation_attestor;
    const Pubkey *moderation_block;
    const Pubkey *task_job_spec;
    const Pubkey *creator;
    const Pubkey *system_program;

    const Hash *job_spec_hash;
    SizedString job_spec_uri;
} AgencSetTaskJobSpecInfo;

typedef struct AgencClaimTaskWithJobSpecInfo {
    const Pubkey *task;
    const Pubkey *task_job_spec;
    const Pubkey *hire_record;
    const Pubkey *legacy_listing;
    const Pubkey *moderation_block;
    const Pubkey *claim;
    const Pubkey *protocol_config;
    const Pubkey *worker;
    const Pubkey *authority;
    const Pubkey *system_program;
} AgencClaimTaskWithJobSpecInfo;

typedef struct AgencSubmitTaskResultInfo {
    const Pubkey *task;
    const Pubkey *claim;
    const Pubkey *task_validation_config;
    const Pubkey *task_submission;
    const Pubkey *protocol_config;
    const Pubkey *worker;
    const Pubkey *authority;
    const Pubkey *system_program;

    const Hash *proof_hash;
    const uint8_t *result_data;
    bool has_result_data;
    Hash artifact_hash;
    bool has_artifact_hash;
} AgencSubmitTaskResultInfo;

typedef struct AgencAcceptTaskResultInfo {
    const Pubkey *task;
    const Pubkey *claim;
    const Pubkey *escrow;
    const Pubkey *task_validation_config;
    const Pubkey *task_submission;
    const Pubkey *worker;
    const Pubkey *protocol_config;
    const Pubkey *treasury;
    const Pubkey *creator;
    const Pubkey *worker_authority;
    const Pubkey *hire_record;
    const Pubkey *operator;
    const Pubkey *referrer;
    const Pubkey *creator_completion_bond;
    const Pubkey *worker_completion_bond;
    const Pubkey *system_program;
} AgencAcceptTaskResultInfo;

typedef struct AgencRejectTaskResultInfo {
    const Pubkey *task;
    const Pubkey *claim;
    const Pubkey *task_validation_config;
    const Pubkey *task_submission;
    const Pubkey *worker;
    const Pubkey *protocol_config;
    const Pubkey *creator;
    const Pubkey *worker_authority;
    const Pubkey *agent_stats;
    const Pubkey *system_program;
    const Pubkey *worker_completion_bond;

    const Hash *rejection_hash;
} AgencRejectTaskResultInfo;

typedef struct AgencCancelTaskInfo {
    const Pubkey *task;
    const Pubkey *escrow;
    const Pubkey *authority;
    const Pubkey *protocol_config;
    const Pubkey *system_program;
    size_t worker_account_count;
} AgencCancelTaskInfo;

typedef struct AgencExpireClaimInfo {
    const Pubkey *authority;
    const Pubkey *task;
    const Pubkey *escrow;
    const Pubkey *claim;
    const Pubkey *worker;
    const Pubkey *protocol_config;
    const Pubkey *task_validation_config;
    const Pubkey *task_submission;
    const Pubkey *rent_recipient;
    const Pubkey *worker_completion_bond;
    const Pubkey *bond_creator;
    const Pubkey *agent_stats;
    const Pubkey *treasury;
    const Pubkey *system_program;
} AgencExpireClaimInfo;

typedef struct AgencInfo {
    AgencInstructionKind kind;
    const Pubkey *program_id;
    union {
        AgencRegisterAgentInfo register_agent;
        AgencCreateTaskInfo create_task;
        AgencConfigureTaskValidationInfo configure_task_validation;
        AgencSetTaskJobSpecInfo set_task_job_spec;
        AgencClaimTaskWithJobSpecInfo claim_task_with_job_spec;
        AgencSubmitTaskResultInfo submit_task_result;
        AgencAcceptTaskResultInfo accept_task_result;
        AgencRejectTaskResultInfo reject_task_result;
        AgencCancelTaskInfo cancel_task;
        AgencExpireClaimInfo expire_claim;
    };
} AgencInfo;

bool is_agenc_program_id(const Pubkey *program_id);

int parse_agenc_instructions(const Instruction *instruction,
                             const MessageHeader *header,
                             AgencInfo *info);

int print_agenc_info(const AgencInfo *info, const PrintConfig *print_config);

int print_agenc_create_task_with_review_info(
    const AgencInfo *create_info,
    const AgencInfo *configure_info,
    const PrintConfig *print_config);
