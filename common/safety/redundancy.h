#ifndef REDUNDANCY_H
#define REDUNDANCY_H

struct redundant_value {
    float value1;
    float value2;
    float value3;
    uint32_t timestamp;
};

bool verify_redundant_value(struct redundant_value *val);
void update_redundant_value(struct redundant_value *val, float new_value);
float get_validated_value(struct redundant_value *val);

#endif /* REDUNDANCY_H */
