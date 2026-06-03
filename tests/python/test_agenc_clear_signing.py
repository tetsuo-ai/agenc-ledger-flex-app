from application_client import solana_utils as SOL
from application_client.agenc_cmd_builder import (
    accept_task_result_message,
    attach_job_spec_message,
    cancel_task_message,
    claim_task_message,
    create_review_message,
    lone_create_task_message,
    reject_task_result_message,
    submit_task_result_message,
)
from application_client.solana_cmd_builder import verify_signature


class TestAgencClearSigning:
    def _approve_and_verify(self, sol, scenario_navigator, root_pytest_dir, message):
        with sol.send_async_sign_message(SOL.SOL_PACKED_DERIVATION_PATH, message):
            scenario_navigator.review_approve(path=root_pytest_dir)

        signature = sol.get_async_response().data
        signer_pubkey = sol.get_public_key(SOL.SOL_PACKED_DERIVATION_PATH)
        verify_signature(signer_pubkey, message, signature)

    def test_agenc_create_task_with_review_ok(self, sol, scenario_navigator, root_pytest_dir):
        signer_pubkey = sol.get_public_key(SOL.SOL_PACKED_DERIVATION_PATH)
        message = create_review_message(signer_pubkey)
        self._approve_and_verify(sol, scenario_navigator, root_pytest_dir, message)

    def test_agenc_attach_job_spec_ok(self, sol, scenario_navigator, root_pytest_dir):
        signer_pubkey = sol.get_public_key(SOL.SOL_PACKED_DERIVATION_PATH)
        message = attach_job_spec_message(signer_pubkey)
        self._approve_and_verify(sol, scenario_navigator, root_pytest_dir, message)

    def test_agenc_claim_task_ok(self, sol, scenario_navigator, root_pytest_dir):
        signer_pubkey = sol.get_public_key(SOL.SOL_PACKED_DERIVATION_PATH)
        message = claim_task_message(signer_pubkey)
        self._approve_and_verify(sol, scenario_navigator, root_pytest_dir, message)

    def test_agenc_submit_task_result_ok(self, sol, scenario_navigator, root_pytest_dir):
        signer_pubkey = sol.get_public_key(SOL.SOL_PACKED_DERIVATION_PATH)
        message = submit_task_result_message(signer_pubkey)
        self._approve_and_verify(sol, scenario_navigator, root_pytest_dir, message)

    def test_agenc_accept_task_result_ok(self, sol, scenario_navigator, root_pytest_dir):
        signer_pubkey = sol.get_public_key(SOL.SOL_PACKED_DERIVATION_PATH)
        message = accept_task_result_message(signer_pubkey)
        self._approve_and_verify(sol, scenario_navigator, root_pytest_dir, message)

    def test_agenc_reject_task_result_ok(self, sol, scenario_navigator, root_pytest_dir):
        signer_pubkey = sol.get_public_key(SOL.SOL_PACKED_DERIVATION_PATH)
        message = reject_task_result_message(signer_pubkey)
        self._approve_and_verify(sol, scenario_navigator, root_pytest_dir, message)

    def test_agenc_cancel_task_ok(self, sol, scenario_navigator, root_pytest_dir):
        signer_pubkey = sol.get_public_key(SOL.SOL_PACKED_DERIVATION_PATH)
        message = cancel_task_message(signer_pubkey)
        self._approve_and_verify(sol, scenario_navigator, root_pytest_dir, message)

    def test_agenc_lone_create_task_ok(self, sol, scenario_navigator, root_pytest_dir):
        # A lone create_task (no paired review config) now clear-signs standalone
        # rather than falling back to blind signing.
        signer_pubkey = sol.get_public_key(SOL.SOL_PACKED_DERIVATION_PATH)
        message = lone_create_task_message(signer_pubkey)
        self._approve_and_verify(sol, scenario_navigator, root_pytest_dir, message)
