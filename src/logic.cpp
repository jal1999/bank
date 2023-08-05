#include "logic.hpp"
#include <stdexcept>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/collection.hpp>
#include <random>
#include <iostream>
#include <Poco/Net/MailMessage.h>
#include <Poco/Net/SMTPClientSession.h>



using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

mongocxx::instance instance{};
mongocxx::uri uri("mongodb://localhost:27017");
mongocxx::client client(uri);
mongocxx::database db = client["bank"];
mongocxx::collection accts = db["accounts"];
mongocxx::collection transactions = db["transactions"];


/**
 * Returns the balance of a given user.
 * 
 * @param acctid The account id of the user.
 */
std::string check_bal(const int acctid) {
    auto acct = accts.find_one(make_document(kvp("account_id", acctid)));
    if (!acct) {
        throw std::invalid_argument("This user does not exist.");
    }
    auto doc = acct->view();
    auto bal = doc["balance"];
    std::cout << bal.get_int32().value << std::endl;
    return std::to_string(bal.get_int32().value);
}


/**
 * Deposits a given amount of money into a user's account.
 *
 * @param acctid The account ID of the user.
 * @param amt The amount of money to deposit into the user's account.
 */
void deposit(const int acctid, const int amt) {
    auto acct = accts.find_one(make_document(kvp("account_id", acctid)));
    if (!acct) {
        throw std::invalid_argument("This user does not exist.");
    } else if (amt <= 0) {
        throw std::invalid_argument("Amount deposited must be positive.");
    }
    auto doc = acct->view();
    auto bal = doc["balance"];
    int currbal = bal.get_int32().value + amt;
    std::cout << currbal << std::endl;

    accts.update_one(make_document(kvp("account_id", acctid)),
        make_document(kvp("$set", make_document(kvp("balance", currbal)))));
    transactions.insert_one(make_document(
        kvp("type", "deposit"),
        kvp("amount", amt),
        kvp("acct_id", acctid)));
}


/**
 * Withdraws the given amount of money from ther user's account.
 *
 * @param acctid The account ID of the user.
 * @param amt The amount of money to withdraw from the user's account.
 */
void withdraw(const int acctid, const int amt) {
    auto acct = accts.find_one(make_document(kvp("account_id", acctid)));
    if (!acct) {
        throw std::invalid_argument("This user does not exist.");
    } else if (amt <= 0) {
        throw std::invalid_argument("Amount deposited must be positive.");
    }
    auto doc = acct->view();
    auto bal = doc["balance"];
    if (amt > bal.get_int32().value) {
        throw std::invalid_argument("Amount to withdraw must be less than the current balance.");
    }
    int newbal = bal.get_int32().value - amt;
    accts.update_one(make_document(kvp("account_id", acctid)),
        make_document(kvp("$set", make_document(kvp("balance", newbal)))));
    transactions.insert_one(make_document(
        kvp("type", "withdrawl"),
        kvp("amount", amt),
        kvp("account_id", acctid)));
}


/**
 * Marks an account as terminated, which will not allow the user to log in, or perform
 * any actions with their account.
 *
 * @param acctid The account ID of the user.
 */
void terminate_acct(const int acctid) {
    auto acct = accts.find_one(make_document(kvp("account_id", acctid)));
    if (!acct) {
       throw std::invalid_argument("This user does not exist.");
    } 
    auto doc = acct->view();
    auto isterminated = doc["is_terminated"];
    if (isterminated.get_bool()) {
        throw std::invalid_argument("This account has already been terminated.");
    }
    accts.update_one(make_document(
        kvp("account_id", acctid)),
        make_document(kvp("$set", make_document(kvp("is_terminated", true)))));
}


/**
 * Creates an account, and stores the user's first name, last name, (hashed) password, email, and balance
 * (with an initial value of 0) in the database.
 * 
 * NOTE: This method of hashing the password is not safe for an enterprise-level application as it
 * simply uses std::hash as its hashing algorithm, which is not safe for passwords. It is simply being
 * used as this is a personal project and not an enterprise level application.
 * 
 * @param fname The first name of the user.
 * @param lname The last name of the user.
 * @param password The raw password of the user.
 */
void create_acct(const std::string fname, const std::string lname, const std::string password, const std::string email) {
    std::hash<std::string> hasher;
    int count = 0;
    auto alldocs = accts.find({});
    for (const auto doc : alldocs)
        ++count;
    accts.insert_one(make_document(
        kvp("balance", 0), 
        kvp("email", email),
        kvp("first_name", fname), 
        kvp("last_name", lname),
        kvp("account_id", count),
        kvp("password", std::to_string(hasher(password)))));
}


/**
 * Compares the given password with the stored hashed password of the user
 * with the given email.
 *
 * @param email The email of the user.
 * @param pw The entered password of the user.
 */
bool login(const std::string email, const std::string pw) {
    std::hash<std::string> hasher;
    auto acct = accts.find_one(make_document(kvp("email", email)));
    if (!acct) {
        throw std::invalid_argument("This user does not exist");
    } 
    auto doc = acct->view();
    auto isterminated = doc["is_terminated"];
    if (isterminated.get_bool()) {
        throw std::invalid_argument("This account has been terminated");
    }
    std::string hashedpass = std::string(doc["password"].get_string().value);
    return hashedpass == std::to_string(hasher(pw));
}

/**
 * Sends an email alerting the account holder that they are negative in their account. 
 *
 * @param acctid The ID of the overdrawn account.
 */
void overdrawn(const int acctid) {
    auto acct = accts.find_one(make_document(kvp("account_id", acctid)));
    if (!acct) {
        throw std::invalid_argument("This user does not exist");
    }
    auto doc = acct->view();
    auto isterminated = doc["is_teriminated"];
    if (isterminated.get_bool()) {
        throw std::invalid_argument("This account has been terminated");
    }
    Poco::Net::MailMessage msg;
    msg.addRecipient(Poco::Net::MailRecipient( Poco::Net::MailRecipient::PRIMARY_RECIPIENT, doc["email"].get_string().value.to_string(), doc["first_name"].get_string().value.to_string()));
    msg.setSender("lafarrbanking@gmail.com");
    msg.setSubject("Overdraw");
    msg.setContent(".");
    Poco::Net::SMTPClientSession smtp("smtp.gmail.com");
    smtp.login(Poco::Net::SMTPClientSession::AUTH_LOGIN, "lafarrbanking@gmail.com", "this is not the password");
    smtp.sendMessage(msg);
    smtp.close();
}

/**
 * Wires the given amount from the source account to the destination account.
 *
 * @param srcacct The account the money is being wired from
 * @param dstacct The account the money is being wired to
 * @param amt The amount to be wired
 */
void wire(const int srcacct, const int dstacct, const int amt) {
    auto src = accts.find_one(make_document(kvp("account_id", srcacct)));
    auto dst = accts.find_one(make_document(kvp("account_id", dstacct)));
    if (!src || !dst) {
        throw std::invalid_argument("Invalid account ID.");
    }
    auto srcdoc = src->view(), dstdoc = dst->view();
    if (srcdoc["is_terminated"].get_bool() || dstdoc["is_terminated"].get_bool()) {
        throw std::invalid_argument("Account has been terminated.");
    }
    int x = srcdoc["balance"].get_int32().value - amt, y = dstdoc["balance"].get_int32().value + amt;
    accts.update_one(make_document(
        kvp("account_id", srcacct)),
        make_document(kvp("$set", make_document(kvp("balance", x)))));
    accts.update_one(make_document(
        kvp("account_id", dstacct)),
        make_document(kvp("$set", make_document(kvp("balance", y)))));
    if (x < 0) {
        overdrawn(srcacct);
    }    
}
