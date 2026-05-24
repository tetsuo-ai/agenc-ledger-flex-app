#pragma once

#include "sol/parser.h"
#include "sol/print_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Current generated artifact / devnet IDL program id:
// 2jdBSJ8U5ixfwgs1bRLPtRRnpZAPm8Xv1tEdu8yjHJC7
#define PROGRAM_ID_AGENC_ARTIFACT \
    25, 198, 253, 92, 56, 39, 5, 41, 36, 30, 224, 19, 19, 87, 58, 95, 151, 67, \
        209, 18, 167, 80, 190, 159, 192, 143, 129, 236, 249, 241, 163, 200

// Current kit mainnet preset program id:
// HJsZ53Zb27b8QMRbQpuDngE44AdwCGxvEZr61Zmxw1xK
//
// This id is intentionally not enabled by default because the bundled IDL
// artifact does not currently prove that this deployment has the same layout.
#define PROGRAM_ID_AGENC_MAINNET_PRESET \
    242, 79, 6, 99, 64, 38, 134, 189, 153, 254, 99, 205, 191, 250, 117, 81, 77, \
        182, 210, 40, 135, 198, 34, 170, 239, 78, 246, 219, 237, 68, 245, 232

extern const Pubkey agenc_artifact_program_id;
extern const Pubkey agenc_mainnet_preset_program_id;

typedef enum AgencInstructionKind {
    AgencInstructionUnknown = 0,
    AgencInstructionCreateTask,
    AgencInstructionConfigureTaskValidation,
    AgencInstructionSetTaskJobSpec,
    AgencInstructionClaimTaskWithJobSpec,
    AgencInstructionSubmitTaskResult,
    AgencInstructionAcceptTaskResult,
    AgencInstructionRejectTaskResult,
    AgencInstructionCancelTask,
} AgencInstructionKind;

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
    const uint8_t *description;
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
    const Pubkey *task_job_spec;
    const Pubkey *creator;
    const Pubkey *system_program;

    const Hash *job_spec_hash;
    SizedString job_spec_uri;
} AgencSetTaskJobSpecInfo;

typedef struct AgencClaimTaskWithJobSpecInfo {
    const Pubkey *task;
    const Pubkey *task_job_spec;
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

typedef struct AgencInfo {
    AgencInstructionKind kind;
    const Pubkey *program_id;
    union {
        AgencCreateTaskInfo create_task;
        AgencConfigureTaskValidationInfo configure_task_validation;
        AgencSetTaskJobSpecInfo set_task_job_spec;
        AgencClaimTaskWithJobSpecInfo claim_task_with_job_spec;
        AgencSubmitTaskResultInfo submit_task_result;
        AgencAcceptTaskResultInfo accept_task_result;
        AgencRejectTaskResultInfo reject_task_result;
        AgencCancelTaskInfo cancel_task;
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
