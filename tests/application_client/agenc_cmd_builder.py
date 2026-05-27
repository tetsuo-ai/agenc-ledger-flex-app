from typing import List

import base58

from application_client.solana_cmd_builder import (
    FAKE_RECENT_BLOCKHASH_BYTES,
    PROGRAM_ID_COMPUTE_BUDGET,
    PROGRAM_ID_SYSTEM,
)


PROGRAM_ID_AGENC_ARTIFACT = "HJsZ53Zb27b8QMRbQpuDngE44AdwCGxvEZr61Zmxw1xK"

DISCRIMINATOR_CREATE_TASK = bytes.fromhex("c25006b4e87f30ab")
DISCRIMINATOR_CONFIGURE_TASK_VALIDATION = bytes.fromhex("0b4f13bc0d20f45a")
DISCRIMINATOR_SET_TASK_JOB_SPEC = bytes.fromhex("866666561fa4cac1")
DISCRIMINATOR_CLAIM_TASK_WITH_JOB_SPEC = bytes.fromhex("e6286b6dd0e4af1f")
DISCRIMINATOR_SUBMIT_TASK_RESULT = bytes.fromhex("276c4a04427d9d07")
DISCRIMINATOR_ACCEPT_TASK_RESULT = bytes.fromhex("59e6331900db0589")
DISCRIMINATOR_REJECT_TASK_RESULT = bytes.fromhex("90073ae89da755d6")
DISCRIMINATOR_CANCEL_TASK = bytes.fromhex("45e486bb8669ee30")

LAMPORTS_PER_SOL = 1_000_000_000


def _u8(value: int) -> bytes:
    return value.to_bytes(1, byteorder="little")


def _u16(value: int) -> bytes:
    return value.to_bytes(2, byteorder="little")


def _u32(value: int) -> bytes:
    return value.to_bytes(4, byteorder="little")


def _u64(value: int) -> bytes:
    return value.to_bytes(8, byteorder="little")


def _i64(value: int) -> bytes:
    return value.to_bytes(8, byteorder="little", signed=True)


def _shortvec_length(value: int) -> bytes:
    assert 0 <= value < 0x4000
    if value < 0x80:
        return _u8(value)
    return _u8((value & 0x7F) | 0x80) + _u8(value >> 7)


def _pubkey(value: str) -> bytes:
    return base58.b58decode(value)


def _deterministic_pubkey(seed: int) -> bytes:
    assert 1 <= seed <= 255
    return bytes([seed]) * 32


def _compiled_instruction(program_id_index: int, accounts: List[int], data: bytes) -> bytes:
    return (
        _u8(program_id_index)
        + _shortvec_length(len(accounts))
        + bytes(accounts)
        + _shortvec_length(len(data))
        + data
    )


def create_task_data(
    reward_amount: int = LAMPORTS_PER_SOL,
    max_workers: int = 1,
    deadline: int = 1_700_000_000,
    task_type: int = 0,
    min_reputation: int = 5,
) -> bytes:
    description = b"test".ljust(64, b"\x00")
    return (
        DISCRIMINATOR_CREATE_TASK
        + _deterministic_pubkey(2)
        + _u64(1)
        + description
        + _u64(reward_amount)
        + _u8(max_workers)
        + _i64(deadline)
        + _u8(task_type)
        + b"\x00"
        + _u16(min_reputation)
        + b"\x00"
    )


def set_task_job_spec_data(uri: bytes = b"ipfs") -> bytes:
    return DISCRIMINATOR_SET_TASK_JOB_SPEC + _deterministic_pubkey(3) + _u32(len(uri)) + uri


def configure_task_validation_data(review_window_secs: int = 3600) -> bytes:
    return (
        DISCRIMINATOR_CONFIGURE_TASK_VALIDATION
        + _u8(1)
        + _i64(review_window_secs)
        + _u8(0)
        + b"\x00"
    )


def submit_task_result_data(include_result_data: bool = True) -> bytes:
    data = DISCRIMINATOR_SUBMIT_TASK_RESULT + _deterministic_pubkey(4)
    if not include_result_data:
        return data + b"\x00"
    return data + b"\x01" + _deterministic_pubkey(5) + _deterministic_pubkey(6)


def reject_task_result_data() -> bytes:
    return DISCRIMINATOR_REJECT_TASK_RESULT + _deterministic_pubkey(6)


def attach_job_spec_message(signer_pubkey: bytes) -> bytes:
    system_index = 7
    agenc_index = 8
    accounts = [2, 1, 3, 4, 5, 0, system_index]
    return _single_agenc_message(signer_pubkey, accounts, set_task_job_spec_data(), agenc_index)


def claim_task_message(signer_pubkey: bytes) -> bytes:
    system_index = 7
    agenc_index = 8
    accounts = [1, 2, 3, 4, 5, 0, system_index]
    return _single_agenc_message(
        signer_pubkey,
        accounts,
        DISCRIMINATOR_CLAIM_TASK_WITH_JOB_SPEC,
        agenc_index,
    )


def submit_task_result_message(signer_pubkey: bytes) -> bytes:
    system_index = 8
    agenc_index = 9
    accounts = [1, 2, 3, 4, 5, 6, 0, system_index]
    return _single_agenc_message(signer_pubkey, accounts, submit_task_result_data(), agenc_index)


def accept_task_result_message(signer_pubkey: bytes) -> bytes:
    system_index = 11
    agenc_index = 12
    accounts = [1, 2, 3, 4, 5, 6, 7, 8, 0, 9, system_index]
    return _single_agenc_message(
        signer_pubkey,
        accounts,
        DISCRIMINATOR_ACCEPT_TASK_RESULT,
        agenc_index,
    )


def reject_task_result_message(signer_pubkey: bytes) -> bytes:
    agenc_index = 8
    accounts = [1, 2, 3, 4, 5, 6, 0, 7]
    return _single_agenc_message(
        signer_pubkey,
        accounts,
        reject_task_result_data(),
        agenc_index,
        include_system_program=False,
    )


def cancel_task_message(signer_pubkey: bytes) -> bytes:
    system_index = 10
    agenc_index = 11
    accounts = [1, 2, 0, 3, system_index, 4, 5, 6, 7, 8, 9]
    return _single_agenc_message(signer_pubkey, accounts, DISCRIMINATOR_CANCEL_TASK, agenc_index)


def create_review_message(signer_pubkey: bytes, include_compute_budget: bool = True) -> bytes:
    account_keys = [
        signer_pubkey,
        _deterministic_pubkey(1),  # task
        _deterministic_pubkey(2),  # escrow / protocol config seed stand-in
        _deterministic_pubkey(3),  # protocol config
        _deterministic_pubkey(4),  # creator agent
        _deterministic_pubkey(5),  # authority rate limit / attestor config
        _deterministic_pubkey(6),  # validation config
        _deterministic_pubkey(7),  # spare account
        _pubkey(PROGRAM_ID_COMPUTE_BUDGET),
        _pubkey(PROGRAM_ID_SYSTEM),
        _pubkey(PROGRAM_ID_AGENC_ARTIFACT),
    ]
    compute_budget_index = 8
    system_index = 9
    agenc_index = 10

    instructions = []
    if include_compute_budget:
        instructions.append(
            _compiled_instruction(compute_budget_index, [], bytes([2, 0x40, 0x0d, 0x03, 0x00]))
        )

    instructions.append(
        _compiled_instruction(
            agenc_index,
            [1, 2, 3, 4, 5, 0, 0, system_index],
            create_task_data(),
        )
    )
    instructions.append(
        _compiled_instruction(
            agenc_index,
            [1, 6, 5, 3, 0, system_index],
            configure_task_validation_data(),
        )
    )

    return _message(account_keys, instructions, num_readonly_unsigned_accounts=3)


def lone_create_task_message(signer_pubkey: bytes) -> bytes:
    account_keys = [
        signer_pubkey,
        _deterministic_pubkey(1),
        _deterministic_pubkey(2),
        _deterministic_pubkey(3),
        _deterministic_pubkey(4),
        _deterministic_pubkey(5),
        _pubkey(PROGRAM_ID_SYSTEM),
        _pubkey(PROGRAM_ID_AGENC_ARTIFACT),
    ]
    instructions = [
        _compiled_instruction(7, [1, 2, 3, 4, 5, 0, 0, 6], create_task_data()),
    ]
    return _message(account_keys, instructions, num_readonly_unsigned_accounts=2)


def _single_agenc_message(
    signer_pubkey: bytes,
    accounts: List[int],
    data: bytes,
    agenc_index: int,
    include_system_program: bool = True,
) -> bytes:
    account_keys = [signer_pubkey]
    if include_system_program:
        account_keys += [_deterministic_pubkey(seed) for seed in range(1, agenc_index - 1)]
        account_keys.append(_pubkey(PROGRAM_ID_SYSTEM))
    else:
        account_keys += [_deterministic_pubkey(seed) for seed in range(1, agenc_index)]
    account_keys.append(_pubkey(PROGRAM_ID_AGENC_ARTIFACT))

    instructions = [_compiled_instruction(agenc_index, accounts, data)]
    readonly_unsigned_accounts = 2 if include_system_program else 1
    return _message(account_keys, instructions, readonly_unsigned_accounts)


def _message(
    account_keys: List[bytes],
    instructions: List[bytes],
    num_readonly_unsigned_accounts: int,
) -> bytes:
    return (
        _u8(1)
        + _u8(0)
        + _u8(num_readonly_unsigned_accounts)
        + _shortvec_length(len(account_keys))
        + b"".join(account_keys)
        + FAKE_RECENT_BLOCKHASH_BYTES
        + _shortvec_length(len(instructions))
        + b"".join(instructions)
    )
