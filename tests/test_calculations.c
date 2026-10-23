#include <get_goals_rubi_csv.h>

/**
 * @brief Run assertions for pace conversion, smoothing, and heart-rate matching.
 * @return 0 if all assertions pass; aborts on the first failed assertion.
 */
int main(void) {
    Pace pace;
    assert(validate_str_time("5:30.5", &pace) == 1);
    assert(fabsf(pace_to_float(&pace) - (5 + 30.5f / 60)) < 1e-6);
    float_to_Pace(&pace, 5 + 59.999f / 60);
    assert(pace.min == 6 && pace.sec == 0 && pace.dec == 0);

    Sample head = {0};
    float values[] = {100, 110, 140, 120, 130, 150, 160};
    Sample nodes[7] = {0};
    head.next = nodes;
    for (int i = 0; i < 7; i++) {
        nodes[i].hr = values[i];
        nodes[i].prev = i ? &nodes[i - 1] : &head;
        nodes[i].next = i < 6 ? &nodes[i + 1] : NULL;
    }
    float window[] = {0.25f, 0.5f, 0.25f};
    int len = 3;
    assert(lowPassHR(&head, window, &len) == 0);
    for (int i = 1; i < 6; i++)
        assert(fabsf(nodes[i].hr - (values[i - 1] + 2 * values[i] + values[i + 1]) / 4) < 1e-5);
    assert(nodes[0].hr == values[0] && nodes[6].hr == values[6]);
    int target = 145;
    float average;
    head.next = &nodes[0];
    nodes[0].next = NULL;
    nodes[0].hr = 146.5f;
    nodes[0].pace_min = 5;
    assert(average_pace_at_HR(&head, &target, 1, &average) == 0 && average == 0);
    nodes[0].hr = 145.5f;
    assert(average_pace_at_HR(&head, &target, 1, &average) == 1 && average == 5);
    return 0;
}
