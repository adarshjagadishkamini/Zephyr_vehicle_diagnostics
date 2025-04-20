#include "redundancy.h"

bool verify_redundant_value(struct redundant_value *val) {
    // Voting mechanism for triple redundancy
    if (fabs(val->value1 - val->value2) < TOLERANCE &&
        fabs(val->value2 - val->value3) < TOLERANCE) {
        return true;
    }
    
    // Determine which value is faulty using majority voting
    if (fabs(val->value1 - val->value2) < TOLERANCE) {
        val->value3 = (val->value1 + val->value2) / 2;
    } else if (fabs(val->value2 - val->value3) < TOLERANCE) {
        val->value1 = (val->value2 + val->value3) / 2;
    } else if (fabs(val->value1 - val->value3) < TOLERANCE) {
        val->value2 = (val->value1 + val->value3) / 2;
    } else {
        return false;
    }
    
    return true;
}
