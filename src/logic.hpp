#ifndef LOGIC_HPP
#define LOGIC_HPP

#include <string>

std::string check_bal(const int acctid);

void deposit(const int acctid, const int amt);

void withdraw(const int acctid, const int amt);

void terminate_acct(const int acctid);

void create_acct(const std::string fname, const std::string lname, const std::string password); 

bool login(const std::string email, const std::string pw);

void wire(const int srcacct, const int dstacct, const int amt);

#endif