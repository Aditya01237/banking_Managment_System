#include "common.h"
#include "data_access.h"
#include <assert.h>

int main(void)
{
    Money value = 0;
    assert(parse_money("1250.75", &value) == 1);
    assert(value == 125075);
    assert(parse_money("0.1", &value) == 1 && value == 10);
    assert(parse_money("10.123", &value) == 0);
    assert(parse_money("12abc", &value) == 0);

    char hash[PASSWORD_HASH_MAX];
    assert(hash_password("strong-pass-123", hash) == 0);
    assert(strcmp(hash, "strong-pass-123") != 0);
    assert(verify_password(hash, "strong-pass-123") == 1);
    assert(verify_password(hash, "wrong-pass") == 0);

    assert(initialize_indexes() == 0);
    assert(find_user_record(2) >= 0);
    assert(find_account_record_by_id(1) >= 0);
    assert(find_account_record_by_number("SB10001") >= 0);

    Account a = getAccount(1);
    Account b = getAccount(2);
    assert(a.balance == 500000);
    assert(b.balance == 2500000);

    puts("test_core: PASS");
    return 0;
}
