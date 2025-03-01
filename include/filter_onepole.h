#pragma once

#include "defjams.h"
#include "filter.h"

struct OnePole : public Filter
{

    OnePole();
    ~OnePole() = default;

    double m_alpha{1.0};
    double m_beta{0.0};
    double m_z1{0.0};
    double m_gamma{1.0};
    double m_delta{0.0};
    double m_epsilon{0.0};
    double m_a0{1.0};
    double m_feedback{0.0};

    double DoFilter(double xn);
    void Update();
    void SetFeedback(double fb);
    double GetFeedbackOutput();
    void Reset();
    void SetFilterType(filter_type ftype); // ENUM in filter.h
};
