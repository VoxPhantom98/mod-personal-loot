/* Voxville persistence tests, GNU AGPL v3. */
#include "CheckedTransaction.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void Require(bool value, char const* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    struct FakeDatabase
    {
        bool begin{true};
        bool statements{true};
        bool commitAcknowledged{true};
        bool rollbackAcknowledged{true};
        bool committedOnServer{false};
        uint32_t statementCalls{0};
        std::vector<std::string> commands;

        bool Control(std::string_view command)
        {
            commands.emplace_back(command);
            if (command == "START TRANSACTION")
                return begin;
            if (command == "COMMIT")
            {
                committedOnServer = true;
                return commitAcknowledged;
            }
            return rollbackAcknowledged;
        }

        TransactionOutcome Run()
        {
            return RunCheckedTransaction([this](std::string_view command) { return Control(command); }, [this]()
            {
                ++statementCalls;
                return statements;
            });
        }
    };
}

int main()
{
    FakeDatabase success;
    Require(success.Run() == TransactionOutcome::Committed, "Successful commit must be confirmed");
    Require(success.statementCalls == 1, "Statements must execute once");

    FakeDatabase beginFailure;
    beginFailure.begin = false;
    Require(beginFailure.Run() == TransactionOutcome::Unknown, "Failed START must not appear successful");
    Require(beginFailure.statementCalls == 0, "No statements after failed START");

    FakeDatabase duplicateClaim;
    duplicateClaim.statements = false;
    Require(duplicateClaim.Run() == TransactionOutcome::RolledBack, "Failed claim must roll back");
    Require(!duplicateClaim.committedOnServer, "Failed claim cannot commit inventory changes");
    Require(duplicateClaim.statementCalls == 1, "Failed statements cannot be replayed");

    FakeDatabase lostRollback;
    lostRollback.statements = false;
    lostRollback.rollbackAcknowledged = false;
    Require(lostRollback.Run() == TransactionOutcome::Unknown, "Lost rollback acknowledgement requires reconciliation");

    FakeDatabase lostCommit;
    lostCommit.commitAcknowledged = false;
    Require(lostCommit.Run() == TransactionOutcome::Unknown, "Lost COMMIT must never be called rolled back");
    Require(lostCommit.committedOnServer, "Test must model a committed but unacknowledged award");
    Require(lostCommit.statementCalls == 1, "Unknown commit cannot replay an award");
    Require(lostCommit.commands == std::vector<std::string>{"START TRANSACTION", "COMMIT", "ROLLBACK"},
        "Failed COMMIT must attempt rollback without repeating START or COMMIT");

    std::cout << "Checked transaction outcome tests passed\n";
}
